# Unit tests for the helper functions of hd-backup that need no database.

import hashlib
import json
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


class TestSha1(TempDirTestCase):
    def test_file_digest(self):
        path = write_file(self.path('f'), b'abc' * 10000)
        self.assertEqual(hb.sha1(path), hashlib.sha1(b'abc' * 10000).hexdigest())

    def test_empty_file(self):
        path = write_file(self.path('f'))
        self.assertEqual(hb.sha1(path), hashlib.sha1(b'').hexdigest())

    def test_directory_has_no_digest(self):
        self.assertEqual(hb.sha1(self.tmp), '')


class TestFindMatch(TempDirTestCase):
    files = {'/f': [{'number': 1, 'size': 10, 'mtime': 100, 'on_disk': 0},
                    {'number': 2, 'size': 20, 'mtime': 200, 'on_disk': 1}]}
    symlinks = {'/l': [{'number': 7, 'target': 'a', 'on_disk': 1}]}

    def test_file_matches_on_size_and_mtime(self):
        self.assertEqual(hb.update_fileset_find_match(False, '/f', self.files, self.symlinks, None, 10, 100), 1)
        self.assertEqual(hb.update_fileset_find_match(False, '/f', self.files, self.symlinks, None, 20, 200), 2)

    def test_file_with_other_size_or_mtime_does_not_match(self):
        self.assertIsNone(hb.update_fileset_find_match(False, '/f', self.files, self.symlinks, None, 10, 200))
        self.assertIsNone(hb.update_fileset_find_match(False, '/f', self.files, self.symlinks, None, 11, 100))

    def test_symlink_matches_on_target(self):
        self.assertEqual(hb.update_fileset_find_match(True, '/l', self.files, self.symlinks, 'a', None, None), 7)
        self.assertIsNone(hb.update_fileset_find_match(True, '/l', self.files, self.symlinks, 'b', None, None))

    def test_unknown_path(self):
        self.assertIsNone(hb.update_fileset_find_match(False, '/x', self.files, self.symlinks, None, 10, 100))
        self.assertIsNone(hb.update_fileset_find_match(True, '/x', self.files, self.symlinks, 'a', None, None))


class TestGetAttributes(TempDirTestCase):
    def test_regular_file(self):
        path = write_file(self.path('f'), b'12345', mtime=1000000000)
        is_symlink, target, size, mtime, hardlink, unsupported = hb.update_fileset_get_attributes(path)
        self.assertEqual((is_symlink, target, size, mtime, hardlink, unsupported),
                         (False, None, 5, 1000000000, False, None))

    def test_hardlinked_file(self):
        path = write_file(self.path('f'), b'x')
        os.link(path, self.path('g'))
        self.assertTrue(hb.update_fileset_get_attributes(path)[4])

    def test_directory(self):
        os.mkdir(self.path('d'))
        os.utime(self.path('d'), (1000000000, 1000000000))
        is_symlink, target, size, mtime, hardlink, unsupported = hb.update_fileset_get_attributes(self.path('d'))
        self.assertEqual((is_symlink, size, mtime, unsupported), (False, 0, 1000000000, None))

    def test_symlink_is_not_followed(self):
        os.mkdir(self.path('d'))
        os.symlink('d', self.path('l'))
        is_symlink, target, size, mtime, hardlink, unsupported = hb.update_fileset_get_attributes(self.path('l'))
        self.assertEqual((is_symlink, target, size, mtime, unsupported), (True, 'd', None, None, None))

    def test_broken_symlink(self):
        os.symlink('missing', self.path('l'))
        self.assertEqual(hb.update_fileset_get_attributes(self.path('l'))[:2], (True, 'missing'))

    def test_fifo(self):
        os.mkfifo(self.path('p'))
        self.assertEqual(hb.update_fileset_get_attributes(self.path('p'))[5], 'FIFO')

    def test_socket(self):
        make_socket(self.path('s'))
        self.assertEqual(hb.update_fileset_get_attributes(self.path('s'))[5], 'socket')

    def test_device(self):
        self.assertTrue(stat.S_ISCHR(os.lstat('/dev/null').st_mode))
        self.assertEqual(hb.update_fileset_get_attributes('/dev/null')[5], 'device file')

    def test_missing_path_raises(self):
        with self.assertRaises(FileNotFoundError):
            hb.update_fileset_get_attributes(self.path('missing'))


class TestUsageText(TempDirTestCase):
    def test_lists_every_command(self):
        # The command table is only defined in the Main section, so read it from the script.
        with open(hb.__file__) as file:
            source = file.read()
        start = source.index("commands_json = '''") + len("commands_json = '''")
        config = json.loads(source[start:source.index("'''", start)])
        text = hb.generate_usage_text(config)
        for command in config['commands']:
            self.assertIn(command['name'], text)
            self.assertIn(command['desc'], text)
            # Every command is implemented by a function of the same name.
            self.assertTrue(callable(getattr(hb, command['name'].replace('-', '_'), None)), command['name'])
