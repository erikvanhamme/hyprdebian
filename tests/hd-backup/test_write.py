# Tests for the parts of a write that do not run tar, and for the backup and carrier commands, on an in-memory
# database.

import os
import shutil
import stat
import subprocess
import sys

from helpers import DbTestCase, capture, quiet, write_file


class WriteTestCase(DbTestCase):
    def setUp(self):
        super().setUp()
        write_file(self.path('data', 'f1'), 'one')
        write_file(self.path('data', 'dir', 'sub', 'f2'), 'two')
        os.symlink('f1', self.path('data', 'l1'))
        self.fileset = self.add_fileset('fs', self.path('data'))
        self.backup = self.add_backup('bk', 'fs', 'c1', 'c2')

    def deletelist(self, backup='bk'):
        backup_number, fileset_number = self.rows('SELECT number, fileset_number FROM backups WHERE name=?',
                                                  (backup,))[0]
        name, size = self.hb.write_create_deletelist(self.db, fileset_number, backup_number)
        with open(name) as file:
            source = file.read()
        namespace = {'__file__': name}
        exec(source.split('\nfor path in paths:')[0], namespace)
        return name, size, namespace['paths']


class TestDeleteList(WriteTestCase):
    def test_empty_without_deletes(self):
        self.record_written('bk')
        name, size, paths = self.deletelist()
        self.assertEqual(paths, [])
        self.assertEqual(size, os.path.getsize(name))
        self.assertEqual(stat.S_IMODE(os.stat(name).st_mode), 0o755)

    def test_lists_deleted_items_children_first(self):
        self.record_written('bk')
        shutil.rmtree(self.path('data', 'dir'))
        os.remove(self.path('data', 'l1'))
        self.update('fs')
        name, size, paths = self.deletelist()
        self.assertEqual(paths, [self.path('data', 'l1'), self.path('data', 'dir', 'sub', 'f2'),
                                 self.path('data', 'dir', 'sub'), self.path('data', 'dir')])

    def test_leaves_out_recreated_paths(self):
        self.record_written('bk')
        os.remove(self.path('data', 'f1'))
        self.update('fs')
        write_file(self.path('data', 'f1'), 'back again')
        self.update('fs')
        name, size, paths = self.deletelist()
        self.assertEqual(paths, [])

    def test_leaves_out_items_never_written_to_this_backup(self):
        self.record_written('bk')
        self.add_backup('other', 'fs', 'o1')
        write_file(self.path('data', 'late'), 'x')
        self.update('fs')
        self.record_written('bk')
        os.remove(self.path('data', 'late'))
        self.update('fs')
        name, size, paths = self.deletelist('other')
        self.assertEqual(paths, [])
        name, size, paths = self.deletelist('bk')
        self.assertEqual(paths, [self.path('data', 'late')])

    def test_leaves_out_other_filesets(self):
        write_file(self.path('other', 'o'), 'x')
        self.add_fileset('fs2', self.path('other'))
        self.add_backup('bk2', 'fs2', 'x1')
        self.record_written('bk')
        self.record_written('bk2')
        os.remove(self.path('other', 'o'))
        self.update('fs2')
        name, size, paths = self.deletelist('bk')
        self.assertEqual(paths, [])

    def test_special_characters_survive(self):
        name = 'q"uote\\back\nnew'
        write_file(self.path('data', name), 'x')
        self.update('fs')
        self.record_written('bk')
        os.remove(self.path('data', name))
        self.update('fs')
        deletelist, size, paths = self.deletelist()
        self.assertEqual(paths, [self.path('data', name)])

    def test_script_removes_paths_in_restore_directory(self):
        self.record_written('bk')
        shutil.rmtree(self.path('data', 'dir'))
        os.remove(self.path('data', 'l1'))
        os.remove(self.path('data', 'f1'))
        self.update('fs')
        deletelist, size, paths = self.deletelist()

        # A restore: the delete list ends up in R/tmp, the backed up paths under R. One deleted path was not
        # restored, and one deleted directory holds a file that is not in the backup.
        restore = self.path('restore')
        os.makedirs(os.path.join(restore, 'tmp'))
        shutil.copy(deletelist, os.path.join(restore, 'tmp'))
        restored = lambda *parts: os.path.join(restore, self.path('data', *parts).lstrip('/'))
        write_file(restored('dir', 'sub', 'f2'), 'two')
        write_file(restored('dir', 'not-backed-up'), 'x')
        os.symlink('f1', restored('l1'))
        write_file(restored('kept'), 'x')

        # Runs from another directory: the paths are relative to the script.
        result = subprocess.run([sys.executable, os.path.join(restore, 'tmp', os.path.basename(deletelist))],
                                capture_output=True, text=True, cwd=self.tmp)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(os.path.lexists(restored('l1')))
        self.assertFalse(os.path.exists(restored('dir', 'sub')))
        self.assertTrue(os.path.exists(restored('dir', 'not-backed-up')))
        self.assertTrue(os.path.exists(restored('kept')))
        self.assertIn('Could not remove', result.stdout)
        self.assertIn(restored('dir'), result.stdout)


