# Shared helpers for the hd-backup tests.

import contextlib
import glob
import importlib.machinery
import importlib.util
import io
import os
import shutil
import sqlite3
import stat
import subprocess
import sys
import tarfile
import tempfile
import unittest

from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
HD_BACKUP = os.path.join(REPO, 'deploy', 'usr', 'local', 'bin', 'hd-backup')
BLOCKWRITE_SOURCE = os.path.join(REPO, 'src', 'blockwrite.c')

_module = None


def load_hd_backup():
    # Imports hd-backup as a module. It has no .py extension, so the loader is given explicitly. Importing it does
    # not run a command: that only happens when it runs as a program.
    global _module
    if _module is None:
        loader = importlib.machinery.SourceFileLoader('hd_backup', HD_BACKUP)
        spec = importlib.util.spec_from_loader('hd_backup', loader)
        _module = importlib.util.module_from_spec(spec)
        loader.exec_module(_module)
    return _module


# The database version the tool expects.
DB_VERSION = load_hd_backup().DB_VERSION


def quiet():
    # The functions print a lot. Keep the test output readable.
    return contextlib.redirect_stdout(io.StringIO())


def capture():
    # Like quiet(), but the output can be read afterwards with .getvalue().
    buffer = io.StringIO()
    return buffer, contextlib.redirect_stdout(buffer)


def write_file(path, content=b'', mtime=None):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if isinstance(content, str):
        content = content.encode()
    with open(path, 'wb') as file:
        file.write(content)
    if mtime is not None:
        os.utime(path, (mtime, mtime))
    return path


def make_socket(path):
    # A unix socket path is limited to 108 bytes, so bind with a relative name from inside its directory.
    import socket
    old_cwd = os.getcwd()
    os.chdir(os.path.dirname(path))
    try:
        sock = socket.socket(socket.AF_UNIX)
        sock.bind(os.path.basename(path))
        sock.close()
    finally:
        os.chdir(old_cwd)


def write_stub(directory, name, script):
    # A stand-in for a program, found first on PATH when directory is put in front of it.
    os.makedirs(directory, exist_ok=True)
    path = os.path.join(directory, name)
    with open(path, 'w') as file:
        file.write(script)
    os.chmod(path, 0o755)
    return path


def snapshot(root):
    # Describes a tree: the type of every item, the contents of files and the targets of symlinks.
    result = {}
    for directory, dirs, files in os.walk(root):
        for name in dirs + files:
            path = os.path.join(directory, name)
            relative = os.path.relpath(path, root)
            if os.path.islink(path):
                result[relative] = ('symlink', os.readlink(path))
            elif os.path.isdir(path):
                result[relative] = ('dir',)
            else:
                with open(path, 'rb') as file:
                    result[relative] = ('file', file.read())
    return result


def make_writable(root):
    # Undo chmod 000 from a test, so the tree can be removed.
    for directory, dirs, files in os.walk(root):
        for name in dirs + files:
            path = os.path.join(directory, name)
            if not os.path.islink(path):
                os.chmod(path, stat.S_IRWXU)
    if os.path.exists(root):
        os.chmod(root, stat.S_IRWXU)


def is_root():
    return os.geteuid() == 0


def user_namespaces_work():
    # Unprivileged user namespaces let a test mount a file system of its own, without root.
    if shutil.which('unshare') is None:
        return False
    return subprocess.run(['unshare', '--map-root-user', '--mount', 'true'], capture_output=True).returncode == 0


class TempDirTestCase(unittest.TestCase):
    # A test with its own temporary directory, removed afterwards. The temporary files of hd-backup go to the
    # subdirectory tmp, so a test can check which are left. Delete lists the tool leaves in /tmp during the test are
    # removed too.

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='hd-backup-test-')
        self.tempdir = self.path('tmp')
        os.makedirs(self.tempdir)
        tempdir = mock.patch.object(tempfile, 'tempdir', self.tempdir)
        tempdir.start()
        self.addCleanup(tempdir.stop)
        self.deletelists_before = set(glob.glob('/tmp/deletes_*.py'))

    def tearDown(self):
        make_writable(self.tmp)
        shutil.rmtree(self.tmp)
        for deletelist in set(glob.glob('/tmp/deletes_*.py')) - self.deletelists_before:
            os.remove(deletelist)

    def path(self, *parts):
        return os.path.join(self.tmp, *parts)

    def temp_files(self):
        # The temporary files hd-backup left behind. Delete lists are always in /tmp, so they are counted separately.
        return sorted(os.listdir(self.tempdir))

    def new_deletelists(self):
        # The delete lists in /tmp that are new since the test started.
        return sorted(set(glob.glob('/tmp/deletes_*.py')) - self.deletelists_before)


