# End to end tests: write backups with hd-backup, restore them with tar and the delete list, and compare the
# result with the live tree. Tape drives are replaced by stub programs.

import os
import shutil
import tarfile
import unittest

from helpers import CliTestCase, is_root, snapshot, write_file, write_stub

# Stubs for the programs a tape write runs. mt-st logs its arguments. mbuffer passes the stream on.
MT_ST_STUB = '#!/bin/sh\necho "$@" >> "$(dirname "$0")/mt-st.log"\n'
MBUFFER_STUB = '#!/bin/sh\ncat\n'

# mt-st that fails on one command, given as the first argument of the stub: setblk, compression, fsf or rewind.
MT_ST_FAILING_STUB = '''#!/bin/sh
echo "$@" >> "$(dirname "$0")/mt-st.log"
if [ "$3" = "{command}" ]; then
    echo "/dev/nst0: Input/output error" >&2
    exit 1
fi
'''

# blockwrite that writes everything to the "device" (a regular file) and reports the total, like the real one.
BLOCKWRITE_STUB = '''#!/bin/sh
cat > "$1"
echo "Total bytes written: $(stat -c %s "$1")" >&2
'''

# blockwrite on a tape that is full after one 4 MiB block.
BLOCKWRITE_FULL_STUB = '''#!/bin/sh
head -c 4194304 > "$1"
cat > /dev/null
echo "Error writing block to device: No space left on device" >&2
echo "Total bytes written: 4194304" >&2
exit 1
'''

# blockwrite on a broken tape drive.
BLOCKWRITE_BROKEN_STUB = '''#!/bin/sh
cat > /dev/null
echo "Error writing block to device: Input/output error" >&2
echo "Total bytes written: 0" >&2
exit 1
'''


class BackupTestCase(CliTestCase):
    def setUp(self):
        super().setUp()
        self.target = self.path('target')
        os.makedirs(self.target)
        self.mtime = 1700000000

    def write(self, name, content):
        # Every write gets a new mtime, so a change is seen even within the same second.
        self.mtime += 10
        return write_file(os.path.join(self.data, name), content, mtime=self.mtime)

    def live(self):
        result = snapshot(self.data)
        return {path: item for path, item in result.items() if not path.startswith('.cache')}

    def tarballs(self, target=None):
        target = target or self.target
        count = len([name for name in os.listdir(target) if name.startswith('backup-')])
        return [os.path.join(target, f'backup-{index}.tar') for index in range(count)]

    def restore_and_compare(self, tarballs):
        restore_dir = self.path('restore')
        shutil.rmtree(restore_dir, ignore_errors=True)
        self.restore(tarballs, restore_dir)
        self.assertEqual(snapshot(self.restored(restore_dir, self.data)), self.live())


class TestRoundTrip(BackupTestCase):
    def build_tree(self):
        self.write('file', 'contents')
        self.write('empty', '')
        self.write('nested/deep/er/file', 'deep')
        self.write('tree/a', 'a')
        self.write('tree/sub/b', 'b')
        self.write('recreated', 'first life')
        self.write('becomes-link', 'file for now')
        self.write('q"uote back\\slash\nnewline é', 'odd name')
        self.write('.cache/skip', 'excluded')
        os.makedirs(os.path.join(self.data, 'empty-dir'))
        os.symlink('file', os.path.join(self.data, 'link'))
        os.symlink('nested', os.path.join(self.data, 'dir-link'))
        os.symlink('missing', os.path.join(self.data, 'broken-link'))

    def test_full_and_incremental_backups_restore_the_tree(self):
        self.build_tree()
        self.run_cli('add-fileset', 'fs', self.data, '^' + os.path.join(self.data, '.cache'))
        self.run_cli('add-backup', 'bk', 'fs')
        self.run_cli('add-carrier', 'bk', 'c1')
        self.run_cli('write', 'bk', self.target)
        self.restore_and_compare(self.tarballs())

        # Every kind of change.
        self.write('file', 'new contents')
        self.write('added', 'new file')
        os.remove(os.path.join(self.data, 'empty'))
        shutil.rmtree(os.path.join(self.data, 'tree'))
        os.remove(os.path.join(self.data, 'recreated'))
        self.run_cli('update-filesets', 'fs')
        self.write('recreated', 'second life')
        os.remove(os.path.join(self.data, 'link'))
        os.symlink('added', os.path.join(self.data, 'link'))
        os.remove(os.path.join(self.data, 'becomes-link'))
        os.symlink('file', os.path.join(self.data, 'becomes-link'))
        self.run_cli('update-filesets', 'fs')
        self.run_cli('write', 'bk', self.target)
        self.restore_and_compare(self.tarballs())

        # A file of the second tarball is deleted before the third.
        os.remove(os.path.join(self.data, 'added'))
        self.run_cli('update-filesets', 'fs')
        self.run_cli('write', 'bk', self.target)
        self.restore_and_compare(self.tarballs())

        summary = self.run_cli('summary', 'bk').stdout
        self.assertIn('All files of the fileset are on carrier', summary)
        self.assertIn('All symlinks of the fileset are on carrier', summary)

    def test_two_backups_of_one_fileset_are_independent(self):
        self.build_tree()
        self.run_cli('add-fileset', 'fs', self.data, '^' + os.path.join(self.data, '.cache'))
        self.run_cli('add-backup', 'early', 'fs')
        self.run_cli('add-carrier', 'early', 'e1')
        self.run_cli('write', 'early', self.target)

        self.write('file', 'changed before the second backup')
        os.remove(os.path.join(self.data, 'empty'))
        self.run_cli('update-filesets', 'fs')
        self.run_cli('add-backup', 'late', 'fs')
        self.run_cli('add-carrier', 'late', 'l1')
        late_target = self.path('late')
        os.makedirs(late_target)
        self.run_cli('write', 'late', late_target)

        # The second backup holds every path once, in its newest version, and nothing to delete.
        with tarfile.open(os.path.join(late_target, 'backup-0.tar')) as archive:
            names = [name for name in archive.getnames() if not name.startswith('tmp/deletes_')]
        self.assertEqual(len(names), len(set(names)))
        self.restore_and_compare(self.tarballs(late_target))

        self.run_cli('write', 'early', self.target)
        self.restore_and_compare(self.tarballs())


