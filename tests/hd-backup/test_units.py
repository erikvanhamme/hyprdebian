# Unit tests for the helper functions of hd-backup that need no database.

import hashlib
import os
import stat
import types

from unittest import mock
import stat

from helpers import TempDirTestCase, load_hd_backup, make_socket, write_file

hb = load_hd_backup()


class TestTarBlocks(TempDirTestCase):
    def test_rounds_up_to_512(self):
        self.assertEqual(hb.tar_blocks(0), 0)
        self.assertEqual(hb.tar_blocks(1), 512)
        self.assertEqual(hb.tar_blocks(511), 512)
        self.assertEqual(hb.tar_blocks(512), 512)
        self.assertEqual(hb.tar_blocks(513), 1024)


def fake_status(kind='file', uid=1000, gid=1000, time_ns=1700000000_123456789):
    # The fields of an lstat result that the estimate reads.
    modes = {'file': stat.S_IFREG, 'dir': stat.S_IFDIR, 'symlink': stat.S_IFLNK}
    return types.SimpleNamespace(st_mode=modes[kind] | 0o644, st_uid=uid, st_gid=gid, st_mtime_ns=time_ns,
                                 st_atime_ns=time_ns, st_ctime_ns=time_ns)


# The three time records with 10 digit seconds and 9 decimals: "30 mtime=1700000000.123456789\n".
TIMES = 3 * 30


class TestPaxRecordSize(TempDirTestCase):
    def test_matches_records_of_tar(self):
        for record in ['30 mtime=1700000000.123456789\n', '20 atime=1700000000\n', '11 atime=0\n',
                       '14 mtime=-1.5\n', '31 mtime=99999999999.123456789\n']:
            key, value = record.split(' ', 1)[1].rstrip('\n').split('=', 1)
            self.assertEqual(hb.pax_record_size(key, len(value)), len(record), record)

    def test_length_digits_grow_with_the_record(self):
        # "99 path=<90 bytes>\n" is 99 bytes. One byte more needs a third digit: 101 bytes.
        self.assertEqual(hb.pax_record_size('path', 90), 99)
        self.assertEqual(hb.pax_record_size('path', 91), 101)
        self.assertEqual(hb.pax_record_size('path', 991), 1002)


class TestPaxTimeSize(TempDirTestCase):
    def test_counts_all_decimals(self):
        self.assertEqual(hb.pax_time_size('mtime', 1700000000_123456789), 30)
        # tar writes "20 atime=1700000000\n" for a whole second, but the time can change before tar reads it.
        self.assertEqual(hb.pax_time_size('atime', 1700000000_000000000), 30)

    def test_short_negative_and_long_times(self):
        self.assertEqual(hb.pax_time_size('mtime', 0), len('21 mtime=0.123456789\n'))
        self.assertEqual(hb.pax_time_size('mtime', -1_500000000), len('22 mtime=-1.123456789\n'))
        self.assertEqual(hb.pax_time_size('mtime', 99999999999_000000000), 31)


class TestPaxDataSize(TempDirTestCase):
    def test_short_path_has_only_times(self):
        self.assertEqual(hb.pax_data_size('/a/b', fake_status(), 1), TIMES)

    def test_path_record_over_100_bytes(self):
        # The name in the tarball has no leading /: a path of 101 bytes is a name of 100.
        self.assertEqual(hb.pax_data_size('/' + 'x' * 100, fake_status(), 1), TIMES)
        self.assertEqual(hb.pax_data_size('/' + 'x' * 101, fake_status(), 1), TIMES + hb.pax_record_size('path', 101))

    def test_directory_name_ends_with_slash(self):
        self.assertEqual(hb.pax_data_size('/' + 'x' * 100, fake_status('dir'), 0),
                         TIMES + hb.pax_record_size('path', 101))

    def test_path_record_for_names_that_are_not_ascii(self):
        self.assertEqual(hb.pax_data_size('/é', fake_status(), 1), TIMES + hb.pax_record_size('path', 2))

    def test_linkpath_record(self):
        self.assertEqual(hb.pax_data_size('/l', fake_status('symlink'), 0, 't' * 100), TIMES)
        self.assertEqual(hb.pax_data_size('/l', fake_status('symlink'), 0, 't' * 101),
                         TIMES + hb.pax_record_size('linkpath', 101))
        self.assertEqual(hb.pax_data_size('/l', fake_status('symlink'), 0, 'ü'),
                         TIMES + hb.pax_record_size('linkpath', 2))

    def test_size_record_from_8_gib(self):
        self.assertEqual(hb.pax_data_size('/f', fake_status(), 0o77777777777), TIMES)
        self.assertEqual(hb.pax_data_size('/f', fake_status(), 0o77777777777 + 1),
                         TIMES + hb.pax_record_size('size', 10))

    def test_uid_and_gid_records_over_ustar_limit(self):
        with mock.patch.object(hb, 'user_name', lambda uid: None), \
                mock.patch.object(hb, 'group_name', lambda gid: None):
            self.assertEqual(hb.pax_data_size('/f', fake_status(uid=2097151, gid=2097151), 1), TIMES)
            self.assertEqual(hb.pax_data_size('/f', fake_status(uid=2097152, gid=3000000), 1),
                             TIMES + hb.pax_record_size('uid', 7) + hb.pax_record_size('gid', 7))

    def test_owner_name_records(self):
        with mock.patch.object(hb, 'user_name', lambda uid: 'erik'), \
                mock.patch.object(hb, 'group_name', lambda gid: 'g' * 40):
            self.assertEqual(hb.pax_data_size('/f', fake_status(), 1), TIMES + hb.pax_record_size('gname', 40))
        with mock.patch.object(hb, 'user_name', lambda uid: 'éric'), \
                mock.patch.object(hb, 'group_name', lambda gid: 'users'):
            self.assertEqual(hb.pax_data_size('/f', fake_status(), 1), TIMES + hb.pax_record_size('uname', 5))


