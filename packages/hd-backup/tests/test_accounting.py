# Tests for the estimate of the bytes tar writes for an item, and for write_determine_written: which items count as
# written when a carrier runs full.

import os
import random
import shutil
import subprocess
import tarfile
import unittest

from helpers import TempDirTestCase, load_hd_backup, quiet, write_file

hb = load_hd_backup()

# Times with all 9 decimals, so tar writes the time records at the length the estimate counts.
FULL_TIME_NS = 1700000000_123456789


def determine(bytes_written, full, symlinks, files, deletelist_tar_size=2048):
    with quiet():
        return hb.write_determine_written(bytes_written, full, symlinks, files, deletelist_tar_size)


class TestDetermineWritten(TempDirTestCase):
    symlinks = [hb.WriteItem(1, '/a/l1', 0, 1536), hb.WriteItem(2, '/a/l2', 0, 1536)]
    files = [hb.WriteItem(3, '/a/f1', 1000, 2560), hb.WriteItem(4, '/a/f2', 2000, 3072)]

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
        symlinks, files, overhead, total_size = determine(2048 + 1536, True, self.symlinks, self.files)
        self.assertEqual((symlinks, files), ([self.symlinks[0]], []))

    def test_item_ending_exactly_at_byte_count_is_written(self):
        end_of_f1 = 2048 + 1536 + 1536 + 2560
        symlinks, files, overhead, total_size = determine(end_of_f1, True, self.symlinks, self.files)
        self.assertEqual(files, [self.files[0]])
        symlinks, files, overhead, total_size = determine(end_of_f1 - 1, True, self.symlinks, self.files)
        self.assertEqual(files, [])

    def test_files_only_after_all_symlinks(self):
        # tar writes the symlinks first. When one does not fit, no file is on the carrier.
        big_symlink = [hb.WriteItem(1, '/a/l1', 0, 6144)]
        files = [hb.WriteItem(3, '/a/f', 0, 1536)]
        symlinks, files_written, overhead, total_size = determine(2048 + 1536, True, big_symlink, files)
        self.assertEqual((symlinks, files_written), ([], []))

    def test_overhead_is_not_negative(self):
        for bytes_written in range(0, 20000, 512):
            symlinks, files, overhead, total_size = determine(bytes_written, True, self.symlinks, self.files)
            self.assertGreaterEqual(overhead, 0)


class TarTestCase(TempDirTestCase):
    # Writes items to a tarball with the tar options of hd-backup, and compares what tar wrote with the estimate.

    def make(self, path, kind='file', size=1, target='t'):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        if kind == 'file':
            write_file(path, b'x' * size)
        elif kind == 'dir':
            os.makedirs(path, exist_ok=True)
        else:
            os.symlink(target, path)
        os.utime(path, ns=(FULL_TIME_NS, FULL_TIME_NS), follow_symlinks=False)
        return path

    def long_path(self, length, tag):
        # An absolute path of exactly length bytes, in parts of at most 200 bytes.
        base = self.path(tag)
        parts = []
        remaining = length - len(base)
        while remaining > 0:
            part = min(200, remaining - 1)
            if remaining - 1 - part == 1:
                part -= 1
            parts.append('x' * part)
            remaining -= part + 1
        path = os.path.join(base, *parts)
        self.assertEqual(len(path), length)
        return path

    def tar(self, paths):
        # The real bytes of every item, and its pax records.
        with open(self.path('list'), 'wb') as filelist:
            filelist.write(b''.join(os.fsencode(path.lstrip('/')) + b'\0' for path in paths))
        tarball = self.path('items.tar')
        subprocess.run(['tar', '--no-recursion', '--hard-dereference', '-H', 'pax', '-b', '8192', '--null',
                        '--verbatim-files-from', '-cf', tarball, '-C', '/', '-T', self.path('list')], check=True)
        result = []
        start = 0
        with tarfile.open(tarball) as archive:
            for member in archive:
                end = member.offset_data + hb.tar_blocks(member.size if member.isreg() else 0)
                result.append((end - start, member.pax_headers))
                start = end
        return result

    def estimate(self, path):
        status = os.lstat(path)
        if os.path.islink(path):
            return hb.tar_item_size(path, status, 0, os.readlink(path))
        elif os.path.isdir(path):
            return hb.tar_item_size(path, status, 0)
        else:
            return hb.tar_item_size(path, status, status.st_size)

    @staticmethod
    def times_at_full_length(pax_headers):
        # tar writes a time without trailing zeros, and mtime not at all when it is a whole second. The estimate
        # counts all 9 decimals. When tar did too, the estimate must be exact.
        for key in ('mtime', 'atime', 'ctime'):
            value = pax_headers.get(key, '')
            if len(value.partition('.')[2]) != 9:
                return False
        return True

    def assert_estimates(self, paths):
        for path, (real, pax_headers) in zip(paths, self.tar(paths)):
            estimate = self.estimate(path)
            message = f'{len(path)} bytes, {sorted(pax_headers)}: estimate {estimate}, real {real}'
            if self.times_at_full_length(pax_headers):
                self.assertEqual(estimate, real, message)
            else:
                # Shorter time records can only make the real size smaller.
                self.assertGreaterEqual(estimate, real, message)
                self.assertLessEqual(estimate - real, 512, message)


