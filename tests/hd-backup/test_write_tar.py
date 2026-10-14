# Tests for write_tar: how the messages and exit status of tar and the tape pipeline are judged. tar, mbuffer and
# blockwrite are replaced by stubs, because a real tar cannot be made to show these messages on demand.

import os
import shlex
from unittest import mock

from helpers import TempDirTestCase, capture, load_hd_backup, quiet, write_stub

hb = load_hd_backup()

TOTALS = 'Total bytes written: 10240 (10KiB, 1.0MiB/s)'


class TestWriteTar(TempDirTestCase):
    def setUp(self):
        super().setUp()
        hb.had_errors = False
        self.bin = self.path('bin')
        self.target = self.path('target')
        os.makedirs(self.target)
        # For a tape write, the target is the device: here a regular file.
        self.device = self.path('tape')
        path = mock.patch.dict(os.environ, {'PATH': self.bin + os.pathsep + os.environ['PATH']})
        path.start()
        self.addCleanup(path.stop)

    def stub_tar(self, messages, status, stdout=''):
        # A tar that prints the messages on stderr and exits with the status. For a tape write, it puts stdout on
        # the stream.
        lines = ' '.join(shlex.quote(message) for message in messages)
        printf = f"printf '%s\\n' {lines} >&2\n" if messages else ''
        write_stub(self.bin, 'tar', f'#!/bin/sh\nprintf %s {shlex.quote(stdout)}\n{printf}exit {status}\n')

    def target_for(self, is_tape):
        if is_tape:
            return self.device
        else:
            return self.target

    def write_tar(self, is_tape=False):
        output, redirect = capture()
        with redirect:
            result = hb.write_tar('/tmp/filelist.txt', self.target_for(is_tape), 0, is_tape)
        return result, output.getvalue()

    def assert_fails(self, is_tape=False):
        with quiet(), self.assertRaises(SystemExit) as raised:
            hb.write_tar('/tmp/filelist.txt', self.target_for(is_tape), 0, is_tape)
        self.assertEqual(raised.exception.code, 1)

    def test_success(self):
        self.stub_tar([TOTALS], 0)
        result, output = self.write_tar()
        self.assertEqual(result, (10240, False))

    def test_totals_without_number(self):
        self.stub_tar(['Total bytes written: unknown'], 0)
        result, output = self.write_tar()
        self.assertEqual(result, (0, False))

    def test_file_changed_while_read_is_a_warning(self):
        self.stub_tar(['tar: data/f: file changed as we read it', TOTALS], 1)
        result, output = self.write_tar()
        self.assertEqual(result, (10240, False))
        self.assertIn('WARNING: tar: data/f: file changed as we read it', output)

    def test_file_changed_with_fatal_status_fails(self):
        self.stub_tar(['tar: data/f: file changed as we read it', TOTALS], 2)
        self.assert_fails()

    def test_file_changed_with_other_problem_fails(self):
        self.stub_tar(['tar: data/f: file changed as we read it', 'tar: data/g: Cannot open: Permission denied',
                       'tar: Exiting with failure status due to previous errors', TOTALS], 2)
        self.assert_fails()

    def test_failure_status_without_message_fails(self):
        for status in (1, 2):
            self.stub_tar([TOTALS], status)
            self.assert_fails()

    def test_unexpected_message_fails(self):
        self.stub_tar(['tar: data/g: Cannot stat: No such file or directory', TOTALS], 0)
        self.assert_fails()

    def test_no_space_left_means_full(self):
        self.stub_tar(['tar: target/backup-0.tar: Cannot write: No space left on device',
                       'tar: Error is not recoverable: exiting now', 'Total bytes written: 4194304'], 2)
        result, output = self.write_tar()
        self.assertEqual(result, (4194304, True))

    def test_broken_pipe_when_full_is_expected(self):
        self.stub_tar(['Error writing block to device: No space left on device', 'cat: write error: Broken pipe',
                       'Total bytes written: 4194304'], 1)
        result, output = self.write_tar()
        self.assertEqual(result, (4194304, True))

    def test_broken_pipe_without_full_fails(self):
        self.stub_tar(['tar: -: Cannot write: Broken pipe', TOTALS], 2)
        self.assert_fails()

    def test_full_with_other_problem_fails(self):
        # A file that could not be read is not in the tarball, even when the carrier also ran full.
        self.stub_tar(['tar: data/g: Cannot open: Permission denied',
                       'tar: target/backup-0.tar: Cannot write: No space left on device', TOTALS], 2)
        self.assert_fails()

    def test_tape_pipeline_reports_bytes_from_blockwrite(self):
        self.stub_tar([], 0, stdout='tarball')
        write_stub(self.bin, 'mbuffer', '#!/bin/sh\ncat\n')
        write_stub(self.bin, 'blockwrite', '#!/bin/sh\ncat > "$1"\necho "Total bytes written: 4194304" >&2\n')
        result, output = self.write_tar(is_tape=True)
        self.assertEqual(result, (4194304, False))
        # The stream went through mbuffer to blockwrite, which wrote it to the device.
        with open(self.device) as device:
            self.assertEqual(device.read(), 'tarball')

    def test_mbuffer_log_is_unique_and_removed_after_success(self):
        # The mbuffer stub writes to its log, and keeps the name of it.
        self.stub_tar([], 0, stdout='tarball')
        write_stub(self.bin, 'mbuffer', '#!/bin/sh\nwhile [ "$1" != "-l" ]; do shift; done\n'
                                        'echo "$2" >> "$(dirname "$0")/logs"\necho logged > "$2"\ncat\n')
        write_stub(self.bin, 'blockwrite', '#!/bin/sh\ncat > /dev/null\necho "Total bytes written: 0" >&2\n')
        self.write_tar(is_tape=True)
        self.write_tar(is_tape=True)
        with open(os.path.join(self.bin, 'logs')) as file:
            logs = file.read().splitlines()
        self.assertEqual(len(set(logs)), 2)
        for log in logs:
            self.assertEqual(os.path.dirname(log), self.tempdir)
        self.assertEqual(self.temp_files(), [])

    def test_mbuffer_log_is_kept_after_failure(self):
        self.stub_tar([], 0, stdout='tarball')
        write_stub(self.bin, 'mbuffer', '#!/bin/sh\ncat > /dev/null\nexit 1\n')
        write_stub(self.bin, 'blockwrite', '#!/bin/sh\ncat > /dev/null\necho "Total bytes written: 0" >&2\n')
        output, redirect = capture()
        with redirect, self.assertRaises(SystemExit):
            hb.write_tar('/tmp/filelist.txt', self.device, 0, True)
        logs = self.temp_files()
        self.assertEqual(len(logs), 1)
        self.assertTrue(logs[0].startswith('hd-backup-mbuffer-'))
        self.assertIn(f'The log of mbuffer is in {os.path.join(self.tempdir, logs[0])}.', output.getvalue())

    def test_disk_write_has_no_mbuffer_log(self):
        self.stub_tar([TOTALS], 0)
        self.write_tar()
        self.assertEqual(self.temp_files(), [])

    def test_tape_pipeline_fails_when_tar_fails(self):
        # Without pipefail, the status of blockwrite at the end of the pipeline would hide the failure of tar.
        self.stub_tar([], 2, stdout='part of a tarball')
        write_stub(self.bin, 'mbuffer', '#!/bin/sh\ncat\n')
        write_stub(self.bin, 'blockwrite', '#!/bin/sh\ncat > /dev/null\necho "Total bytes written: 4194304" >&2\n')
        self.assert_fails(is_tape=True)

    def test_tape_pipeline_fails_when_mbuffer_fails(self):
        self.stub_tar([], 0, stdout='tarball')
        write_stub(self.bin, 'mbuffer', '#!/bin/sh\ncat > /dev/null\nexit 1\n')
        write_stub(self.bin, 'blockwrite', '#!/bin/sh\ncat > /dev/null\necho "Total bytes written: 0" >&2\n')
        self.assert_fails(is_tape=True)
