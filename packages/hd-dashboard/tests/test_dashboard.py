# Tests for hd-dashboard, the hyprdebian dashboard: accepting reports, judging hosts, relaying jobs,
# the pages and the settings. The jobs are signed with test keys made in a temporary directory.

import importlib.machinery
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import threading
import unittest
import unittest.mock
import urllib.error
import urllib.request

PACKAGES_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HD_DASHBOARD = os.path.join(PACKAGES_DIR, 'hd-dashboard', 'hd-dashboard')

NOW = 1_800_000_000
TOKEN = "a" * 32


def load_hd_dashboard():
    # hd-dashboard has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_dashboard', HD_DASHBOARD)
    spec = importlib.util.spec_from_loader('hd_dashboard', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


dash = load_hd_dashboard()
job = dash.job

DEFAULTS = {"listen": "0.0.0.0", "port": 80, "stale_minutes": 45, "offline_days": 14, "history_days": 30,
            "hardening_exceptions": {}, "manager_keys": []}

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


def signed(sequence, host="box", key=MANAGER, not_before=NOW, expires=NOW + 86400):
    return job.sign({"sequence": sequence, "host": host, "command": "upgrade", "not_before": not_before,
                     "expires": expires}, key)


def healthy(host="box", version="1.3"):
    """A report of a host without issues."""
    return {
        "format": 1,
        "host": host,
        "fqdn": host + ".example.net",
        "hyprdebian": {"version": version, "suite": "unstable"},
        "os": {"debian": "forky/sid", "kernel": "7.2.10", "kernel_newest": "7.2.10", "boot_time": NOW - 3600,
               "secure_boot": "on"},
        "load": [0.5, 0.4, 0.3],
        "cpus": 4,
        "memory": {"total": 1000, "used": 250},
        "pools": [{"name": "rpool", "health": "ONLINE", "size": 1000, "capacity": 30,
                   "scrub": {"repaired": 0, "errors": 0, "time": NOW - 86400}}],
        "arrays": [],
        "units": {"state": "running", "failed": []},
        "updates": {"upgradable": 0, "lists_time": NOW, "last_upgrade": NOW - 86400,
                    "reboot_required": False},
        "snapshot": {"name": "hd-up-2026-10-01_100000", "time": NOW - 86400},
        "timers": [{"unit": "backup-vms.timer", "service": "backup-vms.service", "last": NOW - 3600,
                    "next": NOW + 3600, "result": "success"}],
        "hardening": {"ssh": "on", "sysctl": "on", "modules": "on", "zfs": "on", "ufw": "on"},
    }


def post(db, report, token=TOKEN, now=NOW):
    return dash.accept_report(db, json.dumps(report).encode(), "Bearer " + token, "10.0.0.2", now, 30)


def agent_report(host="box", jobs=()):
    """A report of hd-agent, of format 2, with the states of its jobs."""
    report = healthy(host)
    report.update(format=2, jobs=list(jobs))
    return report


def send_jobs(db, *jobs, keys=None, now=NOW):
    return dash.accept_jobs(db, json.dumps(list(jobs)).encode(), [MANAGER_KEY] if keys is None else keys, now)


def states(db):
    return {row["sequence"]: row["state"] for row in db.execute("SELECT sequence, state FROM jobs")}


class TestAccept(unittest.TestCase):
    def setUp(self):
        self.db = dash.connect(":memory:")

    def test_first_report_registers_the_host_and_its_token(self):
        self.assertEqual(post(self.db, healthy()), {"jobs": [], "open": []})

        row = self.db.execute("SELECT * FROM hosts").fetchone()
        self.assertEqual((row["name"], row["address"], row["last_seen"]), ("box", "10.0.0.2", NOW))
        self.assertEqual(row["token"], dash.hash_token(TOKEN))

    def test_later_reports_need_the_same_token(self):
        post(self.db, healthy())
        post(self.db, healthy(), now=NOW + 900)

        with self.assertRaises(dash.Refused) as refused:
            post(self.db, healthy(), token="b" * 32, now=NOW + 1800)

        self.assertEqual(refused.exception.status, 403)
        self.assertEqual(self.db.execute("SELECT last_seen FROM hosts").fetchone()[0], NOW + 900)

    def test_malformed_reports_are_refused(self):
        cases = [
            (b'{"format": 1, "host": "box"}', None, 401),
            (b'{"format": 1, "host": "box"}', "Bearer short", 401),
            (b'not json', "Bearer " + TOKEN, 400),
            (b'[1, 2]', "Bearer " + TOKEN, 400),
            (b'{"format": 1, "host": "../etc"}', "Bearer " + TOKEN, 400),
            (b'{"format": 99, "host": "box"}', "Bearer " + TOKEN, 400),
            (b'{"format": 2, "host": "box"}', "Bearer " + TOKEN, 400),
            (b'{"format": 2, "host": "box", "jobs": [{"sequence": 1}]}', "Bearer " + TOKEN, 400),
            (b'{"format": 2, "host": "box", "jobs": [{"sequence": 1, "state": "ok", "ended": "x"}]}',
             "Bearer " + TOKEN, 400),
            (b'{"format": 2, "host": "box", "jobs": [{"sequence": 9223372036854775808, "state": "ok"}]}',
             "Bearer " + TOKEN, 400),
        ]

        for body, authorization, status in cases:
            with self.subTest(body=body, authorization=authorization):
                with self.assertRaises(dash.Refused) as refused:
                    dash.accept_report(self.db, body, authorization, "10.0.0.2", NOW, 30)
                self.assertEqual(refused.exception.status, status)

    def test_history_keeps_the_metrics_and_drops_old_rows(self):
        post(self.db, healthy(), now=NOW - 40 * 86400)
        post(self.db, healthy())

        rows = self.db.execute("SELECT time, load, memory, pool FROM history").fetchall()
        self.assertEqual([tuple(row) for row in rows], [(NOW, 0.125, 25.0, 30)])

    def test_metrics_of_a_sparse_report(self):
        self.assertEqual(dash.metrics({"load": "x", "memory": {"total": 0}, "pools": [{}]}), (None, None, None))


class TestAssess(unittest.TestCase):
    def messages(self, report, last_seen=NOW, dashboard_version="1.3"):
        return dash.assess(report, last_seen, NOW, 45, 14, dashboard_version)

    def test_a_healthy_host_has_no_issues(self):
        self.assertEqual(self.messages(healthy()), [])

    def test_problems_are_found(self):
        report = healthy(version="1.2")
        report["units"] = {"state": "degraded", "failed": ["zfs-zed.service"]}
        report["pools"][0].update(health="DEGRADED", capacity=92,
                                  scrub={"repaired": 1572864, "errors": 2, "time": NOW - 86400})
        report["arrays"] = [{"name": "swap", "state": "clean", "degraded": 1}]
        report["timers"][0]["result"] = "exit-code"
        report["os"].update(kernel_newest="7.3.0", secure_boot="setup")
        report["updates"].update(upgradable=3, last_upgrade=NOW - 20 * 86400)

        self.assertEqual(self.messages(report, last_seen=NOW - 15 * 86400), [
            ("bad", "No report for 15 days"),
            ("bad", "1 failed: zfs-zed.service"),
            ("bad", "Pool rpool is DEGRADED"),
            ("bad", "Pool rpool is 92% full"),
            ("bad", "Scrub of rpool found 2 errors"),
            ("warn", "Scrub of rpool repaired 1.5 MiB"),
            ("bad", "Array swap is degraded"),
            ("bad", "backup-vms.service failed: exit-code"),
            ("bad", "Secure Boot is off, firmware in setup mode"),
            ("warn", "Reboot pending"),
            ("warn", "Last upgrade 20 days ago"),
            ("info", "3 upgradable packages"),
            ("warn", "HyprDebian 1.2, dashboard is 1.3"),
        ])

    def test_an_offline_host_is_a_note_for_two_weeks(self):
        self.assertEqual(self.messages(healthy(), last_seen=NOW - 3600), [("info", "Offline, no report for 60 min")])
        self.assertEqual(self.messages(healthy(), last_seen=NOW - 13 * 86400), [("info", "Offline, no report for 13 days")])
        self.assertEqual(self.messages(healthy(), last_seen=NOW - 15 * 86400), [("bad", "No report for 15 days")])

    def test_a_pool_without_a_completed_scrub_has_no_issues(self):
        report = healthy()
        report["pools"][0]["scrub"] = None
        self.assertEqual(self.messages(report), [])

    def test_secure_boot_must_be_on(self):
        for state, message in [("off", "Secure Boot is off"),
                               ("unsupported", "Secure Boot is not supported by the firmware"),
                               ("legacy", "Secure Boot is off, booted without UEFI")]:
            with self.subTest(state=state):
                report = healthy()
                report["os"]["secure_boot"] = state
                self.assertEqual(self.messages(report), [("bad", message)])

    def test_parts_that_are_not_hardened_are_warnings(self):
        report = healthy()
        report["hardening"] = {"ssh": "off", "sysctl": "changed", "modules": "on", "zfs": "off", "ufw": "unknown"}
        self.assertEqual(self.messages(report), [("warn", "Not hardened: ssh, zfs"), ("warn", "Hardening changed: sysctl"),
                                                 ("info", "Hardening unknown: ufw")])

    def test_a_report_without_hardening_is_a_warning(self):
        for hardening in [None, {}]:
            report = healthy()
            report["hardening"] = hardening
            self.assertEqual(self.messages(report), [("warn", "Hardening not reported")])

    def test_exceptions_are_not_judged(self):
        report = healthy()
        report["hardening"] = {"ssh": "on", "sysctl": "off", "ufw": "changed"}
        self.assertEqual(dash.assess(report, NOW, NOW, 45, 14, "1.3", ["sysctl"]), [("warn", "Hardening changed: ufw")])

    def test_exceptions_are_per_host(self):
        with tempfile.TemporaryDirectory() as tmp:
            db = dash.connect(f"{tmp}/dashboard.db")
            for host in ["beast", "box"]:
                report = healthy(host)
                report["hardening"]["sysctl"] = "off"
                post(db, report, token=host * 8)
            with unittest.mock.patch.object(dash, "hyprdebian_version", return_value=None):
                issues = {host["name"]: host["issues"] for host in dash.load_hosts(db, NOW, 45, 14, {"beast": ["sysctl"]})}
            db.close()
        self.assertEqual(issues, {"beast": [], "box": [("warn", "Not hardened: sysctl")]})

    def test_a_report_without_secure_boot_is_an_error(self):
        report = healthy()
        del report["os"]["secure_boot"]
        self.assertEqual(self.messages(report), [("bad", "Secure Boot not reported")])

    def test_versions_compare_by_number(self):
        self.assertLess(dash.version_key("v0.99"), dash.version_key("v0.100"))
        self.assertLess(dash.version_key("1.9"), dash.version_key("1.10"))

    def test_a_host_newer_than_the_dashboard_is_not_outdated(self):
        self.assertEqual(self.messages(healthy(version="1.4")), [])

    def test_a_host_without_a_version_is_outdated(self):
        report = healthy()
        del report["hyprdebian"]["version"]
        self.assertEqual(self.messages(report), [("warn", "HyprDebian unknown, dashboard is 1.3")])

    def test_no_version_of_the_dashboard_judges_no_versions(self):
        self.assertEqual(self.messages(healthy(version="1.2"), dashboard_version=None), [])

    def test_hosts_are_compared_to_the_dashboard_not_to_each_other(self):
        db = dash.connect(":memory:")
        post(db, healthy("alpha", version="1.2"))
        post(db, healthy("beta", version="1.3"), token="b" * 32)
        post(db, healthy("gamma", version="1.4"), token="c" * 32)

        with unittest.mock.patch.object(dash, "hyprdebian_version", return_value="1.3"):
            issues = {host["name"]: host["issues"] for host in dash.load_hosts(db, NOW, 45, 14)}

        self.assertEqual(issues, {"alpha": [("warn", "HyprDebian 1.2, dashboard is 1.3")], "beta": [], "gamma": []})

    def test_version_of_the_dashboard(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "version.json")
            self.assertIsNone(dash.hyprdebian_version(path))

            with open(path, "w") as f:
                f.write("not json")
            self.assertIsNone(dash.hyprdebian_version(path))

            with open(path, "w") as f:
                json.dump({"major": 1, "minor": 3}, f)
            self.assertEqual(dash.hyprdebian_version(path), "1.3")

    def test_notes_leave_a_host_good(self):
        self.assertEqual(dash.level_of([("info", "3 upgradable packages")]), "ok")
        self.assertEqual(dash.level_of([("info", "x"), ("warn", "y")]), "warn")
        self.assertEqual(dash.level_of([("warn", "x"), ("bad", "y")]), "bad")

    def test_worst_hosts_come_first(self):
        db = dash.connect(":memory:")
        post(db, healthy("alpha"))
        failing = healthy("beta")
        failing["units"]["failed"] = ["x.service"]
        post(db, failing, token="b" * 32)

        with unittest.mock.patch.object(dash, "hyprdebian_version", return_value="1.3"):
            self.assertEqual([host["name"] for host in dash.load_hosts(db, NOW, 45, 14)], ["beta", "alpha"])


class TestJobs(unittest.TestCase):
    def setUp(self):
        self.db = dash.connect(":memory:")
        post(self.db, healthy())

    def refusal(self, *jobs, keys=None):
        with self.assertRaises(dash.Refused) as refused:
            send_jobs(self.db, *jobs, keys=keys)
        return refused.exception.status

    def test_signed_jobs_are_stored_as_waiting(self):
        self.assertEqual(send_jobs(self.db, signed(2), signed(1)), [1, 2])
        self.assertEqual(states(self.db), {1: "waiting", 2: "waiting"})
        self.assertEqual(dash.load_jobs(self.db)[0]["signed"], signed_job_of(self.db, 1))

    def test_a_job_signed_by_another_key_is_refused(self):
        self.assertEqual(self.refusal(signed(1, key=OTHER)), 403)
        self.assertEqual(states(self.db), {})

    def test_without_manager_keys_every_job_is_refused(self):
        self.assertEqual(self.refusal(signed(1), keys=[]), 403)

    def test_a_changed_job_is_refused(self):
        changed = signed(1)
        changed["job"]["host"] = "other"
        post(self.db, healthy("other"), token="b" * 32)
        self.assertEqual(self.refusal(changed), 403)

    def test_malformed_jobs_are_refused(self):
        incomplete = signed(1)
        del incomplete["job"]["expires"]
        for body in [b"not json", b"[]", b"{}", b"[1]", json.dumps([incomplete]).encode(),
                     json.dumps([signed(2 ** 63)]).encode()]:
            with self.subTest(body=body):
                with self.assertRaises(dash.Refused) as refused:
                    dash.accept_jobs(self.db, body, [MANAGER_KEY], NOW)
                self.assertEqual(refused.exception.status, 400)

    def test_a_job_for_a_host_without_a_token_is_refused(self):
        self.assertEqual(self.refusal(signed(1, host="nope")), 422)

    def test_a_taken_sequence_number_is_refused(self):
        send_jobs(self.db, signed(5))
        self.assertEqual(self.refusal(signed(5)), 409)
        self.assertEqual(self.refusal(signed(4)), 409)
        self.assertEqual(self.refusal(signed(7), signed(7)), 409)

    def test_jobs_are_stored_all_or_none(self):
        send_jobs(self.db, signed(5))
        self.assertEqual(self.refusal(signed(6), signed(5)), 409)
        self.assertEqual(self.refusal(signed(6), signed(7, host="nope")), 422)
        self.assertEqual(self.refusal(signed(6), signed(7, key=OTHER)), 403)
        self.assertEqual(states(self.db), {5: "waiting"})

    def test_the_highest_sequence_number_comes_from_the_jobs_and_the_reports(self):
        self.assertEqual(dash.highest_sequence(self.db), 0)
        send_jobs(self.db, signed(3))
        self.assertEqual(dash.highest_sequence(self.db), 3)

        # A job this dashboard never stored, as after a reinstallation.
        post(self.db, agent_report(jobs=[{"sequence": 9, "state": "scheduled"}]))
        self.assertEqual(dash.highest_sequence(self.db), 9)
        self.assertEqual(self.refusal(signed(8)), 409)
        self.assertEqual(send_jobs(self.db, signed(10)), [10])

    def test_the_sequence_stays_when_jobs_are_removed(self):
        send_jobs(self.db, signed(3))
        self.db.execute("DELETE FROM jobs")
        self.assertEqual(dash.highest_sequence(self.db), 3)

    def test_a_job_is_handed_out_until_it_is_scheduled(self):
        first = signed(1)
        send_jobs(self.db, first)

        self.assertEqual(post(self.db, agent_report()), {"jobs": [first], "open": [1]})
        self.assertEqual(post(self.db, agent_report()), {"jobs": [first], "open": [1]})

        reply = post(self.db, agent_report(jobs=[{"sequence": 1, "state": "scheduled"}]))
        self.assertEqual(reply, {"jobs": [], "open": [1]})
        self.assertEqual(states(self.db), {1: "scheduled"})

        reply = post(self.db, agent_report(jobs=[{"sequence": 1, "state": "ok", "started": NOW + 10, "ended": NOW + 99}]))
        self.assertEqual(reply, {"jobs": [], "open": []})
        value = dash.load_jobs(self.db)[0]
        self.assertEqual((value["state"], value["started"], value["ended"]), ("ok", NOW + 10, NOW + 99))

    def test_a_job_that_ends_between_two_reports_is_no_longer_handed_out(self):
        send_jobs(self.db, signed(1), signed(2))
        reply = post(self.db, agent_report(jobs=[{"sequence": 1, "state": "failed", "started": NOW, "ended": NOW + 5}]))
        self.assertEqual(reply, {"jobs": [signed_job_of(self.db, 2)], "open": [2]})

    def test_jobs_are_handed_out_in_order_of_sequence(self):
        send_jobs(self.db, signed(3), signed(1))
        self.assertEqual([value["job"]["sequence"] for value in post(self.db, agent_report())["jobs"]], [1, 3])

    def test_each_host_gets_its_own_jobs(self):
        post(self.db, healthy("other"), token="b" * 32)
        send_jobs(self.db, signed(1, host="other"))
        self.assertEqual(post(self.db, agent_report()), {"jobs": [], "open": []})

        # A report cannot change the jobs of another host.
        post(self.db, agent_report(jobs=[{"sequence": 1, "state": "ok"}]))
        self.assertEqual(states(self.db), {1: "waiting"})

    def test_a_report_of_hd_report_changes_no_jobs(self):
        send_jobs(self.db, signed(1))
        report = healthy()
        report["jobs"] = [{"sequence": 1, "state": "ok"}]
        self.assertEqual(post(self.db, report), {"jobs": [signed_job_of(self.db, 1)], "open": [1]})
        self.assertEqual(states(self.db), {1: "waiting"})

    def test_an_ended_job_stays_ended(self):
        send_jobs(self.db, signed(1))
        post(self.db, agent_report(jobs=[{"sequence": 1, "state": "cancelled"}]))
        post(self.db, agent_report(jobs=[{"sequence": 1, "state": "scheduled"}]))
        value = dash.load_jobs(self.db)[0]
        self.assertEqual((value["state"], value["ended"]), ("cancelled", NOW))

    def test_a_waiting_job_expires(self):
        send_jobs(self.db, signed(1, expires=NOW + 3600))
        self.assertEqual(post(self.db, agent_report(), now=NOW + 3601), {"jobs": [], "open": []})
        value = dash.load_jobs(self.db)[0]
        self.assertEqual((value["state"], value["ended"]), ("expired", NOW + 3600))

    def test_a_scheduled_job_does_not_expire_on_the_dashboard(self):
        send_jobs(self.db, signed(1, expires=NOW + 3600))
        post(self.db, agent_report(jobs=[{"sequence": 1, "state": "scheduled"}]))
        self.assertEqual(post(self.db, agent_report(), now=NOW + 3601), {"jobs": [], "open": [1]})

    def test_ended_jobs_are_removed_after_history_days(self):
        send_jobs(self.db, signed(1), signed(2), signed(3))
        post(self.db, agent_report(jobs=[{"sequence": 1, "state": "ok", "started": NOW, "ended": NOW},
                                         {"sequence": 2, "state": "scheduled"}]))

        post(self.db, agent_report(), now=NOW + 29 * 86400)
        self.assertEqual(states(self.db), {1: "ok", 2: "scheduled", 3: "expired"})

        post(self.db, agent_report(), now=NOW + 31 * 86400)
        self.assertEqual(states(self.db), {2: "scheduled", 3: "expired"})

        post(self.db, agent_report(), now=NOW + 32 * 86400)
        self.assertEqual(states(self.db), {2: "scheduled"})


def signed_job_of(db, sequence):
    return json.loads(db.execute("SELECT signed FROM jobs WHERE sequence = ?", (sequence,)).fetchone()[0])


class TestAssessJobs(unittest.TestCase):
    def jobs(self, *jobs):
        return [{"sequence": sequence, "command": command, "state": state} for sequence, command, state in jobs]

    def test_failed_and_expired_jobs(self):
        self.assertEqual(dash.assess_jobs(self.jobs((1, "upgrade", "failed"), (2, "upgrade", "expired"))),
                         [("bad", "Upgrade job 1 failed"), ("warn", "Upgrade job 2 expired")])

    def test_a_newer_job_that_ends_ok_clears_them(self):
        self.assertEqual(dash.assess_jobs(self.jobs((1, "upgrade", "failed"), (2, "upgrade", "expired"),
                                                    (3, "upgrade", "ok"))), [])

    def test_an_older_job_that_ended_ok_does_not(self):
        self.assertEqual(dash.assess_jobs(self.jobs((1, "upgrade", "ok"), (2, "upgrade", "failed"))),
                         [("bad", "Upgrade job 2 failed")])

    def test_only_a_job_with_the_same_command_clears_them(self):
        self.assertEqual(dash.assess_jobs(self.jobs((1, "upgrade", "failed"), (2, "other", "ok"))),
                         [("bad", "Upgrade job 1 failed")])

    def test_newer_jobs_that_did_not_end_ok_do_not_clear_them(self):
        self.assertEqual(dash.assess_jobs(self.jobs((1, "upgrade", "failed"), (2, "upgrade", "cancelled"),
                                                    (3, "upgrade", "scheduled"))),
                         [("bad", "Upgrade job 1 failed")])

    def test_cancelled_jobs_are_not_judged(self):
        self.assertEqual(dash.assess_jobs(self.jobs((1, "upgrade", "cancelled"))), [])

    def test_jobs_judge_their_host(self):
        db = dash.connect(":memory:")
        post(db, healthy())
        send_jobs(db, signed(1))
        post(db, agent_report(jobs=[{"sequence": 1, "state": "failed", "started": NOW, "ended": NOW + 60}]))

        with unittest.mock.patch.object(dash, "hyprdebian_version", return_value=None):
            host = dash.load_hosts(db, NOW, 45, 14)[0]
        self.assertEqual((host["level"], host["issues"]), ("bad", [("bad", "Upgrade job 1 failed")]))
        self.assertEqual(host["format"], 2)


class TestPages(unittest.TestCase):
    def setUp(self):
        self.db = dash.connect(":memory:")

    def test_report_values_are_escaped(self):
        report = healthy()
        report["units"]["failed"] = ["<script>alert(1)</script>"]
        post(self.db, report)

        for body in [dash.overview_page(self.db, NOW, 45, 14), dash.host_page(self.db, "box", NOW, 45, 14, 30)]:
            self.assertNotIn("<script>", body)
            self.assertIn("&lt;script&gt;", body)

    def test_hardening_shows_each_part_with_its_state(self):
        report = healthy()
        report["hardening"] = {"ssh": "on", "sysctl": "changed", "zfs": "off", "ufw": "unknown"}
        post(self.db, report)

        body = dash.overview_page(self.db, NOW, 45, 14)
        self.assertIn('<td>Hardening</td><td><span class="ok">ssh</span> <span class="warn">sysctl changed</span> '
                      '<span class="warn">zfs off</span> <span class="dim">ufw unknown</span></td>', body)

    def test_hardening_is_left_out_when_not_reported(self):
        for hardening in [None, {}]:
            report = healthy()
            report["hardening"] = hardening
            post(self.db, report)
            self.assertNotIn("<td>Hardening</td>", dash.overview_page(self.db, NOW, 45, 14))

    def test_an_exception_shows_in_grey(self):
        report = healthy()
        report["hardening"] = {"ssh": "on", "sysctl": "off"}
        post(self.db, report)

        body = dash.overview_page(self.db, NOW, 45, 14, {"box": ["ssh", "sysctl"]})
        self.assertIn('<td>Hardening</td><td><span class="ok">ssh, exception</span> '
                      '<span class="dim">sysctl off, exception</span></td>', body)

    def test_jobs_show_on_the_page_of_their_host(self):
        post(self.db, healthy())
        send_jobs(self.db, signed(1), signed(2))
        post(self.db, agent_report(jobs=[{"sequence": 1, "state": "failed", "started": NOW, "ended": NOW + 60}]),
             now=NOW + 120)

        body = dash.host_page(self.db, "box", NOW + 120, 45, 14, 30)
        self.assertIn('<td>2</td><td>upgrade</td><td class="info">waiting</td>', body)
        self.assertIn('<td>1</td><td>upgrade</td><td class="bad">failed</td>', body)
        self.assertLess(body.index("<td>2</td>"), body.index("<td>1</td>"))

    def test_a_host_without_jobs(self):
        post(self.db, healthy())
        self.assertIn("No jobs.", dash.host_page(self.db, "box", NOW, 45, 14, 30))

    def test_unknown_host_has_no_page(self):
        self.assertIsNone(dash.host_page(self.db, "nope", NOW, 45, 14, 30))

    def test_empty_dashboard(self):
        self.assertIn("No reports yet", dash.overview_page(self.db, NOW, 45, 14))


class TestConfig(unittest.TestCase):
    def test_the_defaults_of_the_package(self):
        with open(os.path.join(PACKAGES_DIR, 'hd-dashboard', 'defaults.json')) as f:
            self.assertEqual(dash.parse_config(None, json.load(f)), DEFAULTS)

    def test_settings_override_the_defaults(self):
        config = {"hd-network": {}, "hd-dashboard": {"port": 8080}}
        self.assertEqual(dash.parse_config(config, DEFAULTS), dict(DEFAULTS, port=8080))

    def test_wrong_settings_are_refused(self):
        cases = [
            {"hd-dashboard": []},
            {"hd-dashboard": {"port": "80"}},
            {"hd-dashboard": {"port": 70000}},
            {"hd-dashboard": {"port": True}},
            {"hd-dashboard": {"listen": ""}},
            {"hd-dashboard": {"history_days": 0}},
            {"hd-dashboard": {"offline_days": 0}},
            {"hd-dashboard": {"hardening_exceptions": []}},
            {"hd-dashboard": {"hardening_exceptions": {"beast": "sysctl"}}},
            {"hd-dashboard": {"hardening_exceptions": {"beast": ["trace"]}}},
            {"hd-dashboard": {"hardening_exceptions": {"bad name": ["sysctl"]}}},
            {"hd-dashboard": {"manager_keys": MANAGER_KEY}},
            {"hd-dashboard": {"manager_keys": ["ssh-rsa AAAA"]}},
            {"hd-dashboard": {"manager_keys": [MANAGER_KEY + "\nssh-ed25519 AAAA"]}},
        ]

        for config in cases:
            with self.subTest(config=config):
                with self.assertRaises(dash.ConfigError):
                    dash.parse_config(config, DEFAULTS)

    def test_manager_keys(self):
        config = {"hd-dashboard": {"manager_keys": [MANAGER_KEY, OTHER_KEY]}}
        self.assertEqual(dash.parse_config(config, DEFAULTS)["manager_keys"], [MANAGER_KEY, OTHER_KEY])


class TestServer(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        database = f"{self.tmp.name}/dashboard.db"
        dash.connect(database).close()
        dash.Handler.config = dict(DEFAULTS)
        dash.Handler.database_path = database
        self.server = dash.ThreadingHTTPServer(("127.0.0.1", 0), dash.Handler)
        self.url = f"http://127.0.0.1:{self.server.server_port}"
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.tmp.cleanup()

    def request(self, path, data=None, headers=None):
        request = urllib.request.Request(self.url + path, data=data, headers=headers or {})
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                return response.status, response.read().decode()
        except urllib.error.HTTPError as error:
            with error:
                return error.code, error.read().decode()

    def test_report_shows_up_on_the_pages(self):
        status, _ = self.request("/report", json.dumps(healthy()).encode(), {"Authorization": "Bearer " + TOKEN})
        self.assertEqual(status, 200)

        status, body = self.request("/")
        self.assertEqual(status, 200)
        self.assertIn('href="/host/box"', body)

        status, body = self.request("/api/hosts")
        self.assertEqual(json.loads(body)[0]["name"], "box")
        self.assertNotIn(TOKEN, body)
        self.assertNotIn(dash.hash_token(TOKEN), body)

        self.assertEqual(self.request("/host/box")[0], 200)
        self.assertEqual(self.request("/host/other")[0], 404)

    def test_wrong_token_and_large_reports_are_refused(self):
        self.request("/report", json.dumps(healthy()).encode(), {"Authorization": "Bearer " + TOKEN})

        status, body = self.request("/report", json.dumps(healthy()).encode(), {"Authorization": "Bearer " + "b" * 32})
        self.assertEqual(status, 403)
        self.assertIn("hd-dashboard forget", body)

        status, _ = self.request("/report", b"x" * (dash.MAX_REPORT_SIZE + 1), {"Authorization": "Bearer " + TOKEN})
        self.assertEqual(status, 413)

        self.assertEqual(self.request("/elsewhere", b"{}")[0], 404)

    def test_jobs_go_from_the_manager_to_the_agent(self):
        dash.Handler.config["manager_keys"] = [MANAGER_KEY]
        authorization = {"Authorization": "Bearer " + TOKEN}
        status, body = self.request("/report", json.dumps(agent_report()).encode(), authorization)
        self.assertEqual((status, json.loads(body)), (200, {"jobs": [], "open": []}))

        first = signed(1)
        self.assertEqual(self.request("/jobs", json.dumps([first]).encode()), (200, "stored jobs 1\n"))
        self.assertEqual(self.request("/jobs", json.dumps([first]).encode())[0], 409)
        self.assertEqual(self.request("/jobs", json.dumps([signed(2, key=OTHER)]).encode())[0], 403)
        self.assertEqual(self.request("/jobs", b"[")[0], 400)
        self.assertEqual(self.request("/jobs", json.dumps([signed(2, host="nope")]).encode())[0], 422)
        self.assertEqual(json.loads(self.request("/sequence")[1]), {"sequence": 1})

        status, body = self.request("/report", json.dumps(agent_report()).encode(), authorization)
        self.assertEqual(json.loads(body), {"jobs": [first], "open": [1]})

        jobs = json.loads(self.request("/api/jobs")[1])
        self.assertEqual([(value["sequence"], value["host"], value["state"]) for value in jobs], [(1, "box", "waiting")])
        self.assertEqual(json.loads(self.request("/api/hosts")[1])[0]["format"], 2)


class TestCommands(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.config = os.path.join(self.tmp.name, "config.json")
        with open(self.config, "w") as f:
            json.dump({"hd-dashboard": {"port": 18080}}, f)
        self.database = os.path.join(self.tmp.name, "dashboard.db")

    def tearDown(self):
        self.tmp.cleanup()

    def run_main(self, *args):
        argv = sys.argv
        sys.argv = ["hd-dashboard", "--config", self.config, "--database", self.database] + list(args)
        try:
            return dash.main()
        finally:
            sys.argv = argv

    def test_forget_removes_the_host_its_history_and_its_jobs(self):
        db = dash.connect(self.database)
        post(db, healthy())
        send_jobs(db, signed(1))
        db.close()

        self.assertEqual(self.run_main("forget", "box"), 0)
        self.assertEqual(self.run_main("forget", "box"), 1)

        db = dash.connect(self.database)
        self.assertEqual(db.execute("SELECT COUNT(*) FROM hosts").fetchone()[0], 0)
        self.assertEqual(db.execute("SELECT COUNT(*) FROM history").fetchone()[0], 0)
        self.assertEqual(db.execute("SELECT COUNT(*) FROM jobs").fetchone()[0], 0)
        self.assertEqual(dash.highest_sequence(db), 1)

    def test_commands_do_not_make_the_database(self):
        self.assertEqual(self.run_main("hosts"), 1)
        self.assertEqual(self.run_main("forget", "box"), 1)
        self.assertFalse(os.path.exists(self.database))

    def test_wrong_settings_stop_the_command(self):
        with open(self.config, "w") as f:
            json.dump({"hd-dashboard": {"port": "x"}}, f)

        self.assertEqual(self.run_main("serve"), 1)


if __name__ == '__main__':
    unittest.main()
