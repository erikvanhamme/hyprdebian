# Tests for hd-firmware, which finds the firmware packages that the hardware needs, on the parts
# that read the output of modprobe, the Contents indexes of apt and /proc/cpuinfo.

import importlib.machinery
import importlib.util
import os
import tempfile
import unittest

HD_FIRMWARE = os.path.join(os.path.dirname(__file__), '..', 'hd-firmware')


def load_hd_firmware():
    # hd-firmware has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_firmware', HD_FIRMWARE)
    spec = importlib.util.spec_from_loader('hd_firmware', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


firmware = load_hd_firmware()

CONTENTS = [
    "etc/default/raspi-firmware                     non-free-firmware/misc/raspi-firmware\n",
    "usr/lib/firmware/amdgpu/gc_12_0_1_mec.bin      non-free-firmware/kernel/firmware-amd-graphics\n",
    "usr/lib/firmware/rtw89/rtw8852c_fw.bin.zst     non-free-firmware/kernel/firmware-realtek\n",
    "lib/firmware/qed/qed_init_values.bin           non-free-firmware/kernel/firmware-qlogic\n",
    "usr/lib/firmware/ti/shared.bin                 kernel/firmware-misc-nonfree,kernel/firmware-ti\n",
    "usr/lib/firmware/brcm/brcmfmac4356-pcie.bin    non-free-firmware/kernel/firmware-brcm80211\n",
]


class TestModprobe(unittest.TestCase):
    def test_inserted_and_builtin_modules(self):
        output = ("insmod /lib/modules/6.1/kernel/lib/crc/crc16.ko.xz \n"
                  "insmod /lib/modules/6.1/kernel/drivers/gpu/amdgpu.ko.xz \n"
                  "builtin usbcore\n"
                  "something else\n")
        self.assertEqual(firmware.parse_show_depends(output), {
            "/lib/modules/6.1/kernel/lib/crc/crc16.ko.xz",
            "/lib/modules/6.1/kernel/drivers/gpu/amdgpu.ko.xz",
            "usbcore",
        })

    def test_module_names(self):
        self.assertEqual(firmware.module_name("/lib/modules/6.1/kernel/rtw89-8852ce.ko.zst"),
                         "rtw89_8852ce")
        self.assertEqual(firmware.module_name("/lib/modules/6.1/kernel/r8169.ko"), "r8169")
        self.assertEqual(firmware.module_name("usbcore"), "usbcore")


class TestContents(unittest.TestCase):
    def setUp(self):
        self.packages = firmware.parse_contents(CONTENTS)

    def test_only_firmware_without_prefix_and_compression(self):
        self.assertEqual(set(self.packages), {
            "amdgpu/gc_12_0_1_mec.bin",
            "rtw89/rtw8852c_fw.bin",
            "qed/qed_init_values.bin",
            "ti/shared.bin",
            "brcm/brcmfmac4356-pcie.bin",
        })

    def test_a_file_in_several_packages(self):
        self.assertEqual(self.packages["ti/shared.bin"], ["firmware-misc-nonfree", "firmware-ti"])

    def test_lookup(self):
        cases = [
            ("amdgpu/gc_12_0_1_mec.bin", ["firmware-amd-graphics"]),
            ("rtw89/rtw8852c_fw.bin", ["firmware-realtek"]),
            ("amdgpu/gc_12_0_1_mec.bin.zst", ["firmware-amd-graphics"]),
            ("brcm/brcmfmac4356-pcie.*", ["firmware-brcm80211"]),
            ("missing/file.bin", []),
        ]
        for name, packages in cases:
            with self.subTest(name=name):
                self.assertEqual(firmware.packages_for(name, self.packages), packages)


class TestMicrocode(unittest.TestCase):
    def cpuinfo(self, tmp, vendor, flags):
        path = os.path.join(tmp, "cpuinfo")
        with open(path, "w") as f:
            for cpu in range(2):
                f.write(f"processor\t: {cpu}\nvendor_id\t: {vendor}\nflags\t\t: {flags}\n\n")
        return path

    def test_vendor_and_hypervisor(self):
        cases = [
            ("AuthenticAMD", "fpu vme sse2", ("AuthenticAMD", False)),
            ("GenuineIntel", "fpu hypervisor sse2", ("GenuineIntel", True)),
        ]
        for vendor, flags, expected in cases:
            with self.subTest(vendor=vendor), tempfile.TemporaryDirectory() as tmp:
                self.assertEqual(firmware.cpu_info(self.cpuinfo(tmp, vendor, flags)), expected)

    def test_packages(self):
        self.assertEqual(firmware.microcode_package("GenuineIntel", False), "intel-microcode")
        self.assertEqual(firmware.microcode_package("AuthenticAMD", False), "amd64-microcode")
        self.assertIsNone(firmware.microcode_package("AuthenticAMD", True))
        self.assertIsNone(firmware.microcode_package("HygonGenuine", False))

    def test_the_cpu_adds_its_microcode(self):
        reasons = firmware.add_microcode({"amd64-microcode": {"ccp"}}, "AuthenticAMD", False)
        self.assertEqual(reasons, {"amd64-microcode": {"ccp", "CPU AuthenticAMD"}})

    def test_a_virtual_machine_gets_no_microcode_also_not_for_a_driver(self):
        reasons = {
            "amd64-microcode": {"ccp"},
            "intel-microcode": {"something"},
            "firmware-realtek": {"r8169"},
        }
        self.assertEqual(firmware.add_microcode(reasons, "AuthenticAMD", True),
                         {"firmware-realtek": {"r8169"}})


if __name__ == '__main__':
    unittest.main()
