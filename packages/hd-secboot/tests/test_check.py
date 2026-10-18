# Tests for hd-secboot-check, which checks that Secure Boot is on, on EFI variables in a temporary
# directory.

import importlib.machinery
import importlib.util
import os
import tempfile
import unittest

HD_SECBOOT_CHECK = os.path.join(os.path.dirname(__file__), '..', 'hd-secboot-check')


def load_hd_secboot_check():
    # hd-secboot-check has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_secboot_check', HD_SECBOOT_CHECK)
    spec = importlib.util.spec_from_loader('hd_secboot_check', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


check = load_hd_secboot_check()


class TestState(unittest.TestCase):
    def efivars(self, tmp, **variables):
        """Writes EFI variables with 4 bytes of attributes in front of their value."""
        for name, value in variables.items():
            with open(os.path.join(tmp, f"{name}-{check.GLOBAL_GUID}"), "wb") as f:
                f.write(b"\x06\x00\x00\x00" + value)
        return tmp

    def test_states(self):
        cases = [
            ({"SecureBoot": b"\x01", "SetupMode": b"\x00"}, "on"),
            ({"SecureBoot": b"\x00", "SetupMode": b"\x00"}, "off"),
            ({"SecureBoot": b"\x00", "SetupMode": b"\x01"}, "setup"),
            ({}, "unsupported"),
        ]

        for variables, state in cases:
            with self.subTest(state=state), tempfile.TemporaryDirectory() as tmp:
                self.assertEqual(check.secure_boot_state(self.efivars(tmp, **variables)), state)

    def test_without_efivars_the_system_booted_without_uefi(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(check.secure_boot_state(os.path.join(tmp, "missing")), "legacy")

    def test_every_state_but_on_has_a_message(self):
        self.assertEqual(set(check.MESSAGES), {"off", "setup", "unsupported", "legacy"})


if __name__ == '__main__':
    unittest.main()
