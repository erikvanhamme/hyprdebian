# Tests for hd-report, the status reports of the package hd-report: parsing what the system tools
# print, the settings, and sending a report.

import gzip
import http.server
import importlib.machinery
import importlib.util
import json
import os
import tempfile
import threading
import time
import unittest

PACKAGES_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HD_REPORT = os.path.join(PACKAGES_DIR, 'hd-report', 'hd-report')


def load_hd_report():
    # hd-report has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_report', HD_REPORT)
    spec = importlib.util.spec_from_loader('hd_report', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


report = load_hd_report()


class TestParsing(unittest.TestCase):
    def test_version_and_suite(self):
        with tempfile.TemporaryDirectory() as tmp:
            version = os.path.join(tmp, "version.json")
            with open(version, "w") as f:
                json.dump({"major": 1, "minor": 1}, f)

            self.assertEqual(report.hyprdebian_version(version), "1.1")
            self.assertEqual(report.hyprdebian_version(os.path.join(tmp, "missing")), "")

        sources = "Types: deb\nURIs: http://repo.example/\nSuites: trixie\nComponents: main\n"
        self.assertEqual(report.parse_suite(sources), "trixie")
        self.assertEqual(report.parse_suite(None), "")

    def test_meminfo_gives_used_memory_in_bytes(self):
        memory = report.parse_meminfo("MemTotal: 1000 kB\nMemAvailable: 400 kB\n"
                                      "SwapTotal: 200 kB\nSwapFree: 150 kB\n")

        self.assertEqual(memory, {"total": 1024000, "used": 614400})

    def test_pools_get_their_last_completed_scrub(self):
        listing = "bpool\tONLINE\t1000\t25%\nrpool\tDEGRADED\t2000\t90\n"
        status = ("  pool: bpool\n state: ONLINE\n"
                  "  scan: scrub repaired 1.50M in 00:12:01 with 2 errors on Sun Oct 11 00:36:02 2026\n"
                  "  pool: rpool\n state: DEGRADED\n  scan: resilvered 1.2G in 00:10:00 with 0 errors on Sun Oct 11 00:30:00 2026\n")

        self.assertEqual(report.parse_pools(listing, status), [
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
                self.assertEqual(report.parse_scrubs(f"  pool: rpool\n  scan: {line}\n"), {})

    def test_sizes_of_zpool(self):
        self.assertEqual([report.parse_size(text) for text in ["0B", "512B", "12K", "1.50M", "2048", "x"]],
                         [0, 512, 12288, 1572864, 2048, None])

    def test_no_zpool_gives_no_pools(self):
        self.assertEqual(report.parse_pools(None, None), [])

    def test_failed_units(self):
        text = "zfs-zed.service loaded failed failed ZFS Event Daemon\nfoo.mount loaded failed failed Foo\n"

        self.assertEqual(report.parse_failed_units(text), ["zfs-zed.service", "foo.mount"])
        self.assertEqual(report.parse_failed_units(""), [])

    def test_history_finds_the_last_upgrade(self):
        text = ("Start-Date: 2026-10-01  10:00:00\nCommandline: apt full-upgrade -y\nEnd-Date: 2026-10-01  10:05:00\n\n"
                "Start-Date: 2026-10-02  11:00:00\nCommandline: apt install foo\nEnd-Date: 2026-10-02  11:01:00\n")

        self.assertEqual(report.parse_history(text), int(time.mktime((2026, 10, 1, 10, 5, 0, 0, 0, -1))))
        self.assertIsNone(report.parse_history("Commandline: apt install foo\nEnd-Date: 2026-10-02  11:01:00\n"))

    def test_upgrade_is_found_in_a_rotated_history_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "history.log")
            with open(path, "w") as f:
                f.write("Commandline: apt install foo\nEnd-Date: 2026-10-02  11:01:00\n")
            for number, day in [(1, 20), (2, 10)]:
                with gzip.open(f"{path}.{number}.gz", "wt") as f:
                    f.write(f"Commandline: apt upgrade\nEnd-Date: 2026-09-{day}  09:00:00\n")

            logs = report.read_history(path)

        self.assertEqual(report.last_upgrade(logs),
                         int(time.mktime((2026, 9, 20, 9, 0, 0, 0, 0, -1))))

    def test_newest_kernel_is_modified_last(self):
        # Two builds of hd-kernel of the same linux: the hash of the newer one sorts first.
        with tempfile.TemporaryDirectory() as tmp:
            for when, version in [(1000, "7.2.13+deb14-amd64"), (2000, "7.2.12+deb14-hdf00000-amd64"),
                                  (3000, "7.2.12+deb14-hd000000-amd64")]:
                path = os.path.join(tmp, "vmlinuz-" + version)
                open(path, "w").close()
                os.utime(path, (when, when))

            self.assertEqual(report.newest_kernel(tmp), "7.2.12+deb14-hd000000-amd64")

        with tempfile.TemporaryDirectory() as tmp:
            self.assertIsNone(report.newest_kernel(tmp))

    def test_only_snapshots_of_hd_up_count(self):
        text = ("rpool/hyprdebian@install\t100\nrpool/hyprdebian@hd-up-2026-10-01_100000\t200\n"
                "rpool/hyprdebian@hd-up-2026-10-02_100000\t300\nrpool/hyprdebian@manual\t400\n")

        self.assertEqual(report.parse_snapshots(text), {"name": "hd-up-2026-10-02_100000", "time": 300})
        self.assertIsNone(report.parse_snapshots(None))

    def test_systemd_timestamps(self):
        properties = report.parse_properties("LastTriggerUSec=@1792114200\nNextElapseUSecRealtime=\n")

        self.assertEqual(report.unix_time(properties["LastTriggerUSec"]), 1792114200)
        self.assertIsNone(report.unix_time(properties["NextElapseUSecRealtime"]))

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

            arrays = report.collect_arrays(os.path.join(tmp, "block"), os.path.join(tmp, "dev", "md"))

        self.assertEqual(arrays, [{"name": "swap", "state": "clean", "degraded": 1}])

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
                    with open(os.path.join(tmp, f"{name}-{report.GLOBAL_GUID}"), "wb") as f:
                        f.write(b"\x06\x00\x00\x00" + value)

                self.assertEqual(report.secure_boot_state(tmp), state)

        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(report.secure_boot_state(os.path.join(tmp, "missing")), "legacy")


class TestSettings(unittest.TestCase):
    def test_the_url_of_the_settings_overrides_the_default(self):
        with open(os.path.join(PACKAGES_DIR, 'hd-report', 'defaults.json')) as f:
            defaults = json.load(f)

        self.assertEqual(report.dashboard_url(None, defaults), defaults["url"])
        self.assertEqual(report.dashboard_url({"hd-report": {"url": "http://dash.example/"}}, defaults),
                         "http://dash.example/")

    def test_a_wrong_url_is_refused(self):
        for url in ["", "dash.example", "ftp://dash.example/", 5]:
            with self.subTest(url=url):
                self.assertIsNone(report.dashboard_url({"hd-report": {"url": url}}, {}))

        self.assertIsNone(report.dashboard_url(None, None))


class TestCollect(unittest.TestCase):
    def test_the_report_of_this_system_is_json(self):
        collected = report.collect()

        self.assertEqual(collected["format"], report.FORMAT)
        self.assertTrue(collected["host"])
        json.dumps(collected)

    def test_hd_dashboard_accepts_the_report(self):
        loader = importlib.machinery.SourceFileLoader('hd_dashboard', os.path.join(PACKAGES_DIR, 'hd-dashboard', 'hd-dashboard'))
        spec = importlib.util.spec_from_loader('hd_dashboard', loader)
        dashboard = importlib.util.module_from_spec(spec)
        loader.exec_module(dashboard)

        collected = report.collect()
        db = dashboard.connect(":memory:")
        host = dashboard.accept_report(db, json.dumps(collected).encode(), "Bearer " + "a" * 64,
                                       "127.0.0.1", int(time.time()), 30)

        self.assertEqual(host, collected["host"])
        self.assertEqual(report.FORMAT, dashboard.FORMAT)


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

        report.send({"host": "box"}, f"http://127.0.0.1:{server.server_port}/", "abc")

        thread.join()
        server.server_close()
        self.assertEqual(received, {"path": "/report", "authorization": "Bearer abc", "body": {"host": "box"}})


if __name__ == '__main__':
    unittest.main()
