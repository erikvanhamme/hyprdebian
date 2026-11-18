# Tests for hd-agent, the agent of the package hd-agent: parsing what the system tools print for
# the report, the settings, sending a report, and the jobs: accepting and refusing them,
# cancelling, expiry, running them, and the jobs in the report, also against the hd-dashboard of
# this repository. The jobs are signed with test keys made in a temporary directory, and the
# store of the jobs is a temporary directory too; the transient units are faked.

import contextlib
import gzip
import http.server
import importlib.machinery
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import unittest.mock

PACKAGES_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HD_AGENT = os.path.join(PACKAGES_DIR, 'hd-agent', 'hd-agent')


def load_hd_agent():
    # hd-agent has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_agent', HD_AGENT)
    spec = importlib.util.spec_from_loader('hd_agent', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


agent = load_hd_agent()
job = agent.job


def load_hd_dashboard():
    loader = importlib.machinery.SourceFileLoader('hd_dashboard', os.path.join(PACKAGES_DIR, 'hd-dashboard', 'hd-dashboard'))
    spec = importlib.util.spec_from_loader('hd_dashboard', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


class TestParsing(unittest.TestCase):
    def test_version_and_suite(self):
        with tempfile.TemporaryDirectory() as tmp:
            version = os.path.join(tmp, "version.json")
            with open(version, "w") as f:
                json.dump({"major": 1, "minor": 1}, f)

            self.assertEqual(agent.hyprdebian_version(version), "1.1")
            self.assertEqual(agent.hyprdebian_version(os.path.join(tmp, "missing")), "")

        sources = "Types: deb\nURIs: http://repo.example/\nSuites: trixie\nComponents: main\n"
        self.assertEqual(agent.parse_suite(sources), "trixie")
        self.assertEqual(agent.parse_suite(None), "")

    def test_meminfo_gives_used_memory_in_bytes(self):
        memory = agent.parse_meminfo("MemTotal: 1000 kB\nMemAvailable: 400 kB\n"
                                      "SwapTotal: 200 kB\nSwapFree: 150 kB\n")

        self.assertEqual(memory, {"total": 1024000, "used": 614400})

    def test_pools_get_their_last_completed_scrub(self):
        listing = "bpool\tONLINE\t1000\t25%\nrpool\tDEGRADED\t2000\t90\n"
        status = ("  pool: bpool\n state: ONLINE\n"
                  "  scan: scrub repaired 1.50M in 00:12:01 with 2 errors on Sun Oct 11 00:36:02 2026\n"
                  "  pool: rpool\n state: DEGRADED\n  scan: resilvered 1.2G in 00:10:00 with 0 errors on Sun Oct 11 00:30:00 2026\n")

        self.assertEqual(agent.parse_pools(listing, status), [
            {"name": "bpool", "health": "ONLINE", "size": 1000, "capacity": 25,
             "scrub": {"repaired": 1572864, "errors": 2, "time": int(time.mktime((2026, 10, 11, 0, 36, 2, 0, 0, -1)))}},
            {"name": "rpool", "health": "DEGRADED", "size": 2000, "capacity": 90, "scrub": None},
        ])

    def test_only_completed_scrubs_count(self):
        for line in ["scrub in progress since Sun Oct 11 00:24:01 2026",
                     "scrub paused since Sun Oct 11 00:24:01 2026",
                     "scrub canceled on Sun Oct 11 00:30:00 2026",
                     "none requested"]:
            with self.subTest(line=line):
                self.assertEqual(agent.parse_scrubs(f"  pool: rpool\n  scan: {line}\n"), {})

    def test_sizes_of_zpool(self):
        self.assertEqual([agent.parse_size(text) for text in ["0B", "512B", "12K", "1.50M", "2048", "x"]],
                         [0, 512, 12288, 1572864, 2048, None])

    def test_no_zpool_gives_no_pools(self):
        self.assertEqual(agent.parse_pools(None, None), [])

    def test_failed_units(self):
        text = "zfs-zed.service loaded failed failed ZFS Event Daemon\nfoo.mount loaded failed failed Foo\n"

        self.assertEqual(agent.parse_failed_units(text), ["zfs-zed.service", "foo.mount"])
        self.assertEqual(agent.parse_failed_units(""), [])

    def test_history_finds_the_last_upgrade(self):
        text = ("Start-Date: 2026-10-01  10:00:00\nCommandline: apt full-upgrade -y\nEnd-Date: 2026-10-01  10:05:00\n\n"
                "Start-Date: 2026-10-02  11:00:00\nCommandline: apt install foo\nEnd-Date: 2026-10-02  11:01:00\n")

        self.assertEqual(agent.parse_history(text), int(time.mktime((2026, 10, 1, 10, 5, 0, 0, 0, -1))))
        self.assertIsNone(agent.parse_history("Commandline: apt install foo\nEnd-Date: 2026-10-02  11:01:00\n"))

    def test_upgrade_is_found_in_a_rotated_history_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "history.log")
            with open(path, "w") as f:
                f.write("Commandline: apt install foo\nEnd-Date: 2026-10-02  11:01:00\n")
            for number, day in [(1, 20), (2, 10)]:
                with gzip.open(f"{path}.{number}.gz", "wt") as f:
                    f.write(f"Commandline: apt upgrade\nEnd-Date: 2026-09-{day}  09:00:00\n")

            logs = agent.read_history(path)

        self.assertEqual(agent.last_upgrade(logs),
                         int(time.mktime((2026, 9, 20, 9, 0, 0, 0, 0, -1))))

    def test_newest_kernel_is_modified_last(self):
        # Two builds of hd-kernel of the same linux: the hash of the newer one sorts first.
        with tempfile.TemporaryDirectory() as tmp:
            for when, version in [(1000, "7.2.13+deb14-amd64"), (2000, "7.2.12+deb14-hdf00000-amd64"),
                                  (3000, "7.2.12+deb14-hd000000-amd64")]:
                path = os.path.join(tmp, "vmlinuz-" + version)
                open(path, "w").close()
                os.utime(path, (when, when))

            self.assertEqual(agent.newest_kernel(tmp), "7.2.12+deb14-hd000000-amd64")

        with tempfile.TemporaryDirectory() as tmp:
            self.assertIsNone(agent.newest_kernel(tmp))

    def test_only_snapshots_of_hd_up_count(self):
        text = ("rpool/hyprdebian@install\t100\nrpool/hyprdebian@hd-up-2026-10-01_100000\t200\n"
                "rpool/hyprdebian@hd-up-2026-10-02_100000\t300\nrpool/hyprdebian@manual\t400\n")

        self.assertEqual(agent.parse_snapshots(text), {"name": "hd-up-2026-10-02_100000", "time": 300})
        self.assertIsNone(agent.parse_snapshots(None))

    def test_systemd_timestamps(self):
        properties = agent.parse_properties("LastTriggerUSec=@1792114200\nNextElapseUSecRealtime=\n")

        self.assertEqual(agent.unix_time(properties["LastTriggerUSec"]), 1792114200)
        self.assertIsNone(agent.unix_time(properties["NextElapseUSecRealtime"]))

    def test_arrays_use_the_name_in_dev_md(self):
        with tempfile.TemporaryDirectory() as tmp:
            md = os.path.join(tmp, "block", "md127", "md")
            os.makedirs(md)
            with open(os.path.join(md, "degraded"), "w") as f:
                f.write("1\n")
            with open(os.path.join(md, "array_state"), "w") as f:
                f.write("clean\n")
            os.makedirs(os.path.join(tmp, "dev", "md"))
            os.symlink(os.path.join(tmp, "block", "md127"), os.path.join(tmp, "dev", "md", "swap"))

            arrays = agent.collect_arrays(os.path.join(tmp, "block"), os.path.join(tmp, "dev", "md"))

        self.assertEqual(arrays, [{"name": "swap", "state": "clean", "degraded": 1}])

    def test_hardening(self):
        output = '{"ssh": "on", "sysctl": "off", "modules": "changed", "zfs": "on", "ufw": "unknown"}\n'
        self.assertEqual(agent.parse_hardening(output),
                         {"ssh": "on", "sysctl": "off", "modules": "changed", "zfs": "on", "ufw": "unknown"})

    def test_no_hardening(self):
        for output in (None, "", "not json", "[]", '"on"'):
            with self.subTest(output=output):
                self.assertIsNone(agent.parse_hardening(output))

    def test_hardening_leaves_out_what_is_not_a_state(self):
        self.assertEqual(agent.parse_hardening('{"ssh": "on", "zfs": 1, "ufw": null}'), {"ssh": "on"})

    def test_secure_boot_state(self):
        cases = [
            ({"SecureBoot": b"\x01", "SetupMode": b"\x00"}, "on"),
            ({"SecureBoot": b"\x00", "SetupMode": b"\x00"}, "off"),
            ({"SecureBoot": b"\x00", "SetupMode": b"\x01"}, "setup"),
            ({}, "unsupported"),
        ]

        for variables, state in cases:
            with self.subTest(state=state), tempfile.TemporaryDirectory() as tmp:
                for name, value in variables.items():
                    with open(os.path.join(tmp, f"{name}-{agent.GLOBAL_GUID}"), "wb") as f:
                        f.write(b"\x06\x00\x00\x00" + value)

                self.assertEqual(agent.secure_boot_state(tmp), state)

        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(agent.secure_boot_state(os.path.join(tmp, "missing")), "legacy")


class TestSettings(unittest.TestCase):
    def test_the_settings_override_the_defaults(self):
        with open(os.path.join(PACKAGES_DIR, 'hd-agent', 'defaults.json')) as f:
            defaults = json.load(f)

        self.assertEqual(agent.read_settings(None, defaults), defaults)
        self.assertEqual(defaults["manager_keys"], [])
        settings = agent.read_settings({"hd-agent": {"url": "http://dash.example/"}}, defaults)
        self.assertEqual(agent.dashboard_url(settings), "http://dash.example/")
        self.assertEqual(settings["manager_keys"], [])

    def test_a_wrong_url_is_refused(self):
        for url in ["", "dash.example", "ftp://dash.example/", 5]:
            with self.subTest(url=url):
                self.assertIsNone(agent.dashboard_url(agent.read_settings({"hd-agent": {"url": url}}, {})))

        self.assertIsNone(agent.dashboard_url(agent.read_settings(None, None)))


class TestCollect(unittest.TestCase):
    def test_the_report_of_this_system_is_json(self):
        collected = agent.collect()

        self.assertEqual(collected["format"], agent.FORMAT)
        self.assertTrue(collected["host"])
        self.assertEqual(collected["jobs"], [])
        json.dumps(collected)

    def test_hd_dashboard_accepts_the_report(self):
        dashboard = load_hd_dashboard()

        collected = agent.collect()
        db = dashboard.connect(":memory:")
        reply = dashboard.accept_report(db, json.dumps(collected).encode(), "Bearer " + "a" * 64,
                                        "127.0.0.1", int(time.time()), 30)

        self.assertEqual(reply, {"jobs": [], "open": []})
        self.assertEqual(db.execute("SELECT name FROM hosts").fetchone()[0], collected["host"])
        # The dashboard is updated before the systems, so it may know a newer format.
        self.assertLessEqual(agent.FORMAT, dashboard.FORMAT)


class TestSend(unittest.TestCase):
    def test_report_is_posted_with_the_token(self):
        received = {}

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                received["path"] = self.path
                received["authorization"] = self.headers["Authorization"]
                received["body"] = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                self.send_response(200)
                self.end_headers()

            def log_message(self, *args):
                pass

        server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.handle_request)
        thread.start()

        reply = agent.send({"host": "box"}, f"http://127.0.0.1:{server.server_port}/", "abc")

        thread.join()
        server.server_close()
        self.assertEqual(received, {"path": "/report", "authorization": "Bearer abc", "body": {"host": "box"}})
        self.assertEqual(reply, b"")



