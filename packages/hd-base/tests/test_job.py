# Tests for hyprdebian.job: the fields of a job, its canonical JSON, and the signatures made with
# test keys in a temporary directory, which verify only against a list with the key that made them,
# for the job as it was signed, in the namespace hd-job.

import copy
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
from hyprdebian import job  # noqa: E402

JOB = {"sequence": 412, "host": "beast", "command": "upgrade",
       "not_before": 1795000000, "expires": 1795086400}


def make_key(directory, name):
    """Makes an Ed25519 key without a passphrase. Returns the path of the private key and the
    public key."""
    path = os.path.join(directory, name)
    subprocess.run([job.SSH_KEYGEN, "-q", "-t", "ed25519", "-N", "", "-C", name, "-f", path],
                   check=True)
    with open(path + ".pub") as f:
        return path, f.read().strip()


class TestCheck(unittest.TestCase):

    def changed(self, **fields):
        changed = dict(JOB)
        changed.update(fields)
        return changed

    def test_valid(self):
        job.check(JOB)

    def test_missing_field(self):
        incomplete = dict(JOB)
        del incomplete["expires"]
        with self.assertRaises(job.JobError):
            job.check(incomplete)

    def test_unknown_field(self):
        with self.assertRaises(job.JobError):
            job.check(self.changed(arguments="--reboot"))

    def test_not_an_object(self):
        with self.assertRaises(job.JobError):
            job.check([JOB])

    def test_sequence(self):
        for sequence in [0, -1, "412", 412.0, True]:
            with self.subTest(sequence=sequence), self.assertRaises(job.JobError):
                job.check(self.changed(sequence=sequence))

    def test_host(self):
        for host in ["", "-beast", "beast.lan", "be ast", "a" * 64, 7]:
            with self.subTest(host=host), self.assertRaises(job.JobError):
                job.check(self.changed(host=host))

    def test_command(self):
        for command in ["reboot", "upgrade --reboot", "", None]:
            with self.subTest(command=command), self.assertRaises(job.JobError):
                job.check(self.changed(command=command))

    def test_times(self):
        for times in [{"not_before": "now"}, {"expires": 1795086400.5}, {"expires": False},
                      {"expires": JOB["not_before"]}, {"expires": JOB["not_before"] - 1}]:
            with self.subTest(times=times), self.assertRaises(job.JobError):
                job.check(self.changed(**times))


class TestCanonical(unittest.TestCase):

    def test_sorted_without_spaces(self):
        self.assertEqual(job.canonical(JOB),
                         b'{"command":"upgrade","expires":1795086400,"host":"beast",'
                         b'"not_before":1795000000,"sequence":412}')

    def test_order_does_not_matter(self):
        self.assertEqual(job.canonical(dict(reversed(list(JOB.items())))), job.canonical(JOB))


class TestSignatures(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.key, cls.public_key = make_key(cls.directory.name, "manager")
        cls.other_key, cls.other_public_key = make_key(cls.directory.name, "other")
        cls.signed = job.sign(dict(JOB), cls.key)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def test_jobs_signed_in_one_go(self):
        jobs = [dict(JOB, sequence=sequence, host=host) for sequence, host in [(413, "beast"), (414, "box")]]
        signed = job.sign_all(jobs, self.key)

        self.assertEqual([value["job"] for value in signed], jobs)
        self.assertEqual([job.verify(value, [self.public_key]) for value in signed], jobs)
        with self.assertRaises(job.JobError):
            job.sign_all([JOB, dict(JOB, command="reboot")], self.key)

    def test_signed_job(self):
        self.assertEqual(self.signed["job"], JOB)
        self.assertTrue(self.signed["signature"].startswith("-----BEGIN SSH SIGNATURE-----\n"))

    def test_verifies_with_its_key(self):
        self.assertEqual(job.verify(self.signed, [self.public_key]), JOB)

    def test_verifies_with_its_key_among_others(self):
        self.assertEqual(job.verify(self.signed, [self.other_public_key, self.public_key]), JOB)

    def test_key_without_comment(self):
        key = " ".join(self.public_key.split()[:2])
        self.assertEqual(job.verify(self.signed, [key]), JOB)

    def test_refused_without_its_key(self):
        with self.assertRaises(job.SignatureError):
            job.verify(self.signed, [self.other_public_key])

    def test_refused_without_keys(self):
        with self.assertRaises(job.SignatureError):
            job.verify(self.signed, [])

    def test_refused_with_changed_field(self):
        for field, value in [("sequence", 413), ("host", "other"),
                             ("not_before", 1794999999), ("expires", 1795086401)]:
            changed = copy.deepcopy(self.signed)
            changed["job"][field] = value
            with self.subTest(field=field), self.assertRaises(job.SignatureError):
                job.verify(changed, [self.public_key])

    def test_refused_in_other_namespace(self):
        data = os.path.join(self.directory.name, "other-namespace.json")
        with open(data, "wb") as f:
            f.write(job.canonical(JOB))
        subprocess.run([job.SSH_KEYGEN, "-q", "-Y", "sign", "-n", "file", "-f", self.key, data],
                       check=True, stderr=subprocess.DEVNULL)
        with open(data + ".sig") as f:
            signed = {"job": dict(JOB), "signature": f.read()}
        with self.assertRaises(job.SignatureError):
            job.verify(signed, [self.public_key])

    def test_refused_with_malformed_signature(self):
        # The last line of base64 holds the signature itself: one character changed breaks it.
        lines = self.signed["signature"].splitlines(keepends=True)
        last = lines[-2]
        lines[-2] = last[:4] + ("A" if last[4] != "A" else "B") + last[5:]
        broken = "".join(lines)
        for signature in ["", "not a signature", broken,
                          self.signed["signature"].replace("SSH SIGNATURE", "PGP SIGNATURE"), None]:
            signed = {"job": dict(JOB), "signature": signature}
            with self.subTest(signature=signature), self.assertRaises(job.SignatureError):
                job.verify(signed, [self.public_key])

    def test_malformed_signed_job(self):
        for signed in [None, {"job": JOB}, {"signature": self.signed["signature"]},
                       dict(self.signed, comment="")]:
            with self.subTest(signed=signed), self.assertRaises(job.JobError):
                job.verify(signed, [self.public_key])

    def test_malformed_job_is_not_a_signature_error(self):
        changed = copy.deepcopy(self.signed)
        changed["job"]["command"] = "reboot"
        with self.assertRaises(job.JobError) as raised:
            job.verify(changed, [self.public_key])
        self.assertNotIsInstance(raised.exception, job.SignatureError)

    def test_malformed_manager_keys(self):
        for keys in [self.public_key, [self.public_key + "\n" + self.other_public_key],
                     ['cert-authority ' + self.public_key], [""], [None]]:
            with self.subTest(keys=keys), self.assertRaises(job.JobError):
                job.verify(self.signed, keys)

    def test_sign_refuses_malformed_job(self):
        with self.assertRaises(job.JobError):
            job.sign(dict(JOB, command="reboot"), self.key)

    def test_sign_fails_without_key(self):
        with self.assertRaises(job.JobError):
            job.sign(dict(JOB), os.path.join(self.directory.name, "missing"))


if __name__ == '__main__':
    unittest.main()
