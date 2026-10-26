# Tests for hd-reboot-check, which tells whether the system needs a reboot, on the parts that
# compare the kernels and the ZFS modules and read /run/reboot-required.pkgs.

import importlib.machinery
import importlib.util
import os
import tempfile
import unittest

HD_REBOOT_CHECK = os.path.join(os.path.dirname(__file__), '..', 'hd-reboot-check')


def load_hd_reboot_check():
    # hd-reboot-check has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_reboot_check', HD_REBOOT_CHECK)
    spec = importlib.util.spec_from_loader('hd_reboot_check', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


check = load_hd_reboot_check()


class TestReasons(unittest.TestCase):
    def test_nothing_new(self):
        self.assertEqual(check.reasons("7.2.12-amd64", "7.2.12-amd64", "2.3.4-1", "2.3.4-1"), [])

    def test_newer_kernel(self):
        self.assertEqual(check.reasons("7.2.12-amd64", "7.2.13-amd64", "2.3.4-1", "2.3.4-1"), [
            ("linux-image-7.2.13-amd64", "new kernel 7.2.13-amd64"),
        ])

    def test_new_zfs_module(self):
        self.assertEqual(check.reasons("7.2.12-amd64", "7.2.12-amd64", "2.3.4-1", "2.3.5-1"), [
            ("zfs-dkms", "new ZFS module 2.3.5-1"),
        ])

    def test_no_zfs_or_no_kernels(self):
        self.assertEqual(check.reasons("7.2.12-amd64", None, None, "2.3.5-1"), [])
        self.assertEqual(check.reasons("7.2.12-amd64", None, "2.3.4-1", None), [])


class TestNewestKernel(unittest.TestCase):
    def test_modified_last(self):
        # Two builds of hd-kernel of the same linux: the hash of the newer one sorts first.
        with tempfile.TemporaryDirectory() as boot:
            for when, version in [(1000, "7.2.13+deb14-amd64"), (2000, "7.2.12+deb14-hdf00000-amd64"),
                                  (3000, "7.2.12+deb14-hd000000-amd64")]:
                path = os.path.join(boot, f"vmlinuz-{version}")
                open(path, "w").close()
                os.utime(path, (when, when))
            open(os.path.join(boot, "config-7.11.0+deb14-amd64"), "w").close()
            self.assertEqual(check.newest_kernel(boot), "7.2.12+deb14-hd000000-amd64")

    def test_no_kernels(self):
        with tempfile.TemporaryDirectory() as boot:
            self.assertIsNone(check.newest_kernel(boot))


class TestRequested(unittest.TestCase):
    def test_packages_once_each(self):
        self.assertEqual(check.requested("libc6\nsystemd\nlibc6\n\n"),
                         "requested by libc6, systemd")

    def test_no_packages(self):
        self.assertEqual(check.requested(None), "requested by a package")
        self.assertEqual(check.requested(""), "requested by a package")


if __name__ == '__main__':
    unittest.main()