class TestWriteErrors(BackupTestCase):
    def setUp(self):
        super().setUp()
        self.write('a', 'a')
        self.write('secret', 'secret')
        self.run_cli('add-fileset', 'fs', self.data)
        self.run_cli('add-backup', 'bk', 'fs')
        self.run_cli('add-carrier', 'bk', 'c1', 'c2')

    @unittest.skipIf(is_root(), 'root can read any file')
    def test_unreadable_file_fails_the_write(self):
        os.chmod(os.path.join(self.data, 'secret'), 0)
        result = self.run_cli('write', 'bk', self.target, expect=1)
        self.assertIn('Cannot open: Permission denied', result.stdout)
        self.assertEqual(self.query('SELECT COUNT(*) FROM tarballs'), [(0,)])
        self.assertIn('There are 2 files not on carrier', self.run_cli('summary').stdout)

        # Fixed, the write succeeds, with the same tarball index.
        os.chmod(os.path.join(self.data, 'secret'), 0o644)
        self.run_cli('write', 'bk', self.target)
        self.assertEqual(self.query('SELECT idx, files FROM tarballs'), [(0, 2)])
        self.restore_and_compare(self.tarballs())

    def test_full_carrier_continues_on_next_carrier(self):
        # A drive that is full from the start.
        os.symlink('/dev/full', os.path.join(self.target, 'backup-0.tar'))
        self.run_cli('write', 'bk', self.target)
        self.assertEqual(self.query('SELECT label, full FROM carrier ORDER BY label'), [('c1', 1), ('c2', 0)])
        self.assertEqual(self.query('SELECT files, symlinks FROM tarballs'), [(0, 0)])

        second = self.path('second')
        os.makedirs(second)
        self.run_cli('write', 'bk', second)
        self.assertIn('All files of the fileset are on carrier', self.run_cli('summary').stdout)
        self.restore_and_compare(self.tarballs(second))


