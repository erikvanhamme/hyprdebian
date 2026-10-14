# Tests for scanning filesets (add-fileset, add-paths, update-filesets), on an in-memory database.

import hashlib
import os
import unittest

from helpers import DbTestCase, is_root, make_socket, quiet, write_file


class TestScan(DbTestCase):
    def setUp(self):
        super().setUp()
        self.root = self.path('data')
        write_file(self.path('data', 'f1'), 'one', mtime=1000000000)
        write_file(self.path('data', 'sub', 'f2'), 'two')
        os.symlink('f1', self.path('data', 'l1'))
        self.fileset = self.add_fileset('fs', self.root)

    def test_records_files_directories_and_symlinks(self):
        f1 = self.rows('SELECT sha1, size, mtime, on_disk FROM files WHERE path=?', (self.path('data', 'f1'),))
        self.assertEqual(f1, [(hashlib.sha1(b'one').hexdigest(), 3, 1000000000, 1)])
        sub = self.rows('SELECT sha1, size FROM files WHERE path=?', (self.path('data', 'sub'),))
        self.assertEqual(sub, [('', 0)])
        self.assertEqual(self.symlink_records(self.path('data', 'l1'))[0][1:], ('f1', 1))
        # Every item is linked to the fileset.
        self.assertEqual(self.value('SELECT COUNT(*) FROM file_fileset_links WHERE fileset_number=?',
                                    (self.fileset,)), 3)
        self.assertEqual(self.value('SELECT COUNT(*) FROM symlink_fileset_links WHERE fileset_number=?',
                                    (self.fileset,)), 1)

    def test_rescan_without_changes_adds_nothing(self):
        before = self.rows('SELECT * FROM files ORDER BY number')
        self.update('fs')
        self.assertEqual(self.rows('SELECT * FROM files ORDER BY number'), before)
        self.assertEqual(self.value('SELECT COUNT(*) FROM file_fileset_links'), 3)

    def test_changed_file_not_written_is_updated_in_place(self):
        number = self.file_records(self.path('data', 'f1'))[0][0]
        write_file(self.path('data', 'f1'), 'changed')
        self.update('fs')
        self.assertEqual(self.file_records(self.path('data', 'f1')), [(number, 7, 1)])
        self.assertEqual(self.value('SELECT sha1 FROM files WHERE number=?', (number,)),
                         hashlib.sha1(b'changed').hexdigest())

    def test_changed_file_already_written_gets_new_record(self):
        self.add_backup('bk', 'fs', 'c1')
        self.record_written('bk')
        write_file(self.path('data', 'f1'), 'changed')
        self.update('fs')
        records = self.file_records(self.path('data', 'f1'))
        self.assertEqual(len(records), 2)
        # Only the newest version is on disk.
        self.assertEqual([(size, on_disk) for number, size, on_disk in records], [(3, 0), (7, 1)])

    def test_deleted_file_not_written_is_removed(self):
        os.remove(self.path('data', 'f1'))
        self.update('fs')
        self.assertEqual(self.file_records(self.path('data', 'f1')), [])
        self.assertEqual(self.value('SELECT COUNT(*) FROM file_fileset_links WHERE file_number NOT IN '
                                    '(SELECT number FROM files)'), 0)

    def test_deleted_file_already_written_is_marked_not_on_disk(self):
        self.add_backup('bk', 'fs', 'c1')
        self.record_written('bk')
        os.remove(self.path('data', 'f1'))
        self.update('fs')
        self.assertEqual([on_disk for number, size, on_disk in self.file_records(self.path('data', 'f1'))], [0])

    def test_recreated_file_is_on_disk_and_old_record_is_not(self):
        self.add_backup('bk', 'fs', 'c1')
        self.record_written('bk')
        os.remove(self.path('data', 'f1'))
        self.update('fs')
        write_file(self.path('data', 'f1'), 'recreated')
        self.update('fs')
        records = self.file_records(self.path('data', 'f1'))
        self.assertEqual([(size, on_disk) for number, size, on_disk in records], [(3, 0), (9, 1)])

    def test_old_record_wrongly_on_disk_is_corrected(self):
        # Databases written by tool version 9 and older kept old versions of a changed file marked as on disk.
        self.add_backup('bk', 'fs', 'c1')
        self.record_written('bk')
        write_file(self.path('data', 'f1'), 'changed')
        self.update('fs')
        old_number = self.file_records(self.path('data', 'f1'))[0][0]
        self.db.execute('UPDATE files SET on_disk=1 WHERE number=?', (old_number,))
        self.update('fs')
        self.assertEqual([on_disk for number, size, on_disk in self.file_records(self.path('data', 'f1'))], [0, 1])

    def test_retargeted_symlink_not_written_is_updated(self):
        number = self.symlink_records(self.path('data', 'l1'))[0][0]
        os.remove(self.path('data', 'l1'))
        os.symlink('sub', self.path('data', 'l1'))
        self.update('fs')
        self.assertFalse(self.hb.had_errors)
        self.assertEqual(self.symlink_records(self.path('data', 'l1')), [(number, 'sub', 1)])

    def test_retargeted_symlink_already_written_gets_new_record(self):
        self.add_backup('bk', 'fs', 'c1')
        self.record_written('bk')
        os.remove(self.path('data', 'l1'))
        os.symlink('sub', self.path('data', 'l1'))
        self.update('fs')
        records = self.symlink_records(self.path('data', 'l1'))
        self.assertEqual([(target, on_disk) for number, target, on_disk in records], [('f1', 0), ('sub', 1)])

    def test_file_replaced_by_symlink(self):
        self.add_backup('bk', 'fs', 'c1')
        self.record_written('bk')
        os.remove(self.path('data', 'f1'))
        os.symlink('sub', self.path('data', 'f1'))
        self.update('fs')
        self.assertEqual([on_disk for number, size, on_disk in self.file_records(self.path('data', 'f1'))], [0])
        self.assertEqual([on_disk for number, target, on_disk in self.symlink_records(self.path('data', 'f1'))],
                         [1])

    def test_symlink_replaced_by_file(self):
        self.add_backup('bk', 'fs', 'c1')
        self.record_written('bk')
        os.remove(self.path('data', 'l1'))
        write_file(self.path('data', 'l1'), 'now a file')
        self.update('fs')
        self.assertEqual([on_disk for number, target, on_disk in self.symlink_records(self.path('data', 'l1'))],
                         [0])
        self.assertEqual([on_disk for number, size, on_disk in self.file_records(self.path('data', 'l1'))], [1])

    def test_names_with_special_characters(self):
        names = ['quote"name', "single'quote", 'back\\slash', 'new\nline', 'percent%_under', 'spa ce', 'é']
        for name in names:
            write_file(self.path('data', name), name)
        self.update('fs')
        self.assertFalse(self.hb.had_errors)
        for name in names:
            self.assertEqual(len(self.file_records(self.path('data', name))), 1, name)

    def test_fifo_socket_and_device_are_skipped(self):
        os.mkfifo(self.path('data', 'fifo'))
        make_socket(self.path('data', 'sock'))
        self.update('fs')
        self.assertFalse(self.hb.had_errors)
        self.assertEqual(self.file_records(self.path('data', 'fifo')), [])
        self.assertEqual(self.file_records(self.path('data', 'sock')), [])

    @unittest.skipIf(is_root(), 'root can read any file')
    def test_unreadable_new_file_is_skipped_with_error(self):
        write_file(self.path('data', 'secret'), 'x')
        os.chmod(self.path('data', 'secret'), 0)
        self.update('fs')
        self.assertTrue(self.hb.had_errors)
        self.assertEqual(self.file_records(self.path('data', 'secret')), [])
        # The rest of the scan went on.
        self.assertEqual(len(self.file_records(self.path('data', 'f1'))), 1)

    @unittest.skipIf(is_root(), 'root can read any directory')
    def test_unreadable_directory_is_reported(self):
        os.mkdir(self.path('data', 'closed'))
        os.chmod(self.path('data', 'closed'), 0)
        self.update('fs')
        self.assertTrue(self.hb.had_errors)

    def test_invalid_utf8_name_is_skipped_with_error(self):
        path = os.fsdecode(os.fsencode(self.path('data')) + b'/bad\xff')
        try:
            write_file(path, 'x')
        except OSError:
            self.skipTest('the file system does not allow names that are not valid UTF-8')
        self.update('fs')
        self.assertTrue(self.hb.had_errors)
        self.assertEqual(len(self.file_records(self.path('data', 'f1'))), 1)

    def test_unknown_fileset_is_an_error(self):
        self.update('nope')
        self.assertTrue(self.hb.had_errors)


