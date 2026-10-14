# Tests for blockwrite, the program that writes the tape stream of hd-backup in 4 MiB blocks.

import os
import shutil
import subprocess
import tempfile
import unittest

from helpers import BLOCKWRITE_SOURCE, TempDirTestCase

BLOCK = 4 * 1024 * 1024


@unittest.skipIf(shutil.which('gcc') is None, 'gcc is not installed')
class TestBlockwrite(TempDirTestCase):
    def setUp(self):
        super().setUp()
        self.program = self.path('blockwrite')
        subprocess.run(['gcc', '-O2', '-Wall', '-Werror', '-D_GNU_SOURCE', '-o', self.program, BLOCKWRITE_SOURCE],
                       check=True)

        # blockwrite opens its target with O_DIRECT, which tmpfs (often /tmp) does not support. /var/tmp is
        # usually on disk.
        self.device_dir = tempfile.mkdtemp(prefix='hd-backup-test-', dir='/var/tmp')
        self.device = os.path.join(self.device_dir, 'device')
        open(self.device, 'w').close()
        probe = subprocess.run([self.program, self.device], input=b'', capture_output=True)
        if probe.returncode != 0:
            shutil.rmtree(self.device_dir)
            self.skipTest(f'O_DIRECT does not work in /var/tmp: {probe.stderr.decode().strip()}')

    def tearDown(self):
        shutil.rmtree(self.device_dir, ignore_errors=True)
        super().tearDown()

    def blockwrite(self, data, device=None):
        return subprocess.run([self.program, device or self.device], input=data, capture_output=True)

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
        result = subprocess.run([self.program], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'Usage', result.stderr)
