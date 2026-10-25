# Tests for hd-secboot: the state --status finds in EFI variables in a temporary directory, the
# EFI signature lists it reads and writes, the option ROMs it finds in a TPM event log, the names
# of their devices, and the hashes picked for --append-rom-hash.

import hashlib
import importlib.machinery
import importlib.util
import os
import struct
import tempfile
import unittest
import uuid

HD_SECBOOT = os.path.join(os.path.dirname(__file__), '..', 'hd-secboot')


def load_hd_secboot():
    # hd-secboot has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_secboot', HD_SECBOOT)
    spec = importlib.util.spec_from_loader('hd_secboot', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


secboot = load_hd_secboot()

OWNER = uuid.UUID("3f1ecfcb-32dc-4d4a-9887-a250f17096d8")


def node(node_type, subtype, data):
    return struct.pack("<BBH", node_type, subtype, 4 + len(data)) + data


def pci_path(*devices, rom=True):
    """A device path below PciRoot(0x0) through PCI nodes (device, function), ending in the offset
    of an option ROM or a file."""
    path = node(2, 1, struct.pack("<II", 0x0A0341D0, 0))
    for device, function in devices:
        path += node(1, 1, bytes([function, device]))
    if rom:
        path += node(4, 8, struct.pack("<IQQ", 0, 0x1000, 0x2000))
    else:
        path += node(4, 4, "driver.efi\0".encode("utf-16-le"))
    return path + node(0x7F, 0xFF, b"")


def event_log(*events):
    """A crypto-agile TPM event log with SHA-1 and SHA-256 digests, of (type, sha256, path)."""
    spec = (b"Spec ID Event03\0" + struct.pack("<IBBBBI", 0, 0, 2, 0, 2, 2)
            + struct.pack("<HHHH", 0x0004, 20, 0x000B, 32) + b"\0")
    log = struct.pack("<II", 0, 3) + b"\0" * 20 + struct.pack("<I", len(spec)) + spec
    for event_type, digest, path in events:
        data = struct.pack("<QQQQ", 0, 0, 0, len(path)) + path
        log += (struct.pack("<III", 2, event_type, 2) + struct.pack("<H", 0x0004) + b"\1" * 20
                + struct.pack("<H", 0x000B) + digest + struct.pack("<I", len(data)) + data)
    return log


class TestState(unittest.TestCase):
    def efivars(self, tmp, **variables):
        """Writes EFI variables with 4 bytes of attributes in front of their value."""
        for name, value in variables.items():
            with open(os.path.join(tmp, f"{name}-{secboot.GLOBAL_GUID}"), "wb") as f:
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
                self.assertEqual(secboot.secure_boot_state(self.efivars(tmp, **variables)), state)

    def test_without_efivars_the_system_booted_without_uefi(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(secboot.secure_boot_state(os.path.join(tmp, "missing")), "legacy")

    def test_every_state_but_on_has_a_message(self):
        self.assertEqual(set(secboot.MESSAGES), {"off", "setup", "unsupported", "legacy"})


class TestSignatureLists(unittest.TestCase):
    CERTIFICATE = b"\x30\x82\x01\x0a" + bytes(range(64))

    def certificate_list(self, certificate):
        size = 16 + len(certificate)
        return (struct.pack("<16sIII", secboot.EFI_CERT_X509_GUID.bytes_le, 28 + size, 0, size)
                + OWNER.bytes_le + certificate)

    def test_hashes_are_read_back(self):
        hashes = [hashlib.sha256(b"a").digest(), hashlib.sha256(b"b").digest()]
        entries = secboot.parse_signature_lists(secboot.signature_list(hashes, OWNER))
        self.assertEqual(entries, [(secboot.EFI_CERT_SHA256_GUID, digest) for digest in hashes])

    def test_lists_follow_each_other(self):
        digest = hashlib.sha256(b"a").digest()
        value = (self.certificate_list(self.CERTIFICATE)
                 + secboot.signature_list([digest], OWNER))
        self.assertEqual(secboot.parse_signature_lists(value), [
            (secboot.EFI_CERT_X509_GUID, self.CERTIFICATE),
            (secboot.EFI_CERT_SHA256_GUID, digest),
        ])

    def test_a_variable_holds_a_certificate(self):
        self.assertTrue(secboot.has_certificate(self.certificate_list(self.CERTIFICATE),
                                                self.CERTIFICATE))

    def test_another_certificate_is_not_it(self):
        self.assertFalse(secboot.has_certificate(self.certificate_list(b"\x30" * 70),
                                                 self.CERTIFICATE))

    def test_a_missing_variable_holds_nothing(self):
        self.assertEqual(secboot.parse_signature_lists(None), [])
        self.assertFalse(secboot.has_certificate(None, self.CERTIFICATE))


class TestOptionRoms(unittest.TestCase):
    GPU = hashlib.sha256(b"gop").digest()
    NIC = hashlib.sha256(b"flexboot").digest()

    def test_option_roms_are_grouped_by_hash(self):
        log = event_log(
            (0x80000004, self.GPU, pci_path((1, 1), (0, 0))),
            (0x80000004, self.NIC, pci_path((2, 1), (0, 0))),
            (0x80000004, self.NIC, pci_path((2, 1), (0, 1))),
        )
        roms = secboot.option_roms(log)
        self.assertEqual(list(roms), [self.GPU, self.NIC])
        self.assertEqual(len(roms[self.NIC]), 2)

    def test_drivers_from_files_and_other_events_are_left_out(self):
        log = event_log(
            (0x80000004, self.GPU, pci_path((1, 1), rom=False)),
            (0x80000003, self.NIC, pci_path((2, 1))),
        )
        self.assertEqual(secboot.option_roms(log), {})

    def test_the_device_path_leads_to_the_pci_address(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, "pci0000:00", "0000:00:01.1", "0000:01:00.0",
                                     "0000:02:00.0", "0000:03:00.0"))
            nodes = secboot.device_path_nodes(pci_path((1, 1), (0, 0), (0, 0), (0, 0)))
            self.assertEqual(secboot.pci_address(nodes, tmp), "0000:03:00.0")
            nodes = secboot.device_path_nodes(pci_path((5, 0)))
            self.assertIsNone(secboot.pci_address(nodes, tmp))


class TestDeviceName(unittest.TestCase):
    PCI_IDS = ("# comment\n"
               "1002  Advanced Micro Devices, Inc. [AMD/ATI]\n"
               "\t7550  Navi 48 [Radeon RX 9070/9070 XT/9070 GRE]\n"
               "\t\t1eae 8811  Some board\n"
               "15b3  Mellanox Technologies\n"
               "\t1015  MT27710 Family [ConnectX-4 Lx]\n")

    def device(self, tmp, address, vendor, device, driver=None):
        directory = os.path.join(tmp, "devices", address)
        os.makedirs(directory)
        for name, value in (("vendor", vendor), ("device", device)):
            with open(os.path.join(directory, name), "w") as f:
                f.write(f"0x{value}\n")
        if driver is not None:
            os.symlink(f"../../bus/pci/drivers/{driver}", os.path.join(directory, "driver"))

    def test_names_come_from_pci_ids_with_the_driver(self):
        with tempfile.TemporaryDirectory() as tmp:
            ids = os.path.join(tmp, "pci.ids")
            with open(ids, "w") as f:
                f.write(self.PCI_IDS)
            self.device(tmp, "0000:03:00.0", "1002", "7550", "amdgpu")
            self.device(tmp, "0000:11:00.0", "15b3", "1015")
            devices = os.path.join(tmp, "devices")
            self.assertEqual(secboot.device_name("0000:03:00.0", devices, [ids]),
                             "0000:03:00.0 [1002:7550] Advanced Micro Devices, Inc. [AMD/ATI] "
                             "Navi 48 [Radeon RX 9070/9070 XT/9070 GRE] (driver amdgpu)")
            self.assertEqual(secboot.device_name("0000:11:00.0", devices, [ids]),
                             "0000:11:00.0 [15b3:1015] Mellanox Technologies "
                             "MT27710 Family [ConnectX-4 Lx]")

    def test_without_pci_ids_the_ids_are_left(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.device(tmp, "0000:03:00.0", "1002", "7550")
            self.assertEqual(secboot.device_name("0000:03:00.0", os.path.join(tmp, "devices"),
                                                 [os.path.join(tmp, "missing")]),
                             "0000:03:00.0 [1002:7550]")


class TestSelection(unittest.TestCase):
    def test_answers(self):
        cases = [
            ("1", [1]),
            ("1 3", [1, 3]),
            ("3,1", [3, 1]),
            ("2 2", [2]),
            ("all", [1, 2, 3]),
            ("", []),
            ("0", None),
            ("4", None),
            ("one", None),
        ]
        for answer, picked in cases:
            with self.subTest(answer=answer):
                self.assertEqual(secboot.parse_selection(answer, 3), picked)


if __name__ == '__main__':
    unittest.main()