class TestFileList(WriteTestCase):
    def read_filelist(self, name):
        with open(name, 'rb') as file:
            return file.read().split(b'\0')

    def test_order_and_separator(self):
        with quiet():
            name, symlinks, files = self.hb.write_create_filelist(self.db, self.fileset, self.backup,
                                                                  '/tmp/deletes_x.py')
        entries = self.read_filelist(name)
        self.assertEqual(entries[-1], b'')
        expected = ['/tmp/deletes_x.py', self.path('data', 'l1')] + sorted(
            [self.path('data', 'f1'), self.path('data', 'dir'), self.path('data', 'dir', 'sub'),
             self.path('data', 'dir', 'sub', 'f2')])
        self.assertEqual(entries[:-1], [os.fsencode(path.lstrip('/')) for path in expected])
        self.assertEqual([symlink[1:] for symlink in symlinks], [(self.path('data', 'l1'), 'f1')])

    def test_leaves_out_what_is_written(self):
        self.record_written('bk')
        write_file(self.path('data', 'new'), 'x')
        self.update('fs')
        with quiet():
            name, symlinks, files = self.hb.write_create_filelist(self.db, self.fileset, self.backup,
                                                                  '/tmp/deletes_x.py')
        # Only the new file. The path of the fileset itself is not recorded, only what is in it.
        self.assertEqual([file[1] for file in files], [self.path('data', 'new')])
        self.assertEqual(symlinks, [])

    def test_only_newest_version_of_changed_file(self):
        self.record_written('bk')
        self.add_backup('fresh', 'fs', 'n1')
        write_file(self.path('data', 'f1'), 'changed')
        self.update('fs')
        fresh = self.value('SELECT number FROM backups WHERE name=?', ('fresh',))
        with quiet():
            name, symlinks, files = self.hb.write_create_filelist(self.db, self.fileset, fresh, '/tmp/deletes_x.py')
        self.assertEqual([file[1] for file in files].count(self.path('data', 'f1')), 1)

    def test_uses_current_size_when_file_grew(self):
        write_file(self.path('data', 'f1'), 'x' * 1000)
        with quiet():
            name, symlinks, files = self.hb.write_create_filelist(self.db, self.fileset, self.backup,
                                                                  '/tmp/deletes_x.py')
        self.assertIn(1000, [file[2] for file in files if file[1] == self.path('data', 'f1')])

    def test_fails_when_file_is_gone(self):
        os.remove(self.path('data', 'f1'))
        with quiet(), self.assertRaises(SystemExit) as raised:
            self.hb.write_create_filelist(self.db, self.fileset, self.backup, '/tmp/deletes_x.py')
        self.assertEqual(raised.exception.code, 1)

    def test_fails_when_symlink_is_gone(self):
        os.remove(self.path('data', 'l1'))
        with quiet(), self.assertRaises(SystemExit):
            self.hb.write_create_filelist(self.db, self.fileset, self.backup, '/tmp/deletes_x.py')