class TestOwnerNames(TempDirTestCase):
    def test_known_and_unknown_ids(self):
        self.assertEqual(hb.user_name(0), 'root')
        self.assertEqual(hb.group_name(0), 'root')
        # tar writes no name for an id without one, so there is no record for it.
        self.assertIsNone(hb.user_name(3999999))
        self.assertIsNone(hb.group_name(3999999))
        self.assertEqual(hb.owner_name_size(None), 0)


class TestTarItemSize(TempDirTestCase):
    def test_headers_and_contents(self):
        # Extended header block, one block of extended header data, ustar header block, then the contents.
        self.assertEqual(hb.tar_item_size('/a', fake_status(), 0), 3 * 512)
        self.assertEqual(hb.tar_item_size('/a', fake_status(), 1), 4 * 512)
        self.assertEqual(hb.tar_item_size('/a', fake_status(), 512), 4 * 512)
        self.assertEqual(hb.tar_item_size('/a', fake_status(), 513), 5 * 512)

    def test_extended_header_data_over_one_block(self):
        # The times take 90 bytes, so a path record of 422 bytes fills the first block: a name of 412 bytes.
        self.assertEqual(hb.pax_record_size('path', 412), 512 - TIMES)
        self.assertEqual(hb.tar_item_size('/' + 'x' * 412, fake_status(), 0), 3 * 512)
        self.assertEqual(hb.tar_item_size('/' + 'x' * 413, fake_status(), 0), 4 * 512)


class TestIsExcluded(TempDirTestCase):
    def test_no_exclusions(self):
        self.assertFalse(hb.is_excluded('/home/erik/file', []))

    def test_prefix_match(self):
        excludes = ['/home/erik/.cache']
        self.assertTrue(hb.is_excluded('/home/erik/.cache', excludes))
        self.assertTrue(hb.is_excluded('/home/erik/.cache/x/y', excludes))
        self.assertFalse(hb.is_excluded('/home/erik/.config', excludes))

    def test_documented_prefix_behaviour(self):
        # Documented: ^/home/erik/tmp also excludes /home/erik/tmpfiles.
        self.assertTrue(hb.is_excluded('/home/erik/tmpfiles', ['/home/erik/tmp']))


class TestFileSha1(TempDirTestCase):
    def test_file_digest(self):
        path = write_file(self.path('f'), b'abc' * 10000)
        self.assertEqual(hb.file_sha1(path), hashlib.sha1(b'abc' * 10000).hexdigest())

    def test_empty_file(self):
        path = write_file(self.path('f'))
        self.assertEqual(hb.file_sha1(path), hashlib.sha1(b'').hexdigest())

    def test_directory_has_no_digest(self):
        self.assertEqual(hb.file_sha1(self.tmp), '')


def file_attributes(size, mtime):
    return hb.Attributes(False, None, size, mtime, False, None)


def symlink_attributes(target):
    return hb.Attributes(True, target, None, None, None, None)