class TestTape(BackupTestCase):
    def setUp(self):
        super().setUp()
        self.bin = self.path('bin')
        os.makedirs(self.bin)
        self.stub('mt-st', MT_ST_STUB)
        self.stub('mbuffer', MBUFFER_STUB)
        self.device = self.path('tape')
        write_file(self.device)

        self.write('small', 'small')
        self.write('big1', os.urandom(3000000))
        self.write('big2', os.urandom(3000000))
        os.symlink('small', os.path.join(self.data, 'link'))
        self.run_cli('add-fileset', 'fs', self.data)
        self.run_cli('add-backup', 'bk', 'fs')
        self.run_cli('add-carrier', 'bk', 't1', 't2')

    def stub(self, name, script):
        write_stub(self.bin, name, script)

    def write_tape(self, expect=0):
        return self.run_cli('write-tape', 'bk', self.device, expect=expect, path_prefix=self.bin)

    def mt_calls(self):
        with open(os.path.join(self.bin, 'mt-st.log')) as file:
            return file.read().splitlines()

    def test_write(self):
        self.stub('blockwrite', BLOCKWRITE_STUB)
        self.write_tape()
        self.assertEqual(self.mt_calls(), [f'-f {self.device} setblk 4194304', f'-f {self.device} compression 1',
                                           f'-f {self.device} rewind'])
        self.assertEqual(self.query('SELECT files, symlinks FROM tarballs'), [(3, 1)])
        self.restore_and_compare([self.device])

        # The second tarball skips over the first file on the tape.
        self.write('small', 'changed')
        self.run_cli('update-filesets', 'fs')
        self.write_tape()
        self.assertIn(f'-f {self.device} fsf 1', self.mt_calls())
        self.assertEqual(self.query('SELECT idx, files FROM tarballs ORDER BY idx'), [(0, 3), (1, 1)])

    def test_full_tape_records_only_complete_items(self):
        self.stub('blockwrite', BLOCKWRITE_FULL_STUB)
        self.write_tape()
        self.assertEqual(self.query('SELECT label, full FROM carrier ORDER BY label'), [('t1', 1), ('t2', 0)])
        recorded = {row[0] for row in self.query(
            'SELECT path FROM files JOIN file_tarball_links ON file_number=number')}
        recorded |= {row[0] for row in self.query(
            'SELECT path FROM symlinks JOIN symlink_tarball_links ON symlink_number=number')}

        # Read the tape up to where it stopped: every recorded item must be complete on it.
        size = os.path.getsize(self.device)
        complete = set()
        with tarfile.open(self.device) as archive:
            while True:
                try:
                    member = archive.next()
                except tarfile.ReadError:
                    break
                if member is None:
                    break
                if member.offset_data + (member.size if member.isreg() else 0) <= size:
                    complete.add('/' + member.name.rstrip('/'))
        self.assertTrue(recorded)
        self.assertLessEqual(recorded, complete)
        self.assertIn(os.path.join(self.data, 'big1'), recorded)
        self.assertNotIn(os.path.join(self.data, 'big2'), recorded)
        self.assertIn(f'-f {self.device} rewind', self.mt_calls())

        # The rest goes on the next tape.
        self.stub('blockwrite', BLOCKWRITE_STUB)
        self.write_tape()
        self.assertIn('All files of the fileset are on carrier', self.run_cli('summary').stdout)

    def test_mt_st_failure_before_writing(self):
        self.stub('blockwrite', BLOCKWRITE_STUB)
        self.stub('mt-st', MT_ST_FAILING_STUB.format(command='setblk'))
        result = self.write_tape(expect=1)
        self.assertIn('Could not set blocksize: mt-st error: /dev/nst0: Input/output error', result.stdout)
        self.assertEqual(self.query('SELECT COUNT(*) FROM tarballs'), [(0,)])
        self.assertEqual(os.path.getsize(self.device), 0)

    def test_compression_and_skip_failures(self):
        self.stub('blockwrite', BLOCKWRITE_STUB)
        self.stub('mt-st', MT_ST_FAILING_STUB.format(command='compression'))
        self.assertIn('Could not enable compression', self.write_tape(expect=1).stdout)

        self.stub('mt-st', MT_ST_STUB)
        self.write_tape()
        self.write('small', 'changed')
        self.run_cli('update-filesets', 'fs')
        self.stub('mt-st', MT_ST_FAILING_STUB.format(command='fsf'))
        self.assertIn('Could not advance tape to correct file record', self.write_tape(expect=1).stdout)
        self.assertEqual(self.query('SELECT COUNT(*) FROM tarballs'), [(1,)])

    def test_rewind_failure_is_reported_after_writing(self):
        # The tarball is on the tape and is recorded, but the command fails, so the rewind is not missed.
        self.stub('blockwrite', BLOCKWRITE_STUB)
        self.stub('mt-st', MT_ST_FAILING_STUB.format(command='rewind'))
        result = self.write_tape(expect=1)
        self.assertIn('Could not rewind tape: mt-st error: /dev/nst0: Input/output error', result.stdout)
        self.assertEqual(self.query('SELECT files, symlinks FROM tarballs'), [(3, 1)])

    def test_tape_error_fails_the_write_and_rewinds(self):
        self.stub('blockwrite', BLOCKWRITE_BROKEN_STUB)
        result = self.write_tape(expect=1)
        self.assertIn('Input/output error', result.stdout)
        self.assertEqual(self.query('SELECT COUNT(*) FROM tarballs'), [(0,)])
        self.assertEqual(self.query('SELECT full FROM carrier'), [(0,), (0,)])
        self.assertEqual(self.mt_calls()[-1], f'-f {self.device} rewind')