class TestWritePrepare(WriteTestCase):
    def test_first_carrier_and_index(self):
        with quiet():
            backup, fileset, carrier, index = self.hb.write_prepare(self.db, 'bk', self.tmp)
        c1 = self.value("SELECT number FROM carrier WHERE label='c1'")
        self.assertEqual((backup, fileset, carrier, index), (self.backup, self.fileset, c1, 0))

    def test_next_index_on_same_carrier(self):
        self.record_written('bk')
        with quiet():
            backup, fileset, carrier, index = self.hb.write_prepare(self.db, 'bk', self.tmp)
        self.assertEqual(index, 1)

    def test_next_carrier_when_full(self):
        self.record_written('bk', full=True)
        with quiet():
            backup, fileset, carrier, index = self.hb.write_prepare(self.db, 'bk', self.tmp)
        self.assertEqual((carrier, index), (self.value("SELECT number FROM carrier WHERE label='c2'"), 0))

    def test_carrier_order_is_by_label(self):
        # Documented: a write goes to the first carrier that is not full, by label.
        self.add_backup('order', 'fs', 'b-2', 'b-1')
        with quiet():
            backup, fileset, carrier, index = self.hb.write_prepare(self.db, 'order', self.tmp)
        self.assertEqual(carrier, self.value("SELECT number FROM carrier WHERE label='b-1'"))

    def test_fails_when_all_carriers_full(self):
        self.record_written('bk', full=True)
        self.record_written('bk', full=True)
        with quiet(), self.assertRaises(SystemExit):
            self.hb.write_prepare(self.db, 'bk', self.tmp)

    def test_fails_for_unknown_backup_or_target(self):
        with quiet(), self.assertRaises(SystemExit):
            self.hb.write_prepare(self.db, 'nope', self.tmp)
        with quiet(), self.assertRaises(SystemExit):
            self.hb.write_prepare(self.db, 'bk', self.path('missing'))


class TestWriteUpdateDb(WriteTestCase):
    def test_records_tarball_and_links(self):
        carrier = self.value("SELECT number FROM carrier WHERE label='c1'")
        files = self.rows('SELECT number, path, size FROM files')
        symlinks = self.rows('SELECT number, path, target FROM symlinks')
        with quiet():
            self.hb.write_update_db(self.db, carrier, 0, 6, 100, symlinks, files, False)
        self.assertEqual(self.rows('SELECT carrier_number, idx, size, files, symlinks, overhead FROM tarballs'),
                         [(carrier, 0, 6, len(files), 1, 100)])
        self.assertEqual(self.value('SELECT COUNT(*) FROM file_tarball_links'), len(files))
        self.assertEqual(self.value('SELECT COUNT(*) FROM symlink_tarball_links'), 1)
        self.assertEqual(self.value('SELECT full FROM carrier WHERE number=?', (carrier,)), 0)

    def test_marks_carrier_full(self):
        carrier = self.value("SELECT number FROM carrier WHERE label='c1'")
        with quiet():
            self.hb.write_update_db(self.db, carrier, 0, 0, 0, [], [], True)
        self.assertEqual(self.value('SELECT full FROM carrier WHERE number=?', (carrier,)), 1)


