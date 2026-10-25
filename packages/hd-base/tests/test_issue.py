# Tests for hd-issue, which writes the login banner, on the part that finds the suite: that of the
# HyprDebian packages, or else the first one of Debian.

import importlib.machinery
import importlib.util
import os
import tempfile
import unittest

HD_ISSUE = os.path.join(os.path.dirname(__file__), '..', 'hd-issue')


def load_hd_issue():
    # hd-issue has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_issue', HD_ISSUE)
    spec = importlib.util.spec_from_loader('hd_issue', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


issue = load_hd_issue()

HYPRDEBIAN = """Types: deb
URIs: http://repo.example/
Suites: trixie
Components: main
"""

DEBIAN = """Types: deb
URIs: http://deb.debian.org/debian/
Suites: unstable
Components: main contrib non-free non-free-firmware

Types: deb
URIs: http://security.debian.org/debian-security/
Suites: unstable-security
Components: main
"""


class TestSuite(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.sources = os.path.join(self.dir.name, 'hyprdebian.sources')
        self.debian_sources = os.path.join(self.dir.name, 'debian.sources')
        self.saved = (issue.SOURCES, issue.DEBIAN_SOURCES)
        issue.SOURCES = self.sources
        issue.DEBIAN_SOURCES = self.debian_sources

    def tearDown(self):
        issue.SOURCES, issue.DEBIAN_SOURCES = self.saved
        self.dir.cleanup()

    def write(self, path, text):
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)

    def test_hyprdebian_source(self):
        self.write(self.sources, HYPRDEBIAN)
        self.write(self.debian_sources, DEBIAN)
        self.assertEqual(issue.suite(), 'trixie')

    def test_debian_source(self):
        self.write(self.debian_sources, DEBIAN)
        self.assertEqual(issue.suite(), 'unstable')

    def test_hyprdebian_source_without_suites(self):
        self.write(self.sources, "Types: deb\n")
        self.write(self.debian_sources, DEBIAN)
        self.assertEqual(issue.suite(), 'unstable')

    def test_no_source(self):
        self.assertEqual(issue.suite(), 'unknown')


if __name__ == '__main__':
    unittest.main()