class TestExclusions(DbTestCase):
    def setUp(self):
        super().setUp()
        write_file(self.path('data', 'keep'), 'k')
        write_file(self.path('data', 'cache', 'deep', 'c'), 'c')
        write_file(self.path('data', 'cachefile'), 'c')

    def test_excluded_directory_is_not_recorded(self):
        self.add_fileset('fs', self.path('data'), '^' + self.path('data', 'cache'))
        paths = [row[0] for row in self.rows('SELECT path FROM files')]
        self.assertIn(self.path('data', 'keep'), paths)
        self.assertNotIn(self.path('data', 'cache'), paths)
        self.assertNotIn(self.path('data', 'cache', 'deep', 'c'), paths)
        # Documented: an exclusion matches every path that starts with the same text.
        self.assertNotIn(self.path('data', 'cachefile'), paths)

    def test_excluded_directory_is_not_entered(self):
        os.chmod(self.path('data', 'cache', 'deep'), 0)
        self.add_fileset('fs', self.path('data'), '^' + self.path('data', 'cache'))
        self.assertFalse(self.hb.had_errors)

    def test_excluded_path_of_fileset_is_not_scanned(self):
        self.add_fileset('fs', self.path('data'), '^' + self.path('data'))
        self.assertEqual(self.value('SELECT COUNT(*) FROM files'), 0)

    def test_paths_are_stored_absolute(self):
        old_cwd = os.getcwd()
        os.chdir(self.tmp)
        try:
            self.add_fileset('fs', 'data', '^data/cache')
        finally:
            os.chdir(old_cwd)
        self.assertEqual(sorted(row[0] for row in self.rows('SELECT path FROM paths')),
                         sorted([self.path('data'), '^' + self.path('data', 'cache')]))


