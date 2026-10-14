# Tests for write_determine_written: which items count as written when a carrier runs full.

import os
import random
import shutil
import subprocess
import tarfile
import unittest

from helpers import TempDirTestCase, load_hd_backup, quiet, write_file

hb = load_hd_backup()


def determine(bytes_written, full, symlinks, files, deletelist_name='/tmp/deletes.py', deletelist_size=700):
    with quiet():
        return hb.write_determine_written(bytes_written, full, symlinks, files, deletelist_name, deletelist_size)


class TestDetermineWritten(TempDirTestCase):
    symlinks = [(1, '/a/l1', 't'), (2, '/a/l2', 't')]
    files = [(3, '/a/f1', 1000), (4, '/a/f2', 2000)]

    def test_not_full_records_everything(self):
        symlinks, files, overhead, total_size = determine(4194304, False, self.symlinks, self.files)
        self.assertEqual(symlinks, self.symlinks)
        self.assertEqual(files, self.files)
        self.assertEqual(total_size, 3000)
        self.assertEqual(overhead, 4194304 - 3000)

    def test_not_full_ignores_byte_count(self):
        # Without a full carrier, everything is on it, whatever the byte count says.
        symlinks, files, overhead, total_size = determine(0, False, self.symlinks, self.files)
        self.assertEqual((len(symlinks), len(files)), (2, 2))

    def test_full_with_nothing_written(self):
        symlinks, files, overhead, total_size = determine(0, True, self.symlinks, self.files)
        self.assertEqual((symlinks, files, total_size, overhead), ([], [], 0, 0))

    def test_full_after_all_items(self):
        symlinks, files, overhead, total_size = determine(10 ** 6, True, self.symlinks, self.files)
        self.assertEqual((symlinks, files), (self.symlinks, self.files))

    def test_full_stops_at_first_item_that_does_not_fit(self):
        end_of_l1 = hb.tar_item_size('/tmp/deletes.py', 700) + hb.tar_item_size('/a/l1', 0, 't')
        symlinks, files, overhead, total_size = determine(end_of_l1, True, self.symlinks, self.files)
        self.assertEqual((symlinks, files), ([self.symlinks[0]], []))

    def test_item_ending_exactly_at_byte_count_is_written(self):
        end_of_f1 = (hb.tar_item_size('/tmp/deletes.py', 700) + hb.tar_item_size('/a/l1', 0, 't')
                     + hb.tar_item_size('/a/l2', 0, 't') + hb.tar_item_size('/a/f1', 1000))
        symlinks, files, overhead, total_size = determine(end_of_f1, True, self.symlinks, self.files)
        self.assertEqual(files, [self.files[0]])
        symlinks, files, overhead, total_size = determine(end_of_f1 - 1, True, self.symlinks, self.files)
        self.assertEqual(files, [])

    def test_files_only_after_all_symlinks(self):
        # tar writes the symlinks first. When one does not fit, no file is on the carrier.
        big_symlink = [(1, '/a/l1', 'x' * 5000)]
        files = [(3, '/a/f', 0)]
        enough_for_file_only = hb.tar_item_size('/tmp/deletes.py', 700) + hb.tar_item_size('/a/f', 0)
        symlinks, files_written, overhead, total_size = determine(enough_for_file_only, True, big_symlink, files)
        self.assertEqual((symlinks, files_written), ([], []))

    def test_overhead_is_not_negative(self):
        for bytes_written in range(0, 20000, 512):
            symlinks, files, overhead, total_size = determine(bytes_written, True, self.symlinks, self.files)
            self.assertGreaterEqual(overhead, 0)


@unittest.skipIf(shutil.which('tar') is None, 'tar is not installed')
class TestDetermineWrittenAgainstTar(TempDirTestCase):
    # Builds a tarball with the same tar options as hd-backup, cuts it at random places, and checks that every item
    # counted as written is complete before the cut.

    def build_tree(self, rng):
        root = self.path('tree')
        symlinks = []
        files = []
        for i in range(150):
            depth = rng.choice([1, 1, 2, 8])
            directory = os.path.join(root, *[('d' * rng.choice([1, 30, 120])) + str(i) for _ in range(depth)])
            os.makedirs(directory, exist_ok=True)
            path = os.path.join(directory, 'f' * rng.choice([1, 90, 200]) + str(i) + 'é' * rng.choice([0, 5]))
            if rng.random() < 0.2:
                target = 't' * rng.choice([1, 150, 600])
                os.symlink(target, path)
                symlinks.append((i, path, target))
            else:
                size = rng.choice([0, 1, 511, 512, 513, 4096, 100000])
                write_file(path, os.urandom(size))
                files.append((i, path, size))
        symlinks.sort(key=lambda item: item[1])
        files.sort(key=lambda item: item[1])
        return symlinks, files

    def test_never_counts_an_incomplete_item(self):
        rng = random.Random(1)
        symlinks, files = self.build_tree(rng)
        deletelist = write_file(self.path('deletes.py'), 'x' * 700)

        names = [deletelist] + [item[1] for item in symlinks] + [item[1] for item in files]
        with open(self.path('list'), 'wb') as filelist:
            filelist.write(b''.join(os.fsencode(name.lstrip('/')) + b'\0' for name in names))
        tarball = self.path('all.tar')
        subprocess.run(['tar', '--no-recursion', '--hard-dereference', '-H', 'pax', '-b', '8192', '--null',
                        '--verbatim-files-from', '-cf', tarball, '-C', '/', '-T', self.path('list')], check=True)

        ends = {}
        with tarfile.open(tarball) as archive:
            for member in archive:
                ends['/' + member.name] = member.offset_data + (member.size if member.isreg() else 0)

        size = os.path.getsize(tarball)
        for cut in sorted(rng.sample(range(size), 300)) + [size]:
            symlinks_written, files_written, overhead, total_size = determine(cut, True, symlinks, files,
                                                                              deletelist, 700)
            for item in symlinks_written + files_written:
                self.assertLessEqual(ends[item[1]], cut, f'{item[1]} counted as written at {cut} bytes')

            if cut == size:
                # The whole tarball fits: everything counts as written.
                self.assertEqual((len(symlinks_written), len(files_written)), (len(symlinks), len(files)))
