# Tests for hd-manager, the manager of the jobs of the package hd-manager: the settings, the times
# of --at and --expires, picking the hosts, and scheduling jobs against a local test dashboard, the
# hd-dashboard of this repository on a free local port: for hosts and for all, a sequence number
# that another manager takes, and a lost counter. The jobs are signed with test keys made in a
# temporary directory, and HOME is a temporary directory too.

import contextlib
import datetime
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
HD_MANAGER = os.path.join(PACKAGES_DIR, 'hd-manager', 'hd-manager')
HD_DASHBOARD = os.path.join(PACKAGES_DIR, 'hd-dashboard', 'hd-dashboard')

TOKEN = "a" * 32


def load(name, path):
    # The programs have no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader(name, path)
    spec = importlib.util.spec_from_loader(name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


manager = load('hd_manager', HD_MANAGER)
dash = load('hd_dashboard', HD_DASHBOARD)
job = manager.job

KEYS = tempfile.TemporaryDirectory()


def make_key(name):
    """Makes an Ed25519 key without a passphrase. Returns the path of the private key and the
    public key."""
    path = os.path.join(KEYS.name, name)
    subprocess.run([job.SSH_KEYGEN, "-q", "-t", "ed25519", "-N", "", "-C", name, "-f", path], check=True)
    with open(path + ".pub") as f:
        return path, f.read().strip()


MANAGER, MANAGER_KEY = make_key("manager")
# The signing itself, for the jobs of the other manager, when a test replaces job.sign_all.
SIGN_ALL = job.sign_all
OTHER, OTHER_KEY = make_key("other")

DASHBOARD_DEFAULTS = {"listen": "127.0.0.1", "port": 80, "stale_minutes": 45, "offline_days": 14,
                      "history_days": 30, "hardening_exceptions": {}, "manager_keys": [MANAGER_KEY, OTHER_KEY]}


def report(host, format=2):
    """A short report of hd-agent, format 2, or of hd-report, format 1."""
    value = {"format": format, "host": host, "hyprdebian": {"version": "1.93", "suite": "unstable"}}
    if format >= 2:
        value["jobs"] = []
    return value


class Home(unittest.TestCase):
    """A temporary HOME, for the settings and the counter of the sequence numbers."""

    def setUp(self):
        self.home = tempfile.TemporaryDirectory()
        patcher = unittest.mock.patch.dict(os.environ, {"HOME": self.home.name})
        patcher.start()
        self.addCleanup(patcher.stop)
        self.addCleanup(self.home.cleanup)

    def write_config(self, settings):
        os.makedirs(os.path.dirname(manager.config_file()))
        with open(manager.config_file(), "w") as f:
            json.dump({"hd-manager": settings}, f)


class TestSettings(Home):
    def test_defaults(self):
        self.assertEqual(manager.load_config(),
                         {"url": "http://dash.rsq-online.ddns.net/", "key": f"{self.home.name}/.ssh/hd-manager"})

    def test_settings_of_the_user(self):
        self.write_config({"url": "http://dash.example/", "key": "~/keys/manager"})
        self.assertEqual(manager.load_config(), {"url": "http://dash.example/", "key": f"{self.home.name}/keys/manager"})

    def test_wrong_settings(self):
        defaults = {"url": "http://dash.example/", "key": "~/.ssh/hd-manager"}
        for config in [{"hd-manager": []}, {"hd-manager": {"url": "dash.example"}}, {"hd-manager": {"key": ""}},
                       {"hd-manager": {"key": None}}]:
            with self.subTest(config=config), self.assertRaises(manager.Failure):
                manager.parse_config(config, defaults)


class TestTimes(unittest.TestCase):
    NOW = int(datetime.datetime(2026, 11, 18, 12, 0).timestamp())

    def test_time_of_day_is_the_next_one(self):
        self.assertEqual(manager.parse_time("13:30", self.NOW), int(datetime.datetime(2026, 11, 18, 13, 30).timestamp()))
        self.assertEqual(manager.parse_time("3:00", self.NOW), int(datetime.datetime(2026, 11, 19, 3, 0).timestamp()))
        self.assertEqual(manager.parse_time("12:00", self.NOW), int(datetime.datetime(2026, 11, 19, 12, 0).timestamp()))

    def test_dates(self):
        self.assertEqual(manager.parse_time("2026-11-20 03:00", self.NOW),
                         int(datetime.datetime(2026, 11, 20, 3, 0).timestamp()))
        self.assertEqual(manager.parse_time("2026-11-20", self.NOW), int(datetime.datetime(2026, 11, 20).timestamp()))
        self.assertEqual(manager.parse_time("2026-11-20T03:00+00:00", self.NOW),
                         int(datetime.datetime(2026, 11, 20, 3, 0, tzinfo=datetime.timezone.utc).timestamp()))

    def test_no_times(self):
        for text in ["25:00", "12:60", "tomorrow", "", "+2h"]:
            with self.subTest(text=text), self.assertRaises(manager.Failure):
                manager.parse_time(text, self.NOW)


class TestPickHosts(unittest.TestCase):
    HOSTS = [{"name": "web", "format": 2}, {"name": "box", "format": 2}, {"name": "old", "format": 1}]

    def test_all_is_every_host_of_hd_agent(self):
        self.assertEqual(manager.pick_hosts(self.HOSTS, ["all"]), ["box", "web"])

    def test_named_hosts(self):
        self.assertEqual(manager.pick_hosts(self.HOSTS, ["web", "box", "web"]), ["web", "box"])

    def test_refused_hosts(self):
        for targets, message in [(["old"], "still runs hd-report"), (["box", "nope"], "does not know nope"),
                                 (["all", "box"], "all cannot")]:
            with self.subTest(targets=targets), self.assertRaisesRegex(manager.Failure, message):
                manager.pick_hosts(self.HOSTS, targets)

        with self.assertRaisesRegex(manager.Failure, "no system that runs hd-agent"):
            manager.pick_hosts([{"name": "old", "format": 1}], ["all"])


class TestScheduling(Home):
    """Scheduling against a local test dashboard, which knows box and web, which run hd-agent,
    and old, which still runs hd-report."""

    def setUp(self):
        super().setUp()
        self.database = os.path.join(self.home.name, "dashboard.db")
        db = dash.connect(self.database)
        for value in [report("box"), report("web"), report("old", format=1)]:
            dash.accept_report(db, json.dumps(value).encode(), "Bearer " + TOKEN, "127.0.0.1", int(time.time()), 30)
        db.close()

        dash.Handler.config = dict(DASHBOARD_DEFAULTS)
        dash.Handler.database_path = self.database
        # The lines of the dashboard for the jobs it stores and refuses stay out of the output.
        patcher = unittest.mock.patch.object(dash.Handler, "log_message", lambda handler, format, *args: None)
        patcher.start()
        self.addCleanup(patcher.stop)
        server = dash.ThreadingHTTPServer(("127.0.0.1", 0), dash.Handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        self.settings = {"url": f"http://127.0.0.1:{server.server_port}/", "key": MANAGER}

    def stored(self):
        """The jobs on the dashboard, as (sequence, host, state)."""
        db = dash.connect(self.database)
        try:
            return [(value["sequence"], value["host"], value["state"]) for value in dash.load_jobs(db)]
        finally:
            db.close()

    def store_other(self, sequence, host="box"):
        """Stores a job of another manager on the dashboard."""
        now = int(time.time())
        signed = SIGN_ALL([{"sequence": sequence, "host": host, "command": "upgrade", "not_before": now,
                            "expires": now + 3600}], OTHER)[0]
        db = dash.connect(self.database)
        try:
            dash.accept_jobs(db, json.dumps([signed]).encode(), [OTHER_KEY], now)
        finally:
            db.close()

    def test_scheduling_for_hosts(self):
        before = int(time.time())
        jobs = manager.schedule(self.settings, "upgrade", ["web", "box"])

        self.assertEqual([(value["sequence"], value["host"]) for value in jobs], [(1, "web"), (2, "box")])
        self.assertEqual(self.stored(), [(1, "web", "waiting"), (2, "box", "waiting")])
        self.assertGreaterEqual(jobs[0]["not_before"], before)
        self.assertEqual(jobs[0]["expires"], jobs[0]["not_before"] + 86400)
        self.assertEqual(manager.read_sequence(), 2)

    def test_scheduling_at_a_time(self):
        now = int(time.time())
        at = time.strftime("%Y-%m-%d %H:%M", time.localtime(now + 7200))
        until = time.strftime("%Y-%m-%d %H:%M", time.localtime(now + 3 * 86400))
        jobs = manager.schedule(self.settings, "upgrade", ["box"], at=at, expires=until)

        self.assertEqual(jobs[0]["not_before"], int(datetime.datetime.fromisoformat(at).timestamp()))
        self.assertEqual(jobs[0]["expires"], int(datetime.datetime.fromisoformat(until).timestamp()))

        with self.assertRaisesRegex(manager.Failure, "is in the past"):
            manager.schedule(self.settings, "upgrade", ["box"], at="2026-01-01 03:00")
        with self.assertRaisesRegex(manager.Failure, "is not after"):
            manager.schedule(self.settings, "upgrade", ["box"], at=until, expires=at)

    def test_scheduling_for_all_leaves_out_hd_report(self):
        jobs = manager.schedule(self.settings, "upgrade", ["all"])

        self.assertEqual(self.stored(), [(1, "box", "waiting"), (2, "web", "waiting")])
        self.assertEqual([value["host"] for value in jobs], ["box", "web"])

    def test_host_with_hd_report_is_refused(self):
        with self.assertRaisesRegex(manager.Failure, "old still runs hd-report"):
            manager.schedule(self.settings, "upgrade", ["box", "old"])

        self.assertEqual(self.stored(), [])

    def test_taken_sequence_number_is_signed_again(self):
        calls = []

        def sign_and_collide(jobs, key):
            # Another manager stores a job with the first number between /sequence and /jobs.
            calls.append([value["sequence"] for value in jobs])
            if len(calls) == 1:
                self.store_other(jobs[0]["sequence"])
            return SIGN_ALL(jobs, key)

        with unittest.mock.patch.object(job, "sign_all", sign_and_collide), \
                contextlib.redirect_stderr(io.StringIO()) as stderr:
            jobs = manager.schedule(self.settings, "upgrade", ["box", "web"])

        self.assertEqual(calls, [[1, 2], [3, 4]])
        self.assertEqual([value["sequence"] for value in jobs], [3, 4])
        self.assertEqual(self.stored(), [(1, "box", "waiting"), (3, "box", "waiting"), (4, "web", "waiting")])
        self.assertIn("Signing again with new numbers", stderr.getvalue())
        self.assertEqual(manager.read_sequence(), 4)

    def test_lost_counter_takes_the_sequence_of_the_dashboard(self):
        manager.schedule(self.settings, "upgrade", ["box"])
        self.store_other(2)
        os.remove(manager.sequence_file())

        jobs = manager.schedule(self.settings, "upgrade", ["box"])

        self.assertEqual(jobs[0]["sequence"], 3)
        self.assertEqual(manager.read_sequence(), 3)

    def test_counter_above_the_dashboard_is_used(self):
        # As after the dashboard lost its jobs: this manager used up to 7.
        manager.write_sequence(7)

        jobs = manager.schedule(self.settings, "upgrade", ["box"])

        self.assertEqual(jobs[0]["sequence"], 8)
        self.assertEqual(self.stored(), [(8, "box", "waiting")])

    def test_dashboard_refusal_stops(self):
        # A dashboard without the key of this manager.
        dash.Handler.config["manager_keys"] = [OTHER_KEY]

        with self.assertRaisesRegex(manager.Failure, "403"):
            manager.schedule(self.settings, "upgrade", ["box"])

    def test_jobs_are_listed(self):
        manager.schedule(self.settings, "upgrade", ["all"])

        with contextlib.redirect_stdout(io.StringIO()) as out:
            manager.list_jobs(self.settings, "web")
        lines = out.getvalue().splitlines()
        self.assertEqual(len(lines), 2)
        self.assertTrue(lines[0].split()[:4] == ["SEQUENCE", "HOST", "COMMAND", "STATE"])
        self.assertEqual(lines[1].split()[:4], ["2", "web", "upgrade", "waiting"])

        with contextlib.redirect_stdout(io.StringIO()) as out:
            manager.list_jobs(self.settings, "old")
        self.assertEqual(out.getvalue(), "No jobs.\n")

    def test_commands(self):
        self.write_config({"url": self.settings["url"], "key": MANAGER})

        def run_main(*args):
            with unittest.mock.patch.object(sys, "argv", ["hd-manager"] + list(args)), \
                    contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()) as err:
                return manager.main(), out.getvalue(), err.getvalue()

        status, out, _ = run_main("schedule", "upgrade", "box")
        self.assertEqual(status, 0)
        self.assertTrue(out.startswith("Stored job 1: upgrade on box, from "))

        status, _, err = run_main("schedule", "upgrade", "old")
        self.assertEqual((status, err), (1, "Error: old still runs hd-report, which runs no jobs.\n"))

        status, out, _ = run_main("jobs")
        self.assertEqual(status, 0)
        self.assertIn("box", out)

        status, out, _ = run_main("key")
        self.assertEqual((status, out), (0, MANAGER_KEY + "\n"))


if __name__ == '__main__':
    unittest.main()
