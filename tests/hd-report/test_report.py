# Tests for hd-report: parsing what the system tools print, and sending a report.

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

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HD_REPORT = os.path.join(REPO, 'deploy', 'usr', 'local', 'bin', 'hd-report')


def load_hd_report():
    # hd-report has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_report', HD_REPORT)
    spec = importlib.util.spec_from_loader('hd_report', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


report = load_hd_report()


class TestParsing(unittest.TestCase):
    def test_config_values_are_unquoted_like_bash_quotes_them(self):
        config = report.parse_config("Q_FULLNAME=Erik\\ Van\\ Hamme\nQ_TYPE=desktop\n"
                                     "declare -gA TASK_DEPS=([a]=b)\nEMPTY=''\n")

        self.assertEqual(config, {"Q_FULLNAME": "Erik Van Hamme", "Q_TYPE": "desktop", "EMPTY": ""})

    def test_meminfo_gives_used_memory_and_swap_in_bytes(self):
        memory, swap = report.parse_meminfo("MemTotal: 1000 kB\nMemAvailable: 400 kB\n"
                                            "SwapTotal: 200 kB\nSwapFree: 150 kB\n")

        self.assertEqual(memory, {"total": 1024000, "used": 614400})
        self.assertEqual(swap, {"total": 204800, "used": 51200})

    def test_pools_get_the_scan_line_of_zpool_status(self):
        listing = "bpool\tONLINE\t1000\t250\t25%\nrpool\tDEGRADED\t2000\t1800\t90\n"
        status = ("  pool: bpool\n state: ONLINE\n  scan: scrub repaired 0B with 0 errors on Sun\n"
                  "  pool: rpool\n state: DEGRADED\n")

        pools = report.parse_pools(listing, status)

        self.assertEqual(pools[0], {"name": "bpool", "health": "ONLINE", "size": 1000, "alloc": 250,
                                    "capacity": 25, "scan": "scrub repaired 0B with 0 errors on Sun"})
        self.assertEqual((pools[1]["health"], pools[1]["capacity"], pools[1]["scan"]), ("DEGRADED", 90, ""))

    def test_no_zpool_gives_no_pools(self):
        self.assertEqual(report.parse_pools(None, None), [])

    def test_failed_units(self):
        text = "zfs-zed.service loaded failed failed ZFS Event Daemon\nfoo.mount loaded failed failed Foo\n"

        self.assertEqual(report.parse_failed_units(text), ["zfs-zed.service", "foo.mount"])
        self.assertEqual(report.parse_failed_units(""), [])

    def test_history_finds_the_last_run_and_the_last_upgrade(self):
        text = ("Start-Date: 2026-10-01  10:00:00\nCommandline: apt full-upgrade -y\nEnd-Date: 2026-10-01  10:05:00\n\n"
                "Start-Date: 2026-10-02  11:00:00\nCommandline: apt install foo\nEnd-Date: 2026-10-02  11:01:00\n")

        upgrade = int(time.mktime((2026, 10, 1, 10, 5, 0, 0, 0, -1)))
        install = int(time.mktime((2026, 10, 2, 11, 1, 0, 0, 0, -1)))
        self.assertEqual(report.parse_history(text, upgrades_only=True), upgrade)
        self.assertEqual(report.parse_history(text, upgrades_only=False), install)

    def test_upgrade_is_found_in_a_rotated_history_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "history.log")
            with open(path, "w") as f:
                f.write("Commandline: apt install foo\nEnd-Date: 2026-10-02  11:01:00\n")
            for number, day in [(1, 20), (2, 10)]:
                with gzip.open(f"{path}.{number}.gz", "wt") as f:
                    f.write(f"Commandline: apt upgrade\nEnd-Date: 2026-09-{day}  09:00:00\n")

            logs = report.read_history(path)

        self.assertEqual(report.last_in_history(logs, upgrades_only=True),
                         int(time.mktime((2026, 9, 20, 9, 0, 0, 0, 0, -1))))

    def test_newest_kernel_compares_versions_by_number(self):
        with tempfile.TemporaryDirectory() as tmp:
            for version in ["6.9.12+deb14-amd64", "6.12.1+deb14-amd64", "6.10.3+deb14-amd64"]:
                open(os.path.join(tmp, "vmlinuz-" + version), "w").close()

            self.assertEqual(report.newest_kernel(tmp), "6.12.1+deb14-amd64")

        with tempfile.TemporaryDirectory() as tmp:
            self.assertIsNone(report.newest_kernel(tmp))

    def test_only_snapshots_of_hd_up_and_upgrade_count(self):
        text = ("rpool/hyprdebian@install\t100\nrpool/hyprdebian@hd-up-2026-10-01_100000\t200\n"
                "rpool/hyprdebian@upgrade-2026-10-02_100000\t300\nrpool/hyprdebian@manual\t400\n")

        self.assertEqual(report.parse_snapshots(text), {"name": "upgrade-2026-10-02_100000", "time": 300})
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


class TestCollect(unittest.TestCase):
    def test_the_report_of_this_system_is_json(self):
        collected = report.collect()

        self.assertEqual(collected["format"], report.FORMAT)
        self.assertTrue(collected["host"])
        json.dumps(collected)


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
