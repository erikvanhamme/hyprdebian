"""The jobs of the estate: their format, and their signatures by the manager keys.

The package hd-base installs this as /usr/lib/python3/dist-packages/hyprdebian/job.py, since
1.91. hd-agent, hd-dashboard and hd-manager import it:

    import sys
    sys.dont_write_bytecode = True
    from hyprdebian import job

A job is one command for one host, to run once, not before not_before and not after expires
(Unix timestamps), with a sequence number that only goes up across the estate:

    {
        "job": {"sequence": 412, "host": "beast", "command": "upgrade",
                "not_before": 1795000000, "expires": 1795086400},
        "signature": "-----BEGIN SSH SIGNATURE-----\\n...\\n-----END SSH SIGNATURE-----\\n"
    }

The signature is made by ssh-keygen -Y sign in the namespace hd-job over the canonical JSON of
the job, and checked by ssh-keygen -Y verify against a list of public keys, the manager_keys.
"""

import json
import os
import re
import subprocess
import tempfile

SSH_KEYGEN = "/usr/bin/ssh-keygen"
# The namespace of the signatures, so a signature made for something else is no job.
NAMESPACE = "hd-job"
# The principal every manager key gets in the allowed-signers file. A job is valid when any
# key on the list signed it, so they all have the same one.
PRINCIPAL = "hd-manager"
# The commands an agent knows by name; nothing else can be run.
COMMANDS = ["upgrade"]
FIELDS = ["sequence", "host", "command", "not_before", "expires"]
HOST_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9-]{0,62}$")
# An Ed25519 public key as in a .pub file, also one on a security key: its type, the key in
# base64 and an optional comment, on one line, so it cannot add options or lines to the
# allowed-signers file.
PUBLIC_KEY = re.compile(
    r"^(ssh-ed25519|sk-ssh-ed25519@openssh\.com) [A-Za-z0-9+/]+={0,2}( [^\n\r]*)?$")
SIGNATURE = re.compile(
    r"^-----BEGIN SSH SIGNATURE-----\n[A-Za-z0-9+/=\n]+-----END SSH SIGNATURE-----\n?$")


class JobError(Exception):
    """A job, a signed job or a manager key that is malformed, or a signing that failed."""


class SignatureError(JobError):
    """A signed job whose signature does not verify against any of the manager keys."""


def is_integer(value):
    # A bool is an int in Python, but true and false are no numbers in JSON.
    return isinstance(value, int) and not isinstance(value, bool)


def check(job):
    """Checks the fields of a job. Raises JobError when one is missing, unknown or wrong."""
    if not isinstance(job, dict):
        raise JobError("the job is not a JSON object")
    missing = [field for field in FIELDS if field not in job]
    if missing:
        raise JobError(f"the job has no {', '.join(missing)}")
    unknown = sorted(field for field in job if field not in FIELDS)
    if unknown:
        raise JobError(f"the job has the unknown fields {', '.join(unknown)}")
    if not is_integer(job["sequence"]) or job["sequence"] < 1:
        raise JobError("the sequence number is not a positive integer")
    if not isinstance(job["host"], str) or not HOST_NAME.match(job["host"]):
        raise JobError("the host is not a host name")
    if job["command"] not in COMMANDS:
        raise JobError(f"the command is not one of {', '.join(COMMANDS)}")
    if not is_integer(job["not_before"]) or not is_integer(job["expires"]):
        raise JobError("not_before and expires are not integers")
    if job["expires"] <= job["not_before"]:
        raise JobError("the job expires before it may run")


def canonical(job):
    """The canonical JSON of a job, which is signed: sorted keys, no spaces."""
    return json.dumps(job, sort_keys=True, separators=(",", ":")).encode()


def sign(job, key):
    """Signs a job with the private key in the file key, which asks for its passphrase on the
    terminal. Returns the signed job. Raises JobError when the job is malformed or the signing
    fails."""
    return sign_all([job], key)[0]


def sign_all(jobs, key):
    """Signs a list of jobs with the private key in the file key, in one run of ssh-keygen, so
    its passphrase is asked for once. Returns the signed jobs, in the same order. Raises
    JobError when a job is malformed or the signing fails."""
    for job in jobs:
        check(job)
    with tempfile.TemporaryDirectory(prefix="hd-job-") as directory:
        # Files to sign, not stdin, so the passphrase is still read from the terminal.
        files = []
        for number, job in enumerate(jobs):
            data = os.path.join(directory, f"job-{number}.json")
            with open(data, "wb") as f:
                f.write(canonical(job))
            files.append(data)
        result = subprocess.run([SSH_KEYGEN, "-Y", "sign", "-n", NAMESPACE, "-f", key] + files,
                                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        if result.returncode != 0:
            raise JobError(f"signing with {key} failed: {result.stderr.strip()}")
        signed = []
        for job, data in zip(jobs, files):
            with open(data + ".sig") as f:
                signed.append({"job": job, "signature": f.read()})
    return signed


def allowed_signers(manager_keys):
    """The allowed-signers file of ssh-keygen for the manager keys: each with the principal
    PRINCIPAL, for the namespace NAMESPACE only. Raises JobError when a key is malformed."""
    if not isinstance(manager_keys, list):
        raise JobError("manager_keys is not a list")
    lines = []
    for key in manager_keys:
        if not isinstance(key, str) or not PUBLIC_KEY.match(key):
            raise JobError(f"manager_keys has {json.dumps(key)}, which is not a public key")
        lines.append(f'{PRINCIPAL} namespaces="{NAMESPACE}" {key}\n')
    return "".join(lines)


def verify(signed, manager_keys):
    """Checks a signed job, {"job": ..., "signature": ...}, against the list of public keys
    manager_keys. Returns the job. Raises SignatureError when its signature does not verify
    against any of the keys, and JobError when the signed job or a key is malformed."""
    if not isinstance(signed, dict) or sorted(signed) != ["job", "signature"]:
        raise JobError("the signed job is not a JSON object with job and signature")
    job = signed["job"]
    check(job)
    signature = signed["signature"]
    if not isinstance(signature, str) or not SIGNATURE.match(signature):
        raise SignatureError("the signature is malformed")
    signers = allowed_signers(manager_keys)
    if not manager_keys:
        raise SignatureError("there are no manager keys")
    with tempfile.TemporaryDirectory(prefix="hd-job-") as directory:
        signers_file = os.path.join(directory, "allowed_signers")
        with open(signers_file, "w") as f:
            f.write(signers)
        signature_file = os.path.join(directory, "job.json.sig")
        with open(signature_file, "w") as f:
            f.write(signature)
        result = subprocess.run([SSH_KEYGEN, "-Y", "verify", "-f", signers_file, "-I", PRINCIPAL,
                                 "-n", NAMESPACE, "-s", signature_file],
                                input=canonical(job), stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE)
    if result.returncode != 0:
        raise SignatureError("the signature does not verify against any of the manager keys")
    return job