class TestFindMatch(TempDirTestCase):
    records = {
        hb.FILE: {'/f': [{'number': 1, 'size': 10, 'mtime': 100, 'on_disk': 0},
                         {'number': 2, 'size': 20, 'mtime': 200, 'on_disk': 1}],
                  '/l': [{'number': 3, 'size': 0, 'mtime': 0, 'on_disk': 0}]},
        hb.SYMLINK: {'/l': [{'number': 7, 'target': 'a', 'on_disk': 1}]},
    }

    def test_file_matches_on_size_and_mtime(self):
        self.assertEqual(hb.find_match(self.records, '/f', file_attributes(10, 100)), 1)
        self.assertEqual(hb.find_match(self.records, '/f', file_attributes(20, 200)), 2)

    def test_file_with_other_size_or_mtime_does_not_match(self):
        self.assertIsNone(hb.find_match(self.records, '/f', file_attributes(10, 200)))
        self.assertIsNone(hb.find_match(self.records, '/f', file_attributes(11, 100)))

    def test_symlink_matches_on_target(self):
        self.assertEqual(hb.find_match(self.records, '/l', symlink_attributes('a')), 7)
        self.assertIsNone(hb.find_match(self.records, '/l', symlink_attributes('b')))

    def test_only_records_of_the_same_kind_match(self):
        self.assertIsNone(hb.find_match(self.records, '/f', symlink_attributes('a')))
        self.assertEqual(hb.find_match(self.records, '/l', file_attributes(0, 0)), 3)

    def test_unknown_path(self):
        self.assertIsNone(hb.find_match(self.records, '/x', file_attributes(10, 100)))
        self.assertIsNone(hb.find_match(self.records, '/x', symlink_attributes('a')))


class TestGetAttributes(TempDirTestCase):
    def test_regular_file(self):
        path = write_file(self.path('f'), b'12345', mtime=1000000000)
        is_symlink, target, size, mtime, hardlink, unsupported = hb.get_attributes(path)
        self.assertEqual((is_symlink, target, size, mtime, hardlink, unsupported),
                         (False, None, 5, 1000000000, False, None))

    def test_hardlinked_file(self):
        path = write_file(self.path('f'), b'x')
        os.link(path, self.path('g'))
        self.assertTrue(hb.get_attributes(path)[4])

    def test_directory(self):
        os.mkdir(self.path('d'))
        os.utime(self.path('d'), (1000000000, 1000000000))
        is_symlink, target, size, mtime, hardlink, unsupported = hb.get_attributes(self.path('d'))
        self.assertEqual((is_symlink, size, mtime, unsupported), (False, 0, 1000000000, None))

    def test_symlink_is_not_followed(self):
        os.mkdir(self.path('d'))
        os.symlink('d', self.path('l'))
        is_symlink, target, size, mtime, hardlink, unsupported = hb.get_attributes(self.path('l'))
        self.assertEqual((is_symlink, target, size, mtime, unsupported), (True, 'd', None, None, None))

    def test_broken_symlink(self):
        os.symlink('missing', self.path('l'))
        self.assertEqual(hb.get_attributes(self.path('l'))[:2], (True, 'missing'))

    def test_fifo(self):
        os.mkfifo(self.path('p'))
        self.assertEqual(hb.get_attributes(self.path('p'))[5], 'FIFO')

    def test_socket(self):
        make_socket(self.path('s'))
        self.assertEqual(hb.get_attributes(self.path('s'))[5], 'socket')

    def test_device(self):
        self.assertTrue(stat.S_ISCHR(os.lstat('/dev/null').st_mode))
        self.assertEqual(hb.get_attributes('/dev/null')[5], 'device file')

    def test_missing_path_raises(self):
        with self.assertRaises(FileNotFoundError):
            hb.get_attributes(self.path('missing'))


class TestCommandLine(TempDirTestCase):
    def test_usage_lists_every_command(self):
        text = hb.generate_usage_text(hb.COMMANDS)
        for command in hb.COMMANDS:
            self.assertIn(f'  {command.name}', text)
            self.assertIn(command.help, text)

    def test_every_command_has_a_function(self):
        for command in hb.COMMANDS:
            self.assertTrue(callable(command.function), command.name)
            # The function takes the database and the arguments.
            self.assertEqual(command.function.__code__.co_argcount, 1 + len(command.arguments), command.name)

    def test_parser_passes_arguments_in_order(self):
        parser = hb.build_parser(hb.COMMANDS)
        args = parser.parse_args(['add-fileset', 'fs', '/a', '^/a/b'])
        command = args.command_info
        self.assertIs(command.function, hb.add_fileset)
        self.assertEqual([getattr(args, argument.name) for argument in command.arguments], ['fs', ['/a', '^/a/b']])
        args = parser.parse_args(['summary'])
        self.assertEqual(args.backup_names, [])
