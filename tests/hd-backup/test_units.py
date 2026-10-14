# Unit tests for the helper functions of hd-backup that need no database.

import hashlib
import os
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


class TestTarItemSize(TempDirTestCase):
    def test_short_path_empty_item(self):
        # Extended header block, one block of extended header data and a ustar header block.
        self.assertEqual(hb.tar_item_size('/a', 0), 3 * 512)

    def test_data_is_padded_to_blocks(self):
        self.assertEqual(hb.tar_item_size('/a', 1), 4 * 512)
        self.assertEqual(hb.tar_item_size('/a', 512), 4 * 512)
        self.assertEqual(hb.tar_item_size('/a', 513), 5 * 512)

    def test_long_path_takes_more_header_data(self):
        self.assertGreater(hb.tar_item_size('/' + 'x' * 600, 0), hb.tar_item_size('/x', 0))

    def test_long_link_target_takes_more_header_data(self):
        self.assertGreater(hb.tar_item_size('/l', 0, 't' * 600), hb.tar_item_size('/l', 0, 't'))

    def test_counts_bytes_not_characters(self):
        # 'é' is 2 bytes in UTF-8: 150 of them do not fit with the margin in one block, 150 'e' do.
        self.assertEqual(hb.tar_item_size('/' + 'e' * 150, 0), 3 * 512)
        self.assertEqual(hb.tar_item_size('/' + 'é' * 150, 0), 4 * 512)


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
