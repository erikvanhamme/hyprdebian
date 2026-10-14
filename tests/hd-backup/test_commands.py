# Runs every command of hd-backup as a program, and checks the exit status.

import os

from helpers import CliTestCase, DB_VERSION, write_file


class TestCommands(CliTestCase):
    def setUp(self):
        super().setUp()
        write_file(os.path.join(self.data, 'f'), 'x')
        self.target = self.path('target')
        os.makedirs(self.target)

    def prepare(self):
        self.run_cli('add-fileset', 'fs', self.data)
        self.run_cli('add-backup', 'bk', 'fs')
        self.run_cli('add-carrier', 'bk', 'c1', 'c2')

    def test_creates_database_on_first_run(self):
        result = self.run_cli('list-filesets')
        self.assertIn('New database created.', result.stdout)
        self.assertEqual(self.query('SELECT version FROM version'), [(DB_VERSION,)])
        self.assertNotIn('New database created.', self.run_cli('list-filesets').stdout)

    def test_banner_shows_versions(self):
        self.assertRegex(self.run_cli('list-filesets').stdout, rf'Backuptool \[tool V\d+, database V{DB_VERSION}\]')

    def test_refuses_other_database_version(self):
        self.run_cli('list-filesets')
        db = self.db()
        db.execute('UPDATE version SET version=?', (DB_VERSION - 1,))
        db.commit()
        db.close()
        result = self.run_cli('list-filesets', expect=1)
        self.assertIn('Incompatible database version', result.stdout)

    def test_every_command_succeeds(self):
        self.prepare()
        write_file(os.path.join(self.data, 'g'), 'y')
        self.run_cli('add-paths', 'fs', self.data)
        self.run_cli('update-filesets', 'fs')
        self.run_cli('write', 'bk', self.target)
        for command in ('list-filesets', 'list-backups', 'list-carrier', 'list-tarballs'):
            self.assertIn('|', self.run_cli(command).stdout, command)
        self.assertIn('Begin carrier c1 manifest', self.run_cli('manifest', 'c1').stdout)
        self.assertIn('Backup set bk:', self.run_cli('summary').stdout)
        self.assertIn('Backup set bk:', self.run_cli('summary', 'b*').stdout)
        self.run_cli('rename-backup', 'bk', 'renamed')
        self.run_cli('remove-carrier', 'c2')
        self.run_cli('remove-backups', 'renamed')
        self.assertEqual(self.query('SELECT COUNT(*) FROM backups'), [(0,)])

    def test_unknown_names_fail(self):
        self.prepare()
        failing = [
            ('add-fileset', 'fs', self.data),
            ('add-fileset', 'new', self.path('missing')),
            ('add-paths', 'nope', self.data),
            ('add-paths', 'fs', self.path('missing')),
            ('update-filesets', 'nope'),
            ('update-filesets', 'fs', 'nope'),
            ('add-backup', 'bk', 'fs'),
            ('add-backup', 'new', 'nope'),
            ('add-carrier', 'nope', 'x1'),
            ('add-carrier', 'bk', 'c1'),
            ('rename-backup', 'nope', 'new'),
            ('rename-backup', 'bk', 'bk'),
            ('manifest', 'nope'),
            ('remove-carrier', 'nope'),
            ('remove-backups', 'nope'),
            ('write', 'nope', self.target),
            ('write', 'bk', self.path('missing')),
        ]
        for args in failing:
            self.run_cli(*args, expect=1)

    def test_missing_arguments_are_a_usage_error(self):
        self.assertEqual(self.run_cli('add-backup', 'only-one', expect=None).returncode, 2)
        self.assertEqual(self.run_cli('no-such-command', expect=None).returncode, 2)

    def test_write_fails_when_all_carriers_are_full(self):
        self.prepare()
        db = self.db()
        db.execute('UPDATE carrier SET full=1')
        db.commit()
        db.close()
        result = self.run_cli('write', 'bk', self.target, expect=1)
        self.assertIn('There are no carrier available', result.stdout)

    def test_write_fails_when_fileset_is_out_of_date(self):
        self.prepare()
        os.remove(os.path.join(self.data, 'f'))
        result = self.run_cli('write', 'bk', self.target, expect=1)
        self.assertIn('Update the fileset first', result.stdout)

    def test_scan_warning_fails_the_command(self):
        os.mkdir(os.path.join(self.data, 'closed'))
        os.chmod(os.path.join(self.data, 'closed'), 0)
        if os.access(os.path.join(self.data, 'closed'), os.R_OK):
            self.skipTest('the directory is readable anyway, for example as root')
        result = self.run_cli('add-fileset', 'fs', self.data, expect=1)
        self.assertIn('Could not read directory', result.stdout)
        # The fileset was still added and scanned: the file, and the directory entry itself, without its contents.
        self.assertEqual(self.query('SELECT COUNT(*) FROM files'), [(2,)])
