## Introduction
The package `hd-repo` turns a Debian or hyprdebian system into the hyprdebian package repository. It holds packages that are not in Debian, for the suites `trixie` and `unstable`, signs them, and serves them over HTTP with nginx. It only hosts packages; they are built elsewhere.

Installing the package sets everything up:
- the user `repo`, which uploads packages and maintains the repository;
- an inbox per suite in `/var/lib/hd-repo/inbox`;
- the repository in `/srv/hd-repo`;
- the signing key in `/etc/hyprdebian/hd-repo/gnupg`, generated on the first installation when there is none;
- the nginx site `/etc/nginx/sites-available/hd-repo`, which serves `/srv/hd-repo` on port 80 in place of the default site of nginx.

The first, empty indices of both suites are made and signed during the installation, so the repository works as soon as the installation is done.

The package comes with two man pages: `man hd-repo` describes the repository and how to use it, `man hd-repo-update` the command that updates it.

## Building the package
The package is built from `packages/hd-repo` in this repository:
```
cd packages/hd-repo
make deb
```

This builds `build/hd-repo_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `nginx`, `gnupg`, `apt-utils` and `passwd`, and recommends `openssh-server`. Install it with:
```
sudo apt install ./build/hd-repo_<version>_all.deb
```

The files involved are:
```
packages/hd-repo/Makefile
packages/hd-repo/version.json
packages/hd-repo/hd-repo-update
packages/hd-repo/deb/control
packages/hd-repo/deb/postinst
packages/hd-repo/deb/postrm
packages/hd-repo/deb/etc/nginx/sites-available/hd-repo
packages/hd-repo/man/hd-repo.7
packages/hd-repo/man/hd-repo-update.1
```

## Setting up
`repo` logs in with ssh using a key; it has no password. Add the public keys of the people who may upload packages to `/var/lib/hd-repo/.ssh/authorized_keys`, owned by `repo` with mode 0600:
```
sudo install -o repo -g repo -m 0600 id_ed25519.pub \
    /var/lib/hd-repo/.ssh/authorized_keys
```

The SSH server is recommended, not required, by `hd-repo`; install `openssh-server` when it is missing.

## Adding packages
Copy the packages to the inbox of their suite, then run `hd-repo-update` as `repo`:
```
scp foo_1.0_amd64.deb repo@repo.example:inbox/unstable/
ssh repo@repo.example hd-repo-update
```

The inboxes are `inbox/trixie` and `inbox/unstable` in the home directory of `repo`. A package built for trixie goes in the first, one built for unstable in the second; a package that fits both is copied to both.

`hd-repo-update` checks every package in the inboxes, moves them into the pool of their suite, replacing older versions of the same package, and makes and signs the indices. A package that is not valid, or not for amd64 or all, stops the update before anything changes.

### The HyprDebian packages
From the root of this repository:
```
make update-repo
```

This builds every package into `build/`, then runs `update-repo/update-repo.py` with them and the kernels that `make kernel` built into `build/<suite>`, see [hd-kernel](hd-kernel.md), which asks for the host name of the repository, `repo` by default, shows the packages of each suite and asks for a go-ahead. It then copies each package to the inboxes of its suites, `inbox/trixie`, `inbox/unstable` or both, and runs `hd-repo-update` for the suites that got packages there, all as `repo` over ssh, so your ssh key must be in the `authorized_keys` of `repo`.

A build server with [hd-buildserver](hd-buildserver.md) does the same every night, unattended, with the options below, and also uploads the live ISO.

The suites of a package come from the `deploy.json` in its package directory:
```
{
    "repos" : ["trixie", "unstable"]
}
```

Most HyprDebian packages go to both suites. `hd-desktop` goes to unstable only, as they need Hyprland, which trixie does not have.

`make update-repo` hands `update-repo.py` each package as `<suites>:<path>`, such as `trixie,unstable:packages/hd-base_1.8_all.deb`. With `--dry-run` before the packages, it only prints the commands.

Some more options of `update-repo.py`, also given before the packages:
- `--host <host>` uploads to that host, without asking for the host name.
- `--yes` uploads without asking for a go-ahead. With `--host` as well, it asks nothing, so it can run unattended, as on a build server.
- `--only-new` leaves out every package whose file name is already in the pool of its suite, so a rebuild of a version that is already in the repository never replaces it.
- `--iso <path>` also uploads the live ISO to `/srv/hd-repo/iso`, where it can be downloaded from `http://<host>/iso/`. It is copied under a temporary name, checked against its sha256 on the repository and only then renamed into place, with a `SHA256SUMS` next to it.

### Emptying the repository
From the root of this repository:
```
make clean-repo
```

This runs `update-repo.py --clean`, which asks for the host name of the repository, shows how many packages each suite has and asks to type the host name again. Then it removes every package from the pools and the inboxes and everything in `/srv/hd-repo/iso`, and runs `hd-repo-update`, which signs the empty indices. The signing key stays, so the clients stay linked and only find no packages; nothing is removed from them. Use it to start a repository over, such as the dev repository before you fill it again with `make update-repo`.

`hd-repo-update` only takes a ZFS snapshot when it adds packages, so emptying takes none. To be able to undo it, take one first:
```
ssh repo@repo.example /usr/sbin/zfs snapshot rpool/hd-repo@before-clean
```

`--clean` takes no packages, `--iso` or `--only-new`. With `--host` it does not ask for the host name, but it always asks to type it again, also with `--yes`.

## hd-repo-update
```
hd-repo-update [suite...]
```