KEYS = tempfile.TemporaryDirectory()


def make_key(name):
    """Makes an Ed25519 key without a passphrase. Returns the path of the private key and the
    public key."""
    path = os.path.join(KEYS.name, name)
    subprocess.run([job.SSH_KEYGEN, "-q", "-t", "ed25519", "-N", "", "-C", name, "-f", path], check=True)
    with open(path + ".pub") as f:
        return path, f.read().strip()


MANAGER, MANAGER_KEY = make_key("manager")
OTHER, OTHER_KEY = make_key("other")
HOST = "box"
NOW = int(time.time())


def signed_jobs(*jobs, key=MANAGER):
    """Signs jobs given as (sequence, not_before, expires), for HOST and the command upgrade."""
    return job.sign_all([{"sequence": sequence, "host": HOST, "command": "upgrade",
                          "not_before": not_before, "expires": expires}
                         for sequence, not_before, expires in jobs], key)


class FakeUnits:
    """The transient units of the jobs: the times of the waiting timers, the jobs started at once,
    and the running services."""

    def __init__(self):
        self.timers = {}
        self.started = []
        self.running = set()

    def active(self, sequence):
        return sequence in self.timers or sequence in self.running

    def start(self, sequence, command, at):
        if at is None:
            self.started.append(sequence)
        else:
            self.timers[sequence] = at

    def stop(self, sequence):
        self.timers.pop(sequence, None)


