# Tests for hd-backup-prep, hd-backup-load and hd-backup-unload of the package hd-backup. The
# commands they run are replaced by a mock of subprocess.run that records them, so no drive or
# pool is touched.

import contextlib
import importlib.machinery
import importlib.util
import io
import os
import stat
import subprocess
import tempfile
import unittest

from unittest import mock

PACKAGE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SIZE = 4000 * 1024 * 1024 * 1024


def load(name):
    # The scripts have no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader(name.replace('-', '_'), os.path.join(PACKAGE, name))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


prep = load('hd-backup-prep')
load_cmd = load('hd-backup-load')
unload_cmd = load('hd-backup-unload')


class Commands:
    """Records the commands, prints the size of the drive for blockdev --getsize64 and fails the
    commands named in fail."""

    def __init__(self, fail=()):
        self.calls = []
        self.fail = fail

    def __call__(self, args, check=False, **kwargs):
        args = list(args)
        self.calls.append(args)
        code = 1 if args[0] in self.fail or ' '.join(args[:2]) in self.fail else 0
        if check and code:
            raise subprocess.CalledProcessError(code, args)
        stdout = f'{SIZE}\n' if args[:2] == ['blockdev', '--getsize64'] else ''
        return subprocess.CompletedProcess(args, code, stdout=stdout)

    def names(self):
        return [' '.join(call[:2]) for call in self.calls]


def block_device(path):
    return os.stat_result((stat.S_IFBLK | 0o660, 0, 0, 1, 0, 0, 0, 0, 0, 0))


def run_quietly(main, argv):
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as err:
        return main(argv), err.getvalue()


class PrepTest(unittest.TestCase):

    def setUp(self):
        self.commands = Commands()
        self.answer = 'y'
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.mnt = tmp.name
        patches = [
            mock.patch.object(prep.subprocess, 'run', self.commands),
            mock.patch.object(prep.os, 'geteuid', return_value=0),
            mock.patch.object(prep.os, 'stat', side_effect=block_device),
            mock.patch.object(prep, 'input', side_effect=lambda prompt: self.answer, create=True),
            mock.patch.object(prep, 'MOUNT_DIR', self.mnt),
            mock.patch.object(prep, 'owner_of', return_value=(1000, 1000)),
        ]
        self.zero = mock.patch.object(prep, 'zero_start').start()
        self.give = mock.patch.object(prep, 'give').start()
        for patch in patches:
            patch.start()
        self.addCleanup(mock.patch.stopall)

    def test_creates_the_pool(self):
        code, err = run_quietly(prep.main, ['/dev/sdx', 'bk01', '--owner', 'erik'])

        self.assertEqual(code, 0, err)
        self.zero.assert_called_once_with('/dev/sdx')
        names = self.commands.names()
        self.assertLess(names.index('zpool labelclear'), names.index('zpool create'))
        self.assertLess(names.index('wipefs -a'), names.index('zpool create'))
        self.assertLess(names.index('udevadm settle'), names.index('zpool create'))
        create = next(call for call in self.commands.calls if call[:2] == ['zpool', 'create'])
        self.assertIn('encryption=aes-256-gcm', create)
        self.assertIn('keyformat=passphrase', create)
        self.assertIn('compression=zstd', create)
        self.assertIn('recordsize=4M', create)
        self.assertIn(f'mountpoint={self.mnt}/bk01', create)
        self.assertEqual(create[-2:], ['bk01', '/dev/sdx'])
        reserved = next(call for call in self.commands.calls if call[:2] == ['zfs', 'create'])
        self.assertEqual(reserved, ['zfs', 'create', '-o', f'reservation={SIZE // 10}', 'bk01/reserved'])
        self.give.assert_called_once_with(f'{self.mnt}/bk01', 1000, 1000)

    def test_no_changes_without_a_yes(self):
        self.answer = ''
        code, err = run_quietly(prep.main, ['/dev/sdx', 'bk01'])

        self.assertEqual(code, 1)
        self.assertEqual(self.commands.names(), ['blockdev --getsize64'])
        self.zero.assert_not_called()

    def test_refuses_a_file(self):
        with mock.patch.object(prep.os, 'stat', return_value=os.stat_result((stat.S_IFREG | 0o644,) + (0,) * 9)):
            code, err = run_quietly(prep.main, ['/tmp/image', 'bk01'])

        self.assertEqual(code, 1)
        self.assertIn('not a block device', err)
        self.assertEqual(self.commands.calls, [])

    def test_wipe_failures_do_not_matter(self):
        self.commands.fail = ('zpool labelclear', 'wipefs', 'blockdev --rereadpt', 'partprobe')
        code, err = run_quietly(prep.main, ['/dev/sdx', 'bk01'])

        self.assertEqual(code, 0, err)
        self.assertIn('zpool create', self.commands.names())

    def test_a_failed_create_fails(self):
        self.commands.fail = ('zpool create',)
        code, err = run_quietly(prep.main, ['/dev/sdx', 'bk01'])

        self.assertEqual(code, 1)
        self.assertIn('zpool create', err)
        self.assertNotIn('zfs create', self.commands.names())
        self.give.assert_not_called()

    def test_needs_root(self):
        with mock.patch.object(prep.os, 'geteuid', return_value=1000):
            code, err = run_quietly(prep.main, ['/dev/sdx', 'bk01'])

        self.assertEqual(code, 1)
        self.assertIn('root', err)
        self.assertEqual(self.commands.calls, [])


