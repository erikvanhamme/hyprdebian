# Tests for hd-up, which upgrades the system after a ZFS snapshot, on the parts that read the
# simulation of apt-get, the configuration and the list of snapshots.

import contextlib
import importlib.machinery
import importlib.util
import io
import os
import unittest

HD_UP = os.path.join(os.path.dirname(__file__), '..', 'hd-up')


def load_hd_up():
    # hd-up has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_up', HD_UP)
    spec = importlib.util.spec_from_loader('hd_up', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


up = load_hd_up()

SIMULATION = """NOTE: This is only a simulation!
Reading package lists...
The following packages will be upgraded:
  bash libc6
Remv old-tool [1.0-1]
Inst libc6 [2.41-1] (2.41-2 Debian:13 [amd64])
Inst bash [5.2-1] (5.2-2 Debian:13 [amd64])
Inst linux-image-6.12.9 (6.12.9-1 Debian:13 [amd64])
Conf libc6 (2.41-2 Debian:13 [amd64])
Conf bash (5.2-2 Debian:13 [amd64])
"""


class TestSimulation(unittest.TestCase):
    def test_installs_and_removals(self):
        self.assertEqual(up.parse_simulation(SIMULATION),
                         (["libc6", "bash", "linux-image-6.12.9"], ["old-tool"]))

    def test_nothing_to_do(self):
        self.assertEqual(up.parse_simulation("Reading package lists...\n0 upgraded\n"), ([], []))


class TestKeep(unittest.TestCase):
    def keep(self, config):
        with contextlib.redirect_stderr(io.StringIO()):
            return up.keep_count(config)

    def test_default(self):
        self.assertEqual(self.keep({}), up.KEEP_DEFAULT)
        self.assertEqual(self.keep({"hd-up": {}}), up.KEEP_DEFAULT)

    def test_configured(self):
        self.assertEqual(self.keep({"hd-up": {"keep": 3}}), 3)

    def test_invalid_falls_back(self):
        for value in [0, -1, "4", 2.5, True, None]:
            self.assertEqual(self.keep({"hd-up": {"keep": value}}), up.KEEP_DEFAULT)
        self.assertEqual(self.keep({"hd-up": 5}), up.KEEP_DEFAULT)


LISTING = "\n".join([
    "rpool/hyprdebian@install\t100",
    "rpool/hyprdebian@hd-up-2026-10-20_043000\t400",
    "rpool/hyprdebian@hd-up-2026-10-06_043000\t200",
    "rpool/hyprdebian@before-test\t250",
    "rpool/hyprdebian@hd-up-test\t260",
    "rpool/hyprdebian@hd-up-2026-10-13_043000\t300",
]) + "\n"


class TestPrune(unittest.TestCase):
    def test_oldest_beyond_keep(self):
        self.assertEqual(up.snapshots_to_prune(LISTING, 1), [
            "rpool/hyprdebian@hd-up-2026-10-06_043000",
            "rpool/hyprdebian@hd-up-2026-10-13_043000",
        ])

    def test_other_snapshots_stay(self):
        self.assertEqual(up.snapshots_to_prune(LISTING, 2),
                         ["rpool/hyprdebian@hd-up-2026-10-06_043000"])
        self.assertEqual(up.snapshots_to_prune(LISTING, 3), [])
        self.assertEqual(up.snapshots_to_prune("", 1), [])


class TestAptOptions(unittest.TestCase):
    def test_unattended_keeps_conffiles(self):
        self.assertIn("Dpkg::Options::=--force-confold", up.apt_options(True))
        self.assertNotIn("Dpkg::Options::=--force-confold", up.apt_options(False))


if __name__ == '__main__':
    unittest.main()