class Jobs(unittest.TestCase):
    """A store of jobs in a temporary directory, and fake units."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.store = agent.Store(tmp.name)
        self.units = FakeUnits()
        self.log = io.StringIO()
        self.enterContext(contextlib.redirect_stdout(self.log))
        self.enterContext(contextlib.redirect_stderr(self.log))

    def take(self, jobs, open=None, manager_keys=(MANAGER_KEY,), now=NOW):
        """Takes a reply with jobs; open has the sequence numbers of the jobs, unless it is given."""
        if open is None:
            open = [signed["job"]["sequence"] for signed in jobs]
        with self.store.lock():
            return agent.take_reply(self.store, self.units, {"jobs": jobs, "open": open},
                                    list(manager_keys), HOST, now)

    def states(self):
        return {sequence: entry["state"] for sequence, entry in self.store.jobs().items()}

    def run_job(self, sequence, command=("true",), manager_keys=(MANAGER_KEY,)):
        with unittest.mock.patch.dict(agent.COMMANDS, {"upgrade": list(command)}):
            return agent.run_job(self.store, sequence, list(manager_keys))


class TestAccepting(Jobs):
    def test_a_job_is_accepted_and_gets_its_timer(self):
        jobs = signed_jobs((5, NOW + 3600, NOW + 7200))

        self.assertTrue(self.take(jobs))

        self.assertEqual(self.states(), {5: "scheduled"})
        self.assertEqual(self.store.jobs()[5]["signed"], jobs[0])
        self.assertEqual(self.store.highest(), 5)
        self.assertEqual(self.units.timers, {5: NOW + 3600})
        self.assertIn("Accepted job 5", self.log.getvalue())

    def test_the_commands_are_those_of_the_jobs(self):
        self.assertEqual(sorted(agent.COMMANDS), sorted(job.COMMANDS))

    def test_a_job_signed_by_another_key_is_ignored_and_logged(self):
        self.assertTrue(self.take(signed_jobs((5, NOW, NOW + 60), key=OTHER)))

        self.assertEqual(self.states(), {})
        self.assertEqual(self.store.highest(), 0)
        self.assertIn("does not verify", self.log.getvalue())

    def test_a_job_with_a_changed_field_is_ignored(self):
        jobs = signed_jobs((5, NOW, NOW + 60))
        jobs[0]["job"]["expires"] += 3600

        self.take(jobs)

        self.assertEqual(self.states(), {})

    def test_a_job_for_another_host_is_ignored(self):
        jobs = job.sign_all([{"sequence": 5, "host": "other", "command": "upgrade",
                              "not_before": NOW, "expires": NOW + 60}], MANAGER)

        self.take(jobs)

        self.assertEqual(self.states(), {})
        self.assertIn("it is for other", self.log.getvalue())

    def test_a_job_not_above_the_last_accepted_is_ignored(self):
        self.take(signed_jobs((7, NOW + 60, NOW + 120)))

        self.take(signed_jobs((6, NOW + 60, NOW + 120)), open=[7, 6])

        self.assertEqual(self.states(), {7: "scheduled"})
        self.assertEqual(self.store.highest(), 7)
        self.assertIn("not above 7", self.log.getvalue())

    def test_a_job_accepted_before_is_no_error(self):
        jobs = signed_jobs((5, NOW + 60, NOW + 120))
        self.take(jobs)
        self.log.truncate(0)

        self.take(jobs)

        self.assertEqual(self.states(), {5: "scheduled"})
        self.assertEqual(self.log.getvalue(), "")

    def test_without_manager_keys_no_job_is_accepted(self):
        self.assertTrue(self.take(signed_jobs((5, NOW, NOW + 60)), manager_keys=()))

        self.assertEqual(self.states(), {})
        self.assertIn("no manager_keys", self.log.getvalue())

    def test_malformed_manager_keys_accept_no_job(self):
        self.assertFalse(self.take(signed_jobs((5, NOW, NOW + 60)), manager_keys=("not a key",)))

        self.assertEqual(self.states(), {})

    def test_a_malformed_reply_is_an_error(self):
        for reply in (None, [], {"jobs": []}, {"jobs": {}, "open": []}):
            with self.subTest(reply=reply), self.store.lock():
                self.assertFalse(agent.take_reply(self.store, self.units, reply, [MANAGER_KEY], HOST, NOW))


class TestCancelling(Jobs):
    def test_a_newer_job_cancels_a_scheduled_one(self):
        self.take(signed_jobs((5, NOW + 3600, NOW + 7200)))

        self.take(signed_jobs((6, NOW + 600, NOW + 7200)), open=[5, 6])

        self.assertEqual(self.states(), {5: "cancelled", 6: "scheduled"})
        self.assertEqual(self.store.jobs()[5]["ended"], NOW)
        self.assertEqual(self.units.timers, {6: NOW + 600})

    def test_two_jobs_in_one_reply_leave_the_newest(self):
        self.take(signed_jobs((5, NOW + 3600, NOW + 7200), (6, NOW + 600, NOW + 7200)))

        self.assertEqual(self.states(), {5: "cancelled", 6: "scheduled"})

    def test_a_running_job_is_not_cancelled(self):
        self.take(signed_jobs((5, NOW, NOW + 7200)))
        entry = self.store.jobs()[5]
        entry["state"] = agent.RUNNING
        self.store.save(entry)

        with self.store.job_lock(5):
            self.take(signed_jobs((6, NOW + 600, NOW + 7200)), open=[5, 6])

        self.assertEqual(self.states(), {5: "running", 6: "scheduled"})


class TestScheduling(Jobs):
    def schedule(self, now=NOW):
        with self.store.lock():
            agent.schedule(self.store, self.units, now)

    def test_a_job_whose_time_passed_runs_at_once(self):
        self.take(signed_jobs((5, NOW - 600, NOW + 3600)))

        self.assertEqual(self.units.started, [5])
        self.assertEqual(self.units.timers, {})

    def test_the_timers_are_made_again_after_a_reboot(self):
        self.take(signed_jobs((5, NOW + 3600, NOW + 7200)))
        self.units = FakeUnits()

        self.schedule()

        self.assertEqual(self.units.timers, {5: NOW + 3600})

    def test_a_waiting_timer_is_not_made_again(self):
        self.take(signed_jobs((5, NOW + 3600, NOW + 7200)))
        self.units.timers[5] = "the first"

        self.schedule()

        self.assertEqual(self.units.timers, {5: "the first"})

    def test_a_job_passed_while_the_system_was_off_runs_at_once(self):
        self.take(signed_jobs((5, NOW + 3600, NOW + 7200)))
        self.units = FakeUnits()

        self.schedule(NOW + 3700)

        self.assertEqual(self.units.started, [5])

    def test_a_job_whose_expiry_passed_is_expired(self):
        self.take(signed_jobs((5, NOW + 3600, NOW + 7200)))

        self.schedule(NOW + 7300)

        self.assertEqual(self.states(), {5: "expired"})
        self.assertEqual(self.store.jobs()[5]["ended"], NOW + 7300)
        self.assertEqual(self.units.timers, {})

    def test_a_job_that_expired_before_it_was_accepted_is_expired(self):
        self.take(signed_jobs((5, NOW - 7200, NOW - 3600)))

        self.assertEqual(self.states(), {5: "expired"})
        self.assertEqual(self.units.started, [])

    def test_a_running_job_that_was_broken_off_failed(self):
        self.take(signed_jobs((5, NOW + 3600, NOW + 7200)))
        entry = self.store.jobs()[5]
        entry["state"] = agent.RUNNING
        self.store.save(entry)

        with self.store.job_lock(5):
            self.schedule()
        self.assertEqual(self.states(), {5: "running"})

        self.schedule()
        self.assertEqual(self.states(), {5: "failed"})


class TestRunning(Jobs):
    def test_a_job_that_ends_with_0_is_ok(self):
        self.take(signed_jobs((5, NOW - 60, NOW + 3600)))

        self.assertEqual(self.run_job(5), 0)

        entry = self.store.jobs()[5]
        self.assertEqual(entry["state"], "ok")
        self.assertGreaterEqual(entry["started"], NOW)
        self.assertGreaterEqual(entry["ended"], entry["started"])

    def test_a_job_that_does_not_end_with_0_failed(self):
        self.take(signed_jobs((5, NOW - 60, NOW + 3600)))

        self.assertEqual(self.run_job(5, command=("false",)), 1)

        self.assertEqual(self.states(), {5: "failed"})

    def test_the_command_runs_while_the_job_is_running(self):
        self.take(signed_jobs((5, NOW - 60, NOW + 3600)))
        script = (f"import json, sys; sys.exit(0 if json.load(open({os.path.join(self.store.jobs_dir, '5.json')!r}))"
                  "['state'] == 'running' else 1)")

        self.assertEqual(self.run_job(5, command=(sys.executable, "-c", script)), 0)

    def test_the_signature_is_checked_again(self):
        self.take(signed_jobs((5, NOW - 60, NOW + 3600)))

        self.assertEqual(self.run_job(5, manager_keys=(OTHER_KEY,)), 1)

        entry = self.store.jobs()[5]
        self.assertEqual(entry["state"], "failed")
        self.assertIsNone(entry["started"])

    def test_the_expiry_is_checked_again(self):
        self.take(signed_jobs((5, NOW - 60, NOW + 3600)))
        with unittest.mock.patch.object(agent.time, "time", return_value=NOW + 3700):
            self.assertEqual(self.run_job(5), 1)

        self.assertEqual(self.states(), {5: "expired"})

    def test_a_job_does_not_run_before_its_time(self):
        self.take(signed_jobs((5, NOW + 3600, NOW + 7200)))

        self.assertEqual(self.run_job(5), 1)

        self.assertEqual(self.states(), {5: "scheduled"})

    def test_only_a_scheduled_job_runs(self):
        self.take(signed_jobs((5, NOW - 60, NOW + 3600), (6, NOW - 60, NOW + 3600)))

        self.assertEqual(self.run_job(5), 1)
        self.assertEqual(self.run_job(7), 1)
        self.assertEqual(self.states(), {5: "cancelled", 6: "scheduled"})

    def test_a_job_runs_once_at_a_time(self):
        self.take(signed_jobs((5, NOW - 60, NOW + 3600)))

        with self.store.job_lock(5):
            self.assertEqual(self.run_job(5), 1)

        self.assertEqual(self.states(), {5: "scheduled"})


class TestReportedJobs(Jobs):
    def test_the_report_has_the_scheduled_and_the_ended_jobs(self):
        self.take(signed_jobs((5, NOW - 60, NOW + 3600), (6, NOW + 60, NOW + 3600),
                              (7, NOW - 60, NOW + 3600)))
        entry = self.store.jobs()[7]
        entry.update(state=agent.RUNNING, started=NOW)
        self.store.save(entry)

        self.assertEqual(agent.reported_jobs(self.store.jobs()), [
            {"sequence": 5, "state": "cancelled", "started": None, "ended": NOW},
            {"sequence": 6, "state": "cancelled", "started": None, "ended": NOW},
            {"sequence": 7, "state": "scheduled"},
        ])

    def test_ended_jobs_are_removed_once_they_are_not_open(self):
        self.take(signed_jobs((5, NOW + 60, NOW + 3600)))
        self.take(signed_jobs((6, NOW + 60, NOW + 3600)), open=[5, 6])
        self.take(signed_jobs((7, NOW + 60, NOW + 3600)), open=[5, 6, 7])

        self.take([], open=[5, 7])

        self.assertEqual(self.states(), {5: "cancelled", 7: "scheduled"})

    def test_a_scheduled_job_stays_when_it_is_not_open(self):
        self.take(signed_jobs((5, NOW + 60, NOW + 3600)))

        self.take([], open=[])

        self.assertEqual(self.states(), {5: "scheduled"})

    def test_the_dashboard_learns_the_states_of_the_jobs(self):
        dashboard = load_hd_dashboard()
        db = dashboard.connect(":memory:")
        token = "Bearer " + "a" * 64

        def report():
            body = json.dumps({"format": agent.FORMAT, "host": HOST,
                               "jobs": agent.reported_jobs(self.store.jobs())}).encode()
            reply = dashboard.accept_report(db, body, token, "127.0.0.1", int(time.time()), 30)
            # As it goes over the wire.
            reply = json.loads(json.dumps(reply))
            self.take(reply["jobs"], open=reply["open"])
            return {value["sequence"]: value["state"] for value in dashboard.load_jobs(db)}

        report()
        dashboard.accept_jobs(db, json.dumps(signed_jobs((5, NOW - 60, NOW + 3600))).encode(),
                              [MANAGER_KEY], NOW)
        self.assertEqual(report(), {5: "waiting"})
        self.assertEqual(self.states(), {5: "scheduled"})
        self.assertEqual(report(), {5: "scheduled"})

        self.run_job(5)
        self.assertEqual(report(), {5: "ok"})
        self.assertEqual(self.states(), {})
        self.assertEqual(report(), {5: "ok"})

if __name__ == '__main__':
    unittest.main()