class PrepFilesTest(unittest.TestCase):

    def test_zero_start_writes_zeros(self):
        with tempfile.NamedTemporaryFile() as image:
            image.write(b'\xff' * (prep.MIB * 2))
            image.flush()
            with mock.patch.object(prep, 'ZERO_MIB', 1):
                prep.zero_start(image.name)
            with open(image.name, 'rb') as f:
                data = f.read()

        self.assertEqual(data[:prep.MIB], bytes(prep.MIB))
        self.assertEqual(data[prep.MIB:], b'\xff' * prep.MIB)

    def test_give_chowns_the_tree(self):
        with tempfile.TemporaryDirectory() as root:
            os.makedirs(os.path.join(root, 'reserved'))
            with mock.patch.object(prep.os, 'chown') as chown:
                prep.give(root, 1000, 1001)

        paths = [call.args[0] for call in chown.call_args_list]
        self.assertEqual(paths, [root, os.path.join(root, 'reserved')])
        self.assertTrue(all(call.args[1:] == (1000, 1001) for call in chown.call_args_list))

    def test_unknown_owner(self):
        with self.assertRaises(prep.PrepError):
            prep.owner_of('no-such-user-hd-backup')


class LoadUnloadTest(unittest.TestCase):

    def run_tool(self, module, argv, commands):
        with mock.patch.object(module.subprocess, 'run', commands), \
                mock.patch.object(module.os, 'geteuid', return_value=0):
            return run_quietly(module.main, argv)

    def test_load(self):
        commands = Commands()
        code, err = self.run_tool(load_cmd, ['bk01'], commands)

        self.assertEqual(code, 0, err)
        self.assertEqual(commands.calls, [['zpool', 'import', '-f', 'bk01'],
                                          ['zfs', 'load-key', 'bk01'],
                                          ['zfs', 'mount', 'bk01']])

    def test_load_stops_at_a_wrong_passphrase(self):
        commands = Commands(fail=('zfs load-key',))
        code, err = self.run_tool(load_cmd, ['bk01'], commands)

        self.assertEqual(code, 1)
        self.assertIn('zfs load-key bk01', err)
        self.assertNotIn('zfs mount', commands.names())

    def test_unload(self):
        commands = Commands()
        code, err = self.run_tool(unload_cmd, ['bk01'], commands)

        self.assertEqual(code, 0, err)
        self.assertEqual(commands.calls, [['zpool', 'export', 'bk01']])

    def test_need_root(self):
        for module in (load_cmd, unload_cmd):
            commands = Commands()
            with mock.patch.object(module.subprocess, 'run', commands), \
                    mock.patch.object(module.os, 'geteuid', return_value=1000):
                code, err = run_quietly(module.main, ['bk01'])

            self.assertEqual(code, 1)
            self.assertEqual(commands.calls, [])


if __name__ == '__main__':
    unittest.main()
