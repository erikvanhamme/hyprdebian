# Tests for hd-backup-vm, hd-backup-vms and hd-restore-vm of the package hd-backup. virsh, zfs and
# zstd are replaced by tests/fake-command, which logs its calls and keeps the snapshots in a
# temporary folder.

import contextlib
import getpass
import importlib.machinery
import importlib.util
import io
import itertools
import json
import os
import stat
import tempfile
import unittest

from unittest import mock

PACKAGE = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
FAKE_COMMAND = os.path.join(PACKAGE, 'tests', 'fake-command')


def load(name):
    # The scripts have no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader(name.replace('-', '_'), os.path.join(PACKAGE, name))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


backup_vm = load('hd-backup-vm')
backup_vms = load('hd-backup-vms')
restore_vm = load('hd-restore-vm')

VM_XML = """<domain type='kvm' xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'>
  <name>git</name>
  <uuid>8a1c6d36-0d43-4b8e-9f4e-3f6a3d1d2c11</uuid>
  <os>
    <type arch='x86_64' machine='q35'>hvm</type>
    <nvram>{nvram}</nvram>
  </os>
  <devices>
    <disk type='block' device='disk'>
      <source dev='/dev/zvol/rpool/vm/git'/>
      <target dev='vda' bus='virtio'/>
    </disk>
    <disk type='block'>
      <source dev='/dev/zvol/dpool/vm/git-data'/>
      <target dev='vdb' bus='virtio'/>
    </disk>
    <disk type='file' device='cdrom'>
      <source file='/srv/iso/debian.iso'/>
      <target dev='sda' bus='sata'/>
    </disk>
    <interface type='bridge'>
      <mac address='52:54:00:12:34:56'/>
      <source bridge='br0'/>
    </interface>
  </devices>
  <qemu:commandline>
    <qemu:arg value='-no-hpet'/>
  </qemu:commandline>
</domain>
"""


class FakeSystem:
    """A temporary folder with the fake commands first on PATH, the XML of the VM, its NVRAM and
    the backup folder."""

    def __init__(self, test):
        tmp = tempfile.TemporaryDirectory()
        test.addCleanup(tmp.cleanup)
        self.root = tmp.name
        self.state = os.path.join(self.root, 'state')
        self.bin = os.path.join(self.root, 'bin')
        self.backups = os.path.join(self.root, 'backups')
        self.nvram = os.path.join(self.root, 'nvram', 'git_VARS.fd')
        for path in (self.state, self.bin, os.path.dirname(self.nvram)):
            os.makedirs(path)
        for name in ('virsh', 'zfs', 'zstd'):
            os.symlink(FAKE_COMMAND, os.path.join(self.bin, name))

        with open(self.nvram, 'w') as f:
            f.write('nvram 1')
        self.set_xml(VM_XML.format(nvram=self.nvram))

        patches = [
            mock.patch.dict(os.environ, {'FAKE_STATE': self.state,
                                         'PATH': self.bin + os.pathsep + os.environ['PATH']}),
            mock.patch.object(backup_vm, 'LOCK_DIR', self.root),
        ]
        for patch in patches:
            patch.start()
            test.addCleanup(patch.stop)

        # Every run gets a timestamp of its own, also within the same second.
        seconds = itertools.count()
        patch = mock.patch.object(backup_vm.time, 'strftime',
                                  side_effect=lambda fmt: f'20261017_0330{next(seconds):02d}')
        patch.start()
        test.addCleanup(patch.stop)

    def set_xml(self, xml):
        with open(os.path.join(self.state, 'vm.xml'), 'w') as f:
            f.write(xml)

    def set_state(self, vm_state):
        with open(os.path.join(self.state, 'domstate'), 'w') as f:
            f.write(vm_state + '\n')

    def fail(self, command, subcommand):
        open(os.path.join(self.state, f'fail-{command}-{subcommand}'), 'w').close()

    def snapshots(self):
        path = os.path.join(self.state, 'snapshots')
        if not os.path.exists(path):
            return []
        with open(path) as f:
            return f.read().split()

    def calls(self):
        with open(os.path.join(self.state, 'log')) as f:
            return [json.loads(line) for line in f]

    def backup(self, vm='git'):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = backup_vm.main([vm, self.backups, getpass.getuser()])
        return status, out.getvalue()

    def files(self):
        return sorted(os.listdir(os.path.join(self.backups, 'git')))

    def read(self, name):
        with open(os.path.join(self.backups, 'git', name)) as f:
            return f.read()