class TestBackupCommands(WriteTestCase):
    def test_add_backup_refuses_duplicate_name(self):
        with quiet(), self.assertRaises(SystemExit):
            self.hb.add_backup(self.db, 'bk', 'fs')
        self.assertEqual(self.value("SELECT COUNT(*) FROM backups WHERE name='bk'"), 1)

    def test_add_backup_refuses_unknown_fileset(self):
        with quiet(), self.assertRaises(SystemExit):
            self.hb.add_backup(self.db, 'new', 'nope')

    def test_add_carrier_refuses_duplicate_label_and_goes_on(self):
        with quiet():
            self.hb.add_carrier(self.db, 'bk', ['c1', 'c3'])
        self.assertTrue(self.hb.had_errors)
        self.assertEqual(self.rows("SELECT label, idx FROM carrier WHERE backup_number=? ORDER BY idx",
                                   (self.backup,)), [('c1', 0), ('c2', 1), ('c3', 2)])

    def test_rename_backup(self):
        with quiet():
            self.hb.rename_backup(self.db, 'bk', 'renamed')
        self.assertEqual(self.value('SELECT name FROM backups WHERE number=?', (self.backup,)), 'renamed')

    def test_rename_backup_refuses_name_in_use(self):
        self.add_backup('other', 'fs')
        with quiet(), self.assertRaises(SystemExit):
            self.hb.rename_backup(self.db, 'bk', 'other')
        with quiet(), self.assertRaises(SystemExit):
            self.hb.rename_backup(self.db, 'nope', 'new')

    def test_remove_carrier_removes_tarballs_and_links(self):
        self.record_written('bk')
        with quiet():
            self.hb.remove_carrier(self.db, ['c1', 'nope'])
        self.assertTrue(self.hb.had_errors)
        self.assertEqual(self.rows("SELECT label FROM carrier"), [('c2',)])
        for table in ('tarballs', 'file_tarball_links', 'symlink_tarball_links'):
            self.assertEqual(self.value(f'SELECT COUNT(*) FROM {table}'), 0, table)

    def test_remove_backups(self):
        self.record_written('bk')
        self.add_backup('keep', 'fs', 'k1')
        with quiet():
            self.hb.remove_backups(self.db, ['bk'])
        self.assertFalse(self.hb.had_errors)
        self.assertEqual(self.rows('SELECT name FROM backups'), [('keep',)])
        self.assertEqual(self.rows('SELECT label FROM carrier'), [('k1',)])
        self.assertEqual(self.value('SELECT COUNT(*) FROM tarballs'), 0)
        # The fileset stays.
        self.assertEqual(self.value('SELECT COUNT(*) FROM filesets'), 1)

    def test_remove_unknown_backup_is_an_error(self):
        with quiet():
            self.hb.remove_backups(self.db, ['nope'])
        self.assertTrue(self.hb.had_errors)


class TestSummary(WriteTestCase):
    def summary(self, *names):
        output, redirect = capture()
        with redirect:
            self.hb.summary(self.db, list(names))
        return output.getvalue()

    def test_all_backups_without_names(self):
        self.add_backup('other', 'fs')
        output = self.summary()
        self.assertIn('Backup set bk:', output)
        self.assertIn('Backup set other:', output)

    def test_wildcard_and_literal_characters(self):
        self.add_backup('b.k', 'fs')
        self.add_backup('bxk-2', 'fs')
        output = self.summary('b.k*')
        self.assertIn('Backup set b.k:', output)
        self.assertNotIn('Backup set bxk-2:', output)
        self.assertNotIn('Backup set bk:', output)
        self.assertIn('Backup set bk:', self.summary('b*'))
        self.assertNotIn('Backup set bk:', self.summary('k*'))

    def test_status_counts_only_current_versions(self):
        self.record_written('bk')
        output = self.summary('bk')
        self.assertIn('All files of the fileset are on carrier', output)
        write_file(self.path('data', 'f1'), 'changed!')
        self.update('fs')
        output = self.summary('bk')
        # Only the new version of the changed file is not on the carrier yet. The old version does not count.
        self.assertIn('There are 1 files not on carrier in the backup set for a size of 8 bytes', output)
        # The fileset: f1, dir, dir/sub and dir/sub/f2, with the new size of f1.
        self.assertIn('stores 4 files for a total size of 11 bytes', output)


class TestManifestAndLists(WriteTestCase):
    def test_manifest(self):
        self.record_written('bk')
        output, redirect = capture()
        with redirect:
            self.hb.manifest(self.db, 'c1')
        output = output.getvalue()
        self.assertIn('carrier is not full', output)
        self.assertIn(self.path('data', 'f1'), output)
        self.assertIn(f'{self.path("data", "l1")} [Symlink]', output)
        self.assertIn(f'{self.path("data", "dir")} [Directory]', output)

    def test_manifest_of_unknown_carrier(self):
        with quiet(), self.assertRaises(SystemExit):
            self.hb.manifest(self.db, 'nope')

    def test_lists(self):
        self.record_written('bk')
        for function in (self.hb.list_filesets, self.hb.list_backups, self.hb.list_carrier, self.hb.list_tarballs):
            output, redirect = capture()
            with redirect:
                function(self.db)
            self.assertIn('|', output.getvalue(), function.__name__)