class DbTestCase(TempDirTestCase):
    # Calls the functions of hd-backup directly, on an in-memory database.

    def setUp(self):
        super().setUp()
        self.hb = load_hd_backup()
        self.hb.had_errors = False
        with quiet():
            self.db = self.hb.open_db(':memory:')

    def tearDown(self):
        self.db.close()
        super().tearDown()

    def value(self, query, params=()):
        return self.db.execute(query, params).fetchone()[0]

    def rows(self, query, params=()):
        # As tuples, so they compare equal to the tuples in the tests.
        return [tuple(row) for row in self.db.execute(query, params)]

    def add_fileset(self, name, *paths):
        with quiet():
            self.hb.add_fileset(self.db, name, list(paths))
        return self.value('SELECT number FROM filesets WHERE name=?', (name,))

    def update(self, *names):
        with quiet():
            self.hb.update_filesets(self.db, list(names))

    def add_backup(self, name, fileset_name, *labels):
        with quiet():
            self.hb.add_backup(self.db, name, fileset_name)
            self.hb.add_carrier(self.db, name, list(labels))
        return self.value('SELECT number FROM backups WHERE name=?', (name,))

    def record_written(self, backup_name, full=False):
        # Records every item of the fileset that is on disk and not on this backup yet as written, in a new tarball
        # on the first carrier that is not full. That is what a write does, without running tar.
        backup_number, fileset_number = self.rows('SELECT number, fileset_number FROM backups WHERE name=?',
                                                  (backup_name,))[0]
        carrier_number = self.rows('SELECT number FROM carrier WHERE backup_number=? AND full=0 ORDER BY label',
                                   (backup_number,))[0][0]
        index = self.value('SELECT COUNT(*) FROM tarballs WHERE carrier_number=?', (carrier_number,))
        written = {}
        for what in ('file', 'symlink'):
            written[what] = self.rows(f'''
                SELECT {what}s.number, {what}s.path
                FROM {what}s
                    JOIN {what}_fileset_links ON {what}_fileset_links.{what}_number={what}s.number
                WHERE {what}_fileset_links.fileset_number=? AND on_disk=1
                AND {what}s.number NOT IN (
                    SELECT {what}_number
                    FROM {what}_tarball_links
                        JOIN tarballs ON tarballs.number={what}_tarball_links.tarball_number
                        JOIN carrier ON carrier.number=tarballs.carrier_number
                    WHERE carrier.backup_number=?
                )
            ''', (fileset_number, backup_number))
        files = [self.hb.WriteItem(number, path, 0, 0) for number, path in written['file']]
        symlinks = [self.hb.WriteItem(number, path, 0, 0) for number, path in written['symlink']]
        with quiet():
            self.hb.write_update_db(self.db, carrier_number, index, 0, 0, symlinks, files, full)
        return written

    def file_records(self, path):
        # (number, size, on_disk) of every record of a path, oldest first.
        return self.rows('SELECT number, size, on_disk FROM files WHERE path=? ORDER BY number', (path,))

    def symlink_records(self, path):
        return self.rows('SELECT number, target, on_disk FROM symlinks WHERE path=? ORDER BY number', (path,))


class CliTestCase(TempDirTestCase):
    # Runs hd-backup as a program, with its own home directory, so it has its own database.

    def setUp(self):
        super().setUp()
        self.home = self.path('home')
        self.data = self.path('data')
        os.makedirs(self.home)
        os.makedirs(self.data)

    def run_cli(self, *args, expect=0, path_prefix=None, wrapper=()):
        # wrapper is a command that runs hd-backup, given as its last arguments.
        env = dict(os.environ, HOME=self.home, TMPDIR=self.tempdir)
        if path_prefix is not None:
            env['PATH'] = path_prefix + os.pathsep + env['PATH']
        result = subprocess.run(list(wrapper) + [sys.executable, HD_BACKUP] + list(args), env=env, capture_output=True,
                                text=True)
        if expect is not None:
            self.assertEqual(result.returncode, expect,
                             f'hd-backup {" ".join(args)} exited with {result.returncode}:\n'
                             f'{result.stdout}\n{result.stderr}')
        return result

    def db(self):
        return sqlite3.connect(os.path.join(self.home, '.local', 'share', 'hyprdebian', 'hd-backup.db'))

    def query(self, query, params=()):
        db = self.db()
        try:
            return db.execute(query, params).fetchall()
        finally:
            db.close()

    def restore(self, tarballs, restore_dir):
        # Extracts the tarballs in the order they were written, then runs the delete list of the last one.
        os.makedirs(restore_dir, exist_ok=True)
        for tarball in tarballs:
            subprocess.run(['tar', '-xf', tarball, '-C', restore_dir], check=True)
        with tarfile.open(tarballs[-1]) as archive:
            deletelists = [name for name in archive.getnames() if os.path.basename(name).startswith('deletes_')]
        self.assertEqual(len(deletelists), 1)
        result = subprocess.run([sys.executable, os.path.join(restore_dir, deletelists[0])],
                                capture_output=True, text=True, check=True)
        return result.stdout

    def restored(self, restore_dir, live_path):
        # Where a live path ends up in a restore directory.
        return os.path.join(restore_dir, live_path.lstrip('/'))
