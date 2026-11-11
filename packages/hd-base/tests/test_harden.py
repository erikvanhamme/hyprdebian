# Tests for hd-harden, on the parts that decide what it changes and what it shows: the users of
# sudo who can log in with a key, the state of a file it writes, the modules that are loaded already, the ZFS dataset of /home, the local IPv4 networks from
# networkctl, the rules of ufw that let ssh in from anywhere, and the networks of --network.

import argparse
import importlib.machinery
import importlib.util
import os
import pwd
import tempfile
import unittest

HD_HARDEN = os.path.join(os.path.dirname(__file__), '..', 'hd-harden')


def load_hd_harden():
    # hd-harden has no .py extension, so the loader is given explicitly.
    loader = importlib.machinery.SourceFileLoader('hd_harden', HD_HARDEN)
    spec = importlib.util.spec_from_loader('hd_harden', loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


harden = load_hd_harden()


def address(text, prefix, family=2, scope='global'):
    return {'Family': family, 'AddressString': text, 'PrefixLength': prefix, 'ScopeString': scope}


class TestAdminsWithKeys(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.dir.cleanup()

    def user(self, name, uid, keys=None, gid=None):
        home = os.path.join(self.dir.name, name)
        if keys is not None:
            os.makedirs(os.path.join(home, '.ssh'))
            with open(os.path.join(home, '.ssh', 'authorized_keys'), 'w', encoding='utf-8') as f:
                f.write(keys)
        return pwd.struct_passwd((name, 'x', uid, uid if gid is None else gid, '', home,
                                  '/bin/bash'))

    def test_admins_with_keys(self):
        users = [self.user('root', 0, 'ssh-ed25519 AAAA root\n'),
                 self.user('rsq', 1000, 'ssh-ed25519 AAAA rsq\n'),
                 self.user('other', 1001, 'ssh-ed25519 AAAA other\n'),
                 self.user('nokey', 1002)]
        self.assertEqual(harden.admins_with_keys(users, (['rsq', 'nokey'], 27)), ['rsq'])

    def test_admin_by_own_group(self):
        users = [self.user('admin', 1000, 'ssh-ed25519 AAAA admin\n', gid=27)]
        self.assertEqual(harden.admins_with_keys(users, ([], 27)), ['admin'])

    def test_root_alone(self):
        users = [self.user('root', 0, 'ssh-ed25519 AAAA root\n')]
        self.assertEqual(harden.admins_with_keys(users, (['root'], 27)), [])

    def test_empty_keys(self):
        users = [self.user('rsq', 1000, '')]
        self.assertEqual(harden.admins_with_keys(users, (['rsq'], 27)), [])

    def test_no_group(self):
        users = [self.user('rsq', 1000, 'ssh-ed25519 AAAA rsq\n')]
        self.assertEqual(harden.admins_with_keys(users, ([], None)), [])


class TestFileState(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.path = os.path.join(self.dir.name, 'hd-harden.conf')

    def tearDown(self):
        self.dir.cleanup()

    def write(self, content):
        with open(self.path, 'wb') as f:
            f.write(content)

    def test_missing(self):
        self.assertEqual(harden.file_state(self.path, b'a\n'), 'off')

    def test_same(self):
        self.write(b'a\n')
        self.assertEqual(harden.file_state(self.path, b'a\n'), 'on')

    def test_other(self):
        self.write(b'b\n')
        self.assertEqual(harden.file_state(self.path, b'a\n'), 'changed')


class TestLoadedModules(unittest.TestCase):
    def test_loaded(self):
        proc_modules = ("sctp 507904 2 - Live 0x0000000000000000\n"
                        "zfs 6602752 6 - Live 0x0000000000000000 (POE)\n"
                        "hfs 77824 0 - Live 0x0000000000000000\n")
        self.assertEqual(harden.loaded_modules(proc_modules), ['sctp', 'hfs'])

    def test_none_loaded(self):
        self.assertEqual(harden.loaded_modules("zfs 6602752 6 - Live 0x0\n"), [])


class TestHomeDataset(unittest.TestCase):
    def test_home(self):
        zfs_list = ("rpool\t/\n"
                    "rpool/home\t/home\n"
                    "rpool/home/rsq\t/home/rsq\n"
                    "rpool/hyprdebian\t/\n")
        self.assertEqual(harden.home_dataset(zfs_list), 'rpool/home')

    def test_no_home(self):
        self.assertIsNone(harden.home_dataset("rpool\t/\nrpool/hyprdebian\t/\n"))


class TestLocalNetworks(unittest.TestCase):
    def test_configured_link(self):
        links = [{'Name': 'enp7s0f3', 'AdministrativeState': 'configured',
                  'Addresses': [address('172.16.1.110', 16),
                                address('2a02:1811::1', 64, family=10),
                                address('fe80::1', 64, family=10, scope='link')]}]
        self.assertEqual(harden.local_networks(links), ['172.16.0.0/16'])

    def test_unmanaged_link(self):
        links = [{'Name': 'lo', 'AdministrativeState': 'unmanaged',
                  'Addresses': [address('127.0.0.1', 8, scope='host')]},
                 {'Name': 'docker0', 'AdministrativeState': 'unmanaged',
                  'Addresses': [address('172.17.0.1', 16)]}]
        self.assertEqual(harden.local_networks(links), [])

    def test_docker_bridges(self):
        links = [{'Name': 'br0', 'AdministrativeState': 'configured',
                  'Addresses': [address('172.16.1.110', 16)]},
                 {'Name': 'docker0', 'AdministrativeState': 'unmanaged',
                  'Addresses': [address('192.168.253.1', 24)]},
                 {'Name': 'br-3f2a9c1d7e4b', 'AdministrativeState': 'unmanaged',
                  'Addresses': [address('192.168.240.1', 24)]}]
        self.assertEqual(harden.local_networks(links), ['172.16.0.0/16'])

    def test_several_networks(self):
        links = [{'Name': 'br0', 'AdministrativeState': 'configured',
                  'Addresses': [address('192.168.1.5', 24), address('10.0.0.5', 8)]},
                 {'Name': 'wlan0', 'AdministrativeState': 'configured',
                  'Addresses': [address('192.168.1.6', 24)]}]
        self.assertEqual(harden.local_networks(links), ['10.0.0.0/8', '192.168.1.0/24'])

    def test_link_without_addresses(self):
        links = [{'Name': 'enp7s0f2', 'AdministrativeState': 'configured', 'Addresses': None}]
        self.assertEqual(harden.local_networks(links), [])


class TestOpenSshRules(unittest.TestCase):
    def test_rules_from_anywhere(self):
        added = ("Added user rules (see 'ufw status' for running firewall):\n"
                 "ufw allow SSH\n"
                 "ufw limit 22/tcp\n"
                 "ufw allow in OpenSSH\n"
                 "ufw allow 22\n")
        self.assertEqual(harden.open_ssh_rules(added),
                         [['allow', 'SSH'], ['limit', '22/tcp'], ['allow', 'in', 'OpenSSH'],
                          ['allow', '22']])

    def test_other_rules(self):
        added = ("Added user rules (see 'ufw status' for running firewall):\n"
                 "ufw allow from 172.16.0.0/16 to any port 22 proto tcp\n"
                 "ufw allow 80/tcp\n"
                 "ufw deny SSH\n")
        self.assertEqual(harden.open_ssh_rules(added), [])

    def test_docker_rules(self):
        # The route rules of the containers of hd-docker, and the rules for the networks of
        # Docker, are not rules that let ssh in from anywhere.
        added = ("Added user rules (see 'ufw status' for running firewall):\n"
                 "ufw route allow proto tcp to any port 22\n"
                 "ufw route allow 22/tcp\n"
                 "ufw allow in on docker0 to any port 22 proto tcp\n"
                 "ufw allow from 192.168.240.0/20 to any port 22 proto tcp\n")
        self.assertEqual(harden.open_ssh_rules(added), [])

    def test_duplicates(self):
        self.assertEqual(harden.open_ssh_rules("ufw allow SSH\nufw allow SSH\n"),
                         [['allow', 'SSH']])


class TestNetwork(unittest.TestCase):
    def test_network(self):
        self.assertEqual(harden.network('192.168.1.0/24'), '192.168.1.0/24')

    def test_host_bits(self):
        self.assertEqual(harden.network('172.16.1.110/16'), '172.16.0.0/16')

    def test_not_ipv4(self):
        for value in ('fd00::/8', 'lan', '300.1.1.0/24'):
            with self.assertRaises(argparse.ArgumentTypeError):
                harden.network(value)


if __name__ == '__main__':
    unittest.main()