@unittest.skipIf(shutil.which('tar') is None, 'tar is not installed')
class TestEstimateAgainstTar(TarTestCase):
    LENGTHS = [40, 99, 100, 101, 102, 150, 255, 256, 257, 300, 350, 400, 405, 410, 415, 420, 500, 768, 800, 900, 920,
               930, 1000, 1400, 2000, 3000, 4000]

    def test_files_with_long_paths(self):
        self.assert_estimates([self.make(self.long_path(length, f'f{length}')) for length in self.LENGTHS])

    def test_directories_with_long_paths(self):
        # The name of a directory in the tarball ends with a /, which can push it over 100 bytes.
        self.assert_estimates([self.make(self.long_path(length, f'd{length}'), 'dir') for length in self.LENGTHS])

    def test_symlinks_with_long_targets(self):
        paths = []
        for length in [1, 99, 100, 101, 200, 300, 400, 410, 420, 500, 1000, 2000, 4000]:
            paths.append(self.make(self.path(f'links/l{length}'), 'symlink', target='t' * length))
        paths.append(self.make(self.long_path(1500, 'long'), 'symlink', target='t' * 1500))
        self.assert_estimates(paths)

    def test_names_that_are_not_ascii(self):
        # Short names that are not ASCII get a path record too.
        paths = [self.make(self.path('u', 'é' * count)) for count in (1, 10, 60, 120)]
        paths.append(self.make(self.path('links', 'é'), 'symlink', target='ü' * 10))
        self.assert_estimates(paths)

    def test_file_sizes_around_blocks(self):
        paths = [self.make(self.path('sizes', str(size)), size=size) for size in (0, 1, 511, 512, 513, 100000)]
        self.assert_estimates(paths)

    def test_whole_second_times_are_never_underestimated(self):
        # tar leaves out mtime when it is a whole second. The estimate counts it anyway.
        path = self.make(self.long_path(405, 'whole'))
        os.utime(path, ns=(1700000000_000000000, 1700000000_000000000))
        (real, pax_headers), = self.tar([path])
        self.assertNotIn('mtime', pax_headers)
        self.assertGreaterEqual(self.estimate(path), real)


@unittest.skipIf(shutil.which('tar') is None, 'tar is not installed')
class TestDetermineWrittenAgainstTar(TarTestCase):
    # Builds a tarball with the same tar options as hd-backup, cuts it at random places, and checks that exactly the
    # items that are complete before the cut count as written.

    def build_tree(self, rng):
        root = self.path('tree')
        symlinks = []
        files = []
        for i in range(150):
            depth = rng.choice([1, 1, 2, 8])
            directory = os.path.join(root, *[('d' * rng.choice([1, 30, 120])) + str(i) for _ in range(depth)])
            path = os.path.join(directory, 'f' * rng.choice([1, 90, 200]) + str(i) + 'é' * rng.choice([0, 5]))
            if rng.random() < 0.2:
                self.make(path, 'symlink', target='t' * rng.choice([1, 150, 600]))
                symlinks.append(hb.WriteItem(i, path, 0, self.estimate(path)))
            else:
                size = rng.choice([0, 1, 511, 512, 513, 4096, 100000])
                self.make(path, size=size)
                files.append(hb.WriteItem(i, path, size, self.estimate(path)))
        symlinks.sort(key=lambda item: item.path)
        files.sort(key=lambda item: item.path)
        return symlinks, files

    def test_counts_exactly_the_complete_items(self):
        rng = random.Random(1)
        symlinks, files = self.build_tree(rng)
        deletelist = self.make(self.path('deletes.py'), size=700)

        items = symlinks + files
        tarred = self.tar([deletelist] + [item.path for item in items])
        sizes = [real for real, pax_headers in tarred]

        # The end of every item in the tarball, and whether its estimate is exact.
        ends = {}
        position = sizes[0]
        for item, size in zip(items, sizes[1:]):
            position += size
            ends[item.path] = position
        exact = all(estimate == real for estimate, real in
                    zip([self.estimate(deletelist)] + [item.tar_size for item in items], sizes))

        total = sum(sizes)
        for cut in sorted(rng.sample(range(total), 300)) + [total]:
            symlinks_written, files_written, overhead, total_size = determine(cut, True, symlinks, files,
                                                                              self.estimate(deletelist))
            counted = {item.path for item in symlinks_written + files_written}
            complete = {path for path, end in ends.items() if end <= cut}

            # Never an item that is cut off. With exact estimates, also every item that is complete, as long as the
            # symlinks before it are all complete too.
            self.assertLessEqual(counted, complete, f'cut at {cut}')
            if exact:
                if all(ends[item.path] <= cut for item in symlinks):
                    self.assertEqual(counted, complete, f'cut at {cut}')
                else:
                    self.assertEqual(counted, {item.path for item in symlinks if ends[item.path] <= cut})