class TestOverlappingFilesets(DbTestCase):
    def test_removed_item_leaves_no_links_in_other_filesets(self):
        write_file(self.path('data', 'f'), 'x')
        self.add_fileset('one', self.path('data'))
        self.add_fileset('two', self.path('data'))
        os.remove(self.path('data', 'f'))
        self.update('one')
        self.assertEqual(self.value('SELECT COUNT(*) FROM file_fileset_links WHERE file_number NOT IN '
                                    '(SELECT number FROM files)'), 0)


class TestFilesetCommands(DbTestCase):
    def test_add_fileset_refuses_missing_directory(self):
        with quiet(), self.assertRaises(SystemExit) as raised:
            self.hb.add_fileset(self.db, 'fs', [self.path('missing')])
        self.assertEqual(raised.exception.code, 1)
        self.assertEqual(self.value('SELECT COUNT(*) FROM filesets'), 0)

    def test_add_fileset_refuses_duplicate_name(self):
        os.mkdir(self.path('data'))
        self.add_fileset('fs', self.path('data'))
        with quiet(), self.assertRaises(SystemExit):
            self.hb.add_fileset(self.db, 'fs', [self.path('data')])

    def test_names_that_are_column_names(self):
        # Names were once put in double quotes in the SQL, where SQLite reads "name" as the column name.
        os.mkdir(self.path('data'))
        self.add_fileset('other', self.path('data'))
        self.add_fileset('name', self.path('data'))
        self.add_fileset('number', self.path('data'))
        self.assertEqual(self.value('SELECT COUNT(*) FROM filesets'), 3)

    def test_add_paths_to_unknown_fileset(self):
        os.mkdir(self.path('data'))
        with quiet(), self.assertRaises(SystemExit):
            self.hb.add_paths(self.db, 'nope', [self.path('data')])

    def test_add_paths_scans_the_new_path(self):
        write_file(self.path('a', 'f'), 'x')
        write_file(self.path('b', 'g'), 'y')
        self.add_fileset('fs', self.path('a'))
        with quiet():
            self.hb.add_paths(self.db, 'fs', [self.path('b')])
        self.assertEqual(len(self.file_records(self.path('b', 'g'))), 1)