class TestBackupVmParsing(unittest.TestCase):
    def test_datasets_are_the_zvols_without_cdroms(self):
        xml = VM_XML.format(nvram='/var/lib/libvirt/qemu/nvram/git_VARS.fd')
        self.assertEqual(backup_vm.vm_datasets(xml), ['rpool/vm/git', 'dpool/vm/git-data'])
        self.assertEqual(backup_vm.nvram_path(xml), '/var/lib/libvirt/qemu/nvram/git_VARS.fd')

    def test_other_disks_are_refused(self):
        image = "<domain><devices><disk type='file' device='disk'><source file='/a.qcow2'/></disk></devices></domain>"
        block = "<domain><devices><disk type='block'><source dev='/dev/sdb'/></disk></devices></domain>"
        empty = "<domain><devices/></domain>"
        for xml in (image, block, empty):
            with self.assertRaises(backup_vm.BackupError):
                backup_vm.vm_datasets(xml)
        self.assertEqual(backup_vm.nvram_path(empty), '')

    def test_snapshots_are_grouped_per_pool(self):
        self.assertEqual(backup_vm.pool_snapshots(['rpool/a', 'dpool/b', 'rpool/c'], 's'),
                         {'rpool': ['rpool/a@s', 'rpool/c@s'], 'dpool': ['dpool/b@s']})

    def test_sequence_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'git.sequence')
            self.assertEqual(backup_vm.read_sequence(path), (0, ''))
            with open(path, 'w') as f:
                f.write('3\n')
            self.assertEqual(backup_vm.read_sequence(path), (3, ''))
            with open(path, 'w') as f:
                f.write('4 backup-20261017_033000\n')
            self.assertEqual(backup_vm.read_sequence(path), (4, 'backup-20261017_033000'))

    def test_earlier_streams(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name in ('rpool_vm_git.0000.zst', 'rpool_vm_git.0001.zst', 'rpool_vm_git2.0000.zst'):
                open(os.path.join(tmp, name), 'w').close()
            self.assertFalse(backup_vm.has_earlier_streams(tmp, 'rpool_vm_git', 0))
            self.assertTrue(backup_vm.has_earlier_streams(tmp, 'rpool_vm_git', 1))
            self.assertFalse(backup_vm.has_earlier_streams(tmp, 'rpool_vm', 5))


class TestBackupVm(unittest.TestCase):
    def setUp(self):
        self.system = FakeSystem(self)

    def test_first_backup_is_full_then_incremental(self):
        status, out = self.system.backup()
        self.assertEqual(status, 0, out)
        self.assertEqual(self.system.files(), [
            'dpool_vm_git-data.0000.zst', 'git.sequence', 'git.xml', 'git_VARS.fd',
            'rpool_vm_git.0000.zst'])
        self.assertEqual(self.system.read('rpool_vm_git.0000.zst'),
                         'send rpool/vm/git@backup-20261017_033000\n')
        self.assertEqual(self.system.read('git.sequence'), '1 backup-20261017_033000\n')
        self.assertEqual(self.system.read('git_VARS.fd'), 'nvram 1')
        self.assertIn('<name>git</name>', self.system.read('git.xml'))
        self.assertEqual(sorted(self.system.snapshots()), [
            'dpool/vm/git-data@backup-20261017_033000', 'rpool/vm/git@backup-20261017_033000'])

        status, out = self.system.backup()
        self.assertEqual(status, 0, out)
        self.assertEqual(self.system.read('rpool_vm_git.0001.zst'),
                         'send -i @backup-20261017_033000 rpool/vm/git@backup-20261017_033001\n')
        self.assertEqual(self.system.read('git.sequence'), '2 backup-20261017_033001\n')
        # Only the snapshot of the last backup stays, as the base of the next one.
        self.assertEqual(sorted(self.system.snapshots()), [
            'dpool/vm/git-data@backup-20261017_033001', 'rpool/vm/git@backup-20261017_033001'])

    def test_guest_is_frozen_around_the_snapshots(self):
        self.system.backup()
        # The name of the VM is left out of the calls of virsh.
        calls = [call[:2] + call[3:4] if call[0] == 'virsh' else call
                 for call in self.system.calls() if call[1] in ('qemu-agent-command', 'snapshot')]
        self.assertEqual(calls, [
            ['virsh', 'qemu-agent-command', '{"execute":"guest-fsfreeze-freeze"}'],
            ['zfs', 'snapshot', 'rpool/vm/git@backup-20261017_033000'],
            ['zfs', 'snapshot', 'dpool/vm/git-data@backup-20261017_033000'],
            ['virsh', 'qemu-agent-command', '{"execute":"guest-fsfreeze-thaw"}'],
        ])

    def test_failed_freeze_takes_a_crash_consistent_backup(self):
        self.system.fail('virsh', 'qemu-agent-command')
        status, out = self.system.backup()
        self.assertEqual(status, 0, out)
        self.assertIn('crash-consistent', out)

    def test_shut_off_vm_is_not_frozen(self):
        self.system.set_state('shut off')
        status, out = self.system.backup()
        self.assertEqual(status, 0, out)
        self.assertIn('git is shut off, its disks are consistent.', out)
        self.assertNotIn('qemu-agent-command', json.dumps(self.system.calls()))
        self.assertEqual(self.system.read('git.sequence'), '1 backup-20261017_033000\n')
        self.assertEqual(self.system.read('git_VARS.fd'), 'nvram 1')

    def test_failed_shut_off_backup_does_not_thaw(self):
        self.system.set_state('shut off')
        self.system.fail('zfs', 'send')
        status, out = self.system.backup()
        self.assertEqual(status, 1)
        self.assertNotIn('qemu-agent-command', json.dumps(self.system.calls()))
        self.assertEqual(self.system.snapshots(), [])

    def test_paused_vm_takes_a_crash_consistent_backup(self):
        self.system.set_state('paused')
        status, out = self.system.backup()
        self.assertEqual(status, 0, out)
        self.assertIn('Warning: git is paused, taking crash-consistent snapshot.', out)
        self.assertNotIn('qemu-agent-command', json.dumps(self.system.calls()))

    def test_new_disk_starts_with_a_full_stream(self):
        self.system.backup()
        xml = VM_XML.format(nvram=self.system.nvram).replace(
            '</devices>', "<disk type='block'><source dev='/dev/zvol/rpool/vm/git-new'/></disk></devices>")
        self.system.set_xml(xml)
        status, out = self.system.backup()
        self.assertEqual(status, 0, out)
        self.assertEqual(self.system.read('rpool_vm_git-new.0001.zst'),
                         'send rpool/vm/git-new@backup-20261017_033001\n')

    def test_failed_backup_leaves_the_last_finished_one(self):
        self.system.backup()
        self.system.fail('zfs', 'send')
        nvram_before = self.system.read('git_VARS.fd')
        with open(self.system.nvram, 'w') as f:
            f.write('nvram 2')

        status, out = self.system.backup()
        self.assertEqual(status, 1)
        self.assertEqual(self.system.files(), [
            'dpool_vm_git-data.0000.zst', 'git.sequence', 'git.xml', 'git_VARS.fd',
            'rpool_vm_git.0000.zst'])
        self.assertEqual(self.system.read('git.sequence'), '1 backup-20261017_033000\n')
        self.assertEqual(self.system.read('git_VARS.fd'), nvram_before)
        self.assertEqual(sorted(self.system.snapshots()), [
            'dpool/vm/git-data@backup-20261017_033000', 'rpool/vm/git@backup-20261017_033000'])
        # The guest is thawed after the snapshots, not again in the cleanup.
        thaws = [call for call in self.system.calls() if 'guest-fsfreeze-thaw' in ' '.join(call)]
        self.assertEqual(len(thaws), 2)

    def test_failed_snapshot_thaws_the_guest(self):
        self.system.fail('zfs', 'snapshot')
        status, out = self.system.backup()
        self.assertEqual(status, 1)
        self.assertIn('guest-fsfreeze-thaw', json.dumps(self.system.calls()[-1]))
        self.assertEqual(self.system.files(), [])

    def test_missing_base_snapshot_is_refused(self):
        self.system.backup()
        os.remove(os.path.join(self.system.state, 'snapshots'))
        status, out = self.system.backup()
        self.assertEqual(status, 1)
        self.assertIn('the base snapshot of rpool/vm/git is gone', out)
        self.assertEqual(self.system.read('git.sequence'), '1 backup-20261017_033000\n')

    def test_old_sequence_file_takes_the_newest_backup_snapshot(self):
        self.system.backup()
        with open(os.path.join(self.system.backups, 'git', 'git.sequence'), 'w') as f:
            f.write('1\n')
        status, out = self.system.backup()
        self.assertEqual(status, 0, out)
        self.assertEqual(self.system.read('rpool_vm_git.0001.zst'),
                         'send -i @backup-20261017_033000 rpool/vm/git@backup-20261017_033001\n')

    def test_vm_with_an_image_file_is_refused_before_the_snapshots(self):
        self.system.set_xml("<domain><devices><disk type='file' device='disk'>"
                            "<source file='/a.qcow2'/></disk></devices></domain>")
        status, out = self.system.backup()
        self.assertEqual(status, 1)
        self.assertIn('not ZFS volumes', out)
        self.assertEqual(self.system.snapshots(), [])

    def test_unknown_owner_is_refused(self):
        out = io.StringIO()
        with contextlib.redirect_stderr(out):
            status = backup_vm.main(['git', self.system.backups, 'no-such-user-hd-backup'])
        self.assertEqual(status, 1)
        self.assertIn('does not exist', out.getvalue())

    def test_second_backup_of_the_same_vm_is_refused(self):
        lock_file = backup_vm.lock('git')
        try:
            status, out = self.system.backup()
        finally:
            lock_file.close()
        self.assertEqual(status, 1)
        self.assertIn('already running', out)


class TestBackupVms(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.config = os.path.join(tmp.name, 'config.json')
        self.log = os.path.join(tmp.name, 'log')
        script = os.path.join(tmp.name, 'hd-backup-vm')
        with open(script, 'w') as f:
            f.write('#!/bin/sh\necho "$@" >> %s\n[ "$1" != bad ]\n' % self.log)
        os.chmod(script, stat.S_IRWXU)

        for patch in (mock.patch.object(backup_vms, 'CONFIG_FILE', self.config),
                      mock.patch.object(backup_vms, 'BACKUP_VM', script)):
            patch.start()
            self.addCleanup(patch.stop)

    def run_with(self, config):
        with open(self.config, 'w') as f:
            json.dump(config, f)
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = backup_vms.main([])
        calls = []
        if os.path.exists(self.log):
            with open(self.log) as f:
                calls = f.read().splitlines()
        return status, calls, out.getvalue()

    def test_every_vm_is_backed_up(self):
        status, calls, out = self.run_with({'hd-backup': [
            {'vm': 'git', 'dir': '/dpool/backups/vm', 'owner': 'erik'},
            {'vm': 'web', 'dir': '/dpool/backups/vm', 'owner': 'erik'},
        ]})
        self.assertEqual(status, 0, out)
        self.assertEqual(calls, ['git /dpool/backups/vm erik', 'web /dpool/backups/vm erik'])

    def test_a_failure_does_not_stop_the_others(self):
        status, calls, out = self.run_with({'hd-backup': [
            {'vm': 'bad', 'dir': '/b', 'owner': 'erik'},
            {'vm': 'git', 'dir': '/b'},
            {'vm': 'web', 'dir': '/b', 'owner': 'erik'},
        ]})
        self.assertEqual(status, 1)
        self.assertEqual(calls, ['bad /b erik', 'web /b erik'])
        self.assertIn('Invalid entry', out)
        self.assertIn('Backups failed for: bad', out)

    def test_empty_list_does_nothing(self):
        status, calls, out = self.run_with({'hd-backup': []})
        self.assertEqual(status, 0)
        self.assertEqual(calls, [])

    def test_missing_or_wrong_key_fails(self):
        for config in ({}, {'hd-backup': {'vm': 'git'}}, []):
            status, calls, out = self.run_with(config)
            self.assertEqual(status, 1, config)
            self.assertIn('Error:', out)

    def test_missing_configuration_fails(self):
        out = io.StringIO()
        with contextlib.redirect_stderr(out):
            self.assertEqual(backup_vms.main([]), 1)
        self.assertIn('Cannot read', out.getvalue())


class TestRestoreVm(unittest.TestCase):
    def setUp(self):
        self.system = FakeSystem(self)
        self.assertEqual(self.system.backup()[0], 0)
        self.assertEqual(self.system.backup()[0], 0)
        # A stream of a disk that was removed from the VM.
        open(os.path.join(self.system.backups, 'git', 'rpool_vm_old.0000.zst'), 'w').close()
        self.backup_dir = os.path.join(self.system.backups, 'git') + '/'

    def restore(self, answers, *args):
        out = io.StringIO()
        with mock.patch('builtins.input', side_effect=answers), \
                contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            status = restore_vm.main([self.backup_dir, *args])
        return status, out.getvalue()

    def received(self, dataset):
        with open(os.path.join(self.system.state, 'received-' + dataset.replace('/', '_'))) as f:
            return f.read()

    def defined(self):
        with open(os.path.join(self.system.state, 'defined.xml')) as f:
            return f.read()

    def test_restore_in_place(self):
        status, out = self.restore(['', '', ''])
        self.assertEqual(status, 0, out)
        self.assertEqual(self.received('rpool/vm/git'),
                         'send rpool/vm/git@backup-20261017_033000\n'
                         'send -i @backup-20261017_033000 rpool/vm/git@backup-20261017_033001\n')
        self.assertIn('Skipping rpool_vm_old', out)
        xml = self.defined()
        self.assertIn('<name>git</name>', xml)
        self.assertIn('<uuid>', xml)
        self.assertIn("52:54:00:12:34:56", xml)
        self.assertIn('<qemu:commandline>', xml)
        self.assertIn("/dev/zvol/rpool/vm/git", xml)

    def test_restore_as_a_copy(self):
        os.remove(self.system.nvram)
        status, out = self.restore(['rpool/vm/git-copy', 'dpool/vm/git-copy-data', 'y'], 'git-copy')
        self.assertEqual(status, 0, out)
        self.assertIn('backup-20261017_033001', self.received('rpool/vm/git-copy'))
        self.assertIn('backup-20261017_033001', self.received('dpool/vm/git-copy-data'))
        xml = self.defined()
        new_nvram = os.path.join(os.path.dirname(self.system.nvram), 'git-copy_VARS.fd')
        self.assertIn('<name>git-copy</name>', xml)
        self.assertNotIn('<uuid>', xml)
        self.assertNotIn('<mac', xml)
        self.assertIn(f'<nvram>{new_nvram}</nvram>', xml)
        self.assertIn('/dev/zvol/rpool/vm/git-copy"', xml)
        self.assertIn('/dev/zvol/dpool/vm/git-copy-data"', xml)
        self.assertNotIn('/dev/zvol/rpool/vm/git"', xml)
        self.assertIn('<qemu:arg value="-no-hpet" />', xml)
        with open(new_nvram) as f:
            self.assertEqual(f.read(), 'nvram 1')
        self.assertFalse(os.path.exists(self.system.nvram))

    def test_missing_streams_are_refused(self):
        os.remove(os.path.join(self.backup_dir, 'dpool_vm_git-data.0000.zst'))
        os.remove(os.path.join(self.backup_dir, 'dpool_vm_git-data.0001.zst'))
        status, out = self.restore([])
        self.assertEqual(status, 1)
        self.assertIn('No stream files found for dpool/vm/git-data', out)

    def test_missing_xml_is_refused(self):
        os.remove(os.path.join(self.backup_dir, 'git.xml'))
        status, out = self.restore([])
        self.assertEqual(status, 1)
        self.assertIn('XML config file not found', out)

    def test_failed_receive_stops_the_restore(self):
        self.system.fail('zfs', 'receive')
        status, out = self.restore(['', '', ''])
        self.assertEqual(status, 1)
        self.assertIn('failed', out)
        self.assertFalse(os.path.exists(os.path.join(self.system.state, 'defined.xml')))


if __name__ == '__main__':
    unittest.main()
