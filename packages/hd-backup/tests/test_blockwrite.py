# Tests for blockwrite, the program that writes the tape stream of hd-backup in 4 MiB blocks. make
# test builds it and passes its path in the environment variable BLOCKWRITE.

import os
import shutil
import subprocess
import tempfile
import unittest

BLOCK = 4 * 1024 * 1024
PROGRAM = os.environ.get('BLOCKWRITE')


@unittest.skipIf(not PROGRAM, 'BLOCKWRITE is not set; run the tests with make test')
class TestBlockwrite(unittest.TestCase):
    def setUp(self):
        # blockwrite opens its target with O_DIRECT, which tmpfs (often /tmp) does not support.
        # /var/tmp is usually on disk.
        self.device_dir = tempfile.mkdtemp(prefix='hd-backup-test-', dir='/var/tmp')
        self.addCleanup(shutil.rmtree, self.device_dir, ignore_errors=True)
        self.device = os.path.join(self.device_dir, 'device')
        open(self.device, 'w').close()
        probe = self.blockwrite(b'')
        if probe.returncode != 0:
            self.skipTest(f'O_DIRECT does not work in /var/tmp: {probe.stderr.decode().strip()}')

    def blockwrite(self, data, device=None):
        return subprocess.run([PROGRAM, device or self.device], input=data, capture_output=True)

    def test_pads_last_block_with_zeros(self):
        data = os.urandom(BLOCK + 1000)
        result = self.blockwrite(data)
        self.assertEqual(result.returncode, 0, result.stderr)
        with open(self.device, 'rb') as file:
            written = file.read()
        self.assertEqual(len(written), 2 * BLOCK)
        self.assertEqual(written[:len(data)], data)
        self.assertEqual(written[len(data):], bytes(2 * BLOCK - len(data)))

    def test_reports_total(self):
        result = self.blockwrite(os.urandom(BLOCK + 1))
        self.assertIn(b'Total bytes written: 8388608', result.stderr)

    def test_exact_blocks_need_no_padding(self):
        result = self.blockwrite(bytes(BLOCK))
        self.assertEqual(os.path.getsize(self.device), BLOCK)
        self.assertIn(b'Total bytes written: 4194304', result.stderr)

    def test_empty_input_writes_nothing(self):
        result = self.blockwrite(b'')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(os.path.getsize(self.device), 0)
        self.assertIn(b'Total bytes written: 0', result.stderr)

    def test_missing_device_fails(self):
        result = self.blockwrite(b'x', os.path.join(self.device_dir, 'missing'))
        self.assertNotEqual(result.returncode, 0)

    def test_usage(self):
        result = subprocess.run([PROGRAM], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'Usage', result.stderr)


if __name__ == '__main__':
    unittest.main()
