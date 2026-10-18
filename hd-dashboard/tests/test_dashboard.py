# Tests for hd-dashboard, the hyprdebian dashboard: accepting reports, judging hosts, the pages and
# the settings.

import importlib.machinery
import importlib.util
import json
import os
import sys
import tempfile
import threading
import unittest
import unittest.mock
import urllib.error
import urllib.request

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HD_DASHBOARD = os.path.join(REPO, 'hd-dashboard', 'hd-dashboard')

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

DEFAULTS = {"listen": "0.0.0.0", "port": 80, "stale_minutes": 45, "history_days": 30}


def healthy(host="box", version="1.3"):
    """A report of a host without issues."""
    return {
        "format": 1,
        "host": host,
        "fqdn": host + ".example.net",
        "hyprdebian": {"version": version, "suite": "unstable"},
        "os": {"debian": "forky/sid", "kernel": "7.2.10", "kernel_newest": "7.2.10", "boot_time": NOW - 3600},
        "load": [0.5, 0.4, 0.3],
        "cpus": 4,
        "memory": {"total": 1000, "used": 250},
        "swap": {"total": 0, "used": 0},
        "pools": [{"name": "rpool", "health": "ONLINE", "size": 1000, "alloc": 300, "capacity": 30, "scan": ""}],
        "arrays": [],
        "units": {"state": "running", "failed": []},
        "updates": {"upgradable": 0, "lists_time": NOW, "last_apt": NOW, "last_upgrade": NOW - 86400,
                    "reboot_required": False},
        "snapshot": {"name": "hd-up-2026-10-01_100000", "time": NOW - 86400},
        "timers": [{"unit": "backup-vms.timer", "service": "backup-vms.service", "last": NOW - 3600,
                    "next": NOW + 3600, "result": "success", "state": "inactive"}],
    }


def post(db, report, token=TOKEN, now=NOW):
    return dash.accept_report(db, json.dumps(report).encode(), "Bearer " + token, "10.0.0.2", now, 30)


class TestAccept(unittest.TestCase):
    def setUp(self):
        self.db = dash.connect(":memory:")

    def test_first_report_registers_the_host_and_its_token(self):
        self.assertEqual(post(self.db, healthy()), "box")

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
        return dash.assess(report, last_seen, NOW, 45, dashboard_version)

    def test_a_healthy_host_has_no_issues(self):
        self.assertEqual(self.messages(healthy()), [])

    def test_problems_are_found(self):
        report = healthy(version="1.2")
        report["units"] = {"state": "degraded", "failed": ["zfs-zed.service"]}
        report["pools"][0].update(health="DEGRADED", capacity=92)
        report["arrays"] = [{"name": "swap", "state": "clean", "degraded": 1}]
        report["timers"][0]["result"] = "exit-code"
        report["os"]["kernel_newest"] = "7.3.0"
        report["updates"].update(upgradable=3, last_upgrade=NOW - 20 * 86400)

        self.assertEqual(self.messages(report, last_seen=NOW - 3600), [
            ("bad", "No report for 60 min"),
            ("bad", "1 failed: zfs-zed.service"),
            ("bad", "Pool rpool is DEGRADED"),
            ("bad", "Pool rpool is 92% full"),
            ("bad", "Array swap is degraded"),
            ("bad", "backup-vms.service failed: exit-code"),
            ("warn", "Reboot pending"),
            ("warn", "Last upgrade 20 days ago"),
            ("info", "3 upgradable packages"),
            ("warn", "HyprDebian 1.2, dashboard is 1.3"),
        ])

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
            issues = {host["name"]: host["issues"] for host in dash.load_hosts(db, NOW, 45)}

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
            self.assertEqual([host["name"] for host in dash.load_hosts(db, NOW, 45)], ["beta", "alpha"])


class TestPages(unittest.TestCase):
    def setUp(self):
        self.db = dash.connect(":memory:")

    def test_report_values_are_escaped(self):
        report = healthy()
        report["units"]["failed"] = ["<script>alert(1)</script>"]
        post(self.db, report)

        for body in [dash.overview_page(self.db, NOW, 45), dash.host_page(self.db, "box", NOW, 45, 30)]:
            self.assertNotIn("<script>", body)
            self.assertIn("&lt;script&gt;", body)

    def test_unknown_host_has_no_page(self):
        self.assertIsNone(dash.host_page(self.db, "nope", NOW, 45, 30))

    def test_empty_dashboard(self):
        self.assertIn("No reports yet", dash.overview_page(self.db, NOW, 45))


class TestConfig(unittest.TestCase):
    def test_the_defaults_of_the_package(self):
        with open(os.path.join(REPO, 'hd-dashboard', 'defaults.json')) as f:
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
        ]

        for config in cases:
            with self.subTest(config=config):
                with self.assertRaises(dash.ConfigError):
                    dash.parse_config(config, DEFAULTS)


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

    def test_forget_removes_the_host_and_its_history(self):
        db = dash.connect(self.database)
        post(db, healthy())
        db.close()

        self.assertEqual(self.run_main("forget", "box"), 0)
        self.assertEqual(self.run_main("forget", "box"), 1)

        db = dash.connect(self.database)
        self.assertEqual(db.execute("SELECT COUNT(*) FROM hosts").fetchone()[0], 0)
        self.assertEqual(db.execute("SELECT COUNT(*) FROM history").fetchone()[0], 0)

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