`hd-repo-update` adds the packages in the inboxes to the repository in `/srv/hd-repo`, then makes the indices of every suite and signs them.

Without arguments, it takes the packages from the inboxes of all suites, `/var/lib/hd-repo/inbox/trixie` and `/var/lib/hd-repo/inbox/unstable`. With arguments, only from the inboxes of the suites named, `trixie` or `unstable`. The indices of all suites are made and signed again either way.

Run it as the user `repo`, without sudo: the repository and the key belong to that user.

An update goes through these steps:
1. Every package in the inboxes is checked first. A file that is not a package, or a package that is not for amd64 or all, stops the update, and the repository and the inboxes stay as they were.
2. When there are packages and `/srv/hd-repo` is a ZFS dataset of its own, a snapshot `<dataset>@hd-repo-update-<date>-<time>` is taken, so the update can be undone with `zfs rollback`. When the snapshot fails, the update stops before it changes anything.
3. Each package is moved from its inbox to the pool of its suite, `/srv/hd-repo/pool/<suite>/main`. Older versions of the same package in that pool are removed.
4. For every suite, the `Packages` index is made with `apt-ftparchive`, then the `Release` file with its hashes, which is signed into `InRelease` and `Release.gpg`. They are made in a temporary directory and moved into place together, so clients never see indices without the signed Release that matches them.
5. The public key is exported to `/srv/hd-repo/hyprdebian-archive-keyring.pgp`.

Without new packages, nothing is snapshotted or moved; the repository is only signed again. Only one `hd-repo-update` runs at a time; a second one stops with an error, kept apart by the lock file `/var/lib/hd-repo/update.lock`.

It exits with 0 when the repository was updated and signed, and with 1 when the update stopped on an error.

## Using the repository
A client needs the public key and an apt source. Download the key from the repository:
```
sudo /usr/lib/apt/apt-helper download-file \
    http://repo.example/hyprdebian-archive-keyring.pgp \
    /usr/share/keyrings/hyprdebian-archive-keyring.pgp
```

and add `/etc/apt/sources.list.d/hyprdebian.sources`:
```
Types: deb
URIs: http://repo.example/
Suites: unstable
Components: main
Signed-By: /usr/share/keyrings/hyprdebian-archive-keyring.pgp
Architectures: amd64
```

For trixie, set `Suites` to `trixie`. On a HyprDebian system, `hd-link-repo` of `hd-base` does both: `sudo hd-link-repo http://repo.example/ --suite unstable`.

The key is downloaded over the same connection as the packages, so it does not protect against someone who can change that traffic. It makes sure that every client keeps getting its packages from the repository it was set up with.

## Layout
```
/srv/hd-repo/
  hyprdebian-archive-keyring.pgp    the public key
  pool/trixie/main/                 the packages of trixie
  pool/unstable/main/               the packages of unstable
  dists/trixie/                     the indices of trixie
  dists/sid/                        the indices of unstable
  dists/stable -> trixie
  dists/unstable -> sid
  iso/                              the live ISO and its SHA256SUMS, from update-repo.py --iso
```

Each directory in `dists` holds `Release`, its signatures `InRelease` and `Release.gpg`, and `main/binary-amd64/Packages(.gz)`.

The suites have their own pools, so a package built for one suite never replaces the one built for the other. Clients may ask for trixie by its codename or as `stable`, and for unstable by its name or as `sid`.

## ZFS
When `/srv/hd-repo` is a ZFS dataset of its own, `hd-repo-update` takes a snapshot of it before it adds packages, so an update can be undone with `zfs rollback`. The installation lets `repo` take those snapshots when the dataset already exists. When the dataset is made later, allow it by hand:
```
sudo zfs allow -u repo snapshot,mount rpool/hd-repo
```

Those snapshots also keep every live ISO that `update-repo.py --iso` replaced. To keep the ISOs out of them, make `/srv/hd-repo/iso` a dataset of its own, owned by `repo`:
```
sudo zfs create rpool/hd-repo/iso
sudo chown repo:repo /srv/hd-repo/iso
```

## The signing key
Every client trusts the key in `/etc/hyprdebian/hd-repo/gnupg`. It has no passphrase, so updates can sign without asking for one. Back it up: with a new key, `apt update` fails on every client until the new public key is downloaded on it, as in [Using the repository](#using-the-repository).

Removing the package keeps everything. Purging it removes the user `repo`, its home directory with the inboxes, and the nginx site, but keeps `/srv/hd-repo` and the key, so a new installation takes them over.

## HTTPS
The repository is served over HTTP. To serve it over HTTPS as well, add a `server` block that listens on port 443 with the certificate and `root /srv/hd-repo` to `/etc/nginx/sites-available/hd-repo`, and change `http://` to `https://` in the sources of the clients.

## Files
| Path | Purpose |
| --- | --- |
| `/srv/hd-repo` | The repository, served by nginx. |
| `/srv/hd-repo/iso` | The live ISO and its `SHA256SUMS`, uploaded by `update-repo.py --iso`. |
| `/var/lib/hd-repo` | The home directory of `repo`. |
| `/var/lib/hd-repo/inbox/trixie`, `/var/lib/hd-repo/inbox/unstable` | The inboxes, emptied by a successful update. |
| `/var/lib/hd-repo/update.lock` | Keeps two updates from running at the same time. |
| `/etc/hyprdebian/hd-repo/gnupg` | The signing key. |
| `/etc/nginx/sites-available/hd-repo` | The nginx site, a conffile. |
| `/usr/bin/hd-repo-update` | Updates the repository. |
