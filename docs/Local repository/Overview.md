## Introduction
The local hyprdebian repository holds packages that are not in Debian, like `yazi` and `wiremix`. They are built for `unstable`, so the installer only offers the repo on that suite.

The repository is signed. The installer downloads the public key from the repo and installs it as `/usr/share/keyrings/hyprdebian-archive-keyring.pgp`. `hyprdebian.local.sources` points `Signed-By` at that file, so apt only trusts that key for this repo, and rejects indices and packages that it did not sign.

The repo is meant for the local network. The key is downloaded over the same connection as the packages, so it does not protect against someone who can change that traffic. It does make sure that every system gets its packages from the repo they were set up with.

## The repo VM
The repo runs on its own Debian trixie VM, installed with a ZFS root pool `rpool`:
- The repo is on its own dataset, `rpool/hyprdebian-repo`, mounted on `/srv/hyprdebian-repo`. The repo user may snapshot it, through `zfs allow`.
- nginx serves `/srv/hyprdebian-repo` on port 80, in place of the default site.
- The signing key is in `/var/lib/hyprdebian-repo/gnupg`. It has no passphrase, so updates can sign without asking for one.
- `/etc/hyprdebian-repo.conf` holds the user that maintains the repo and these paths.
- docker runs the package builds.

The files involved are:
```
repo/scripts/setup_hyprdebian_repo.sh
repo/scripts/update_hyprdebian_repo.sh
repo/scripts/create_deb_*.sh
```

## Setting up a new VM
On a fresh trixie VM, as the user that will maintain the repo, clone this repo, then run from `repo/scripts`:
```
sudo ./setup_hyprdebian_repo.sh
```

The script installs nginx, gnupg, apt-utils and docker, adds the user to the docker group, creates the dataset and lets the user snapshot it, generates the signing key, sets up nginx, and makes and signs a first, empty index. Running it again keeps the dataset, the key and the packages.

Log out and in again before building packages, so the user is in the docker group.

The key only exists on the VM. When the VM is replaced, a new key is generated, and every installed system needs the new key. See [Changing the key](#changing-the-key).

## Adding packages
From `repo/scripts`, build the packages and add them to the repo:
```
./create_deb_yazi.sh
./update_hyprdebian_repo.sh
```

The `create_deb_*.sh` scripts write their package to `output`. When there are new packages, `update_hyprdebian_repo.sh` first takes a snapshot `rpool/hyprdebian-repo@update-<date>-<time>`, so an update can be undone with `zfs rollback`. Without new packages, the repo is only signed again, and no snapshot is taken. It moves the packages into the pool, removing older versions of the same package. Then it makes the `Packages` index and a `Release` file with its hashes, and signs that into `InRelease` and `Release.gpg`.

Run `update_hyprdebian_repo.sh` as the repo user, without sudo. The key and the repo belong to that user.

## Layout
```
/srv/hyprdebian-repo/
├── hyprdebian-archive-keyring.pgp   public key, downloaded by the installer
├── pool/main/                       the packages
└── dists/
    ├── sid/                         Release, InRelease, Release.gpg
    │   └── main/binary-amd64/       Packages, Packages.gz
    └── unstable -> sid
```

The suite is `unstable` with the codename `sid`. Installed systems ask for `unstable`, the `Q_SUITE` they were installed with.

## Changing the key
An installed system gets the key once: `b_apt_init` downloads it on a new installation, and the `0022-signed-repo` migration on an upgrade. After a new key, `apt update` fails on those systems because of the signature. To fix that, download the new key on each system:
```
sudo /usr/lib/apt/apt-helper download-file http://<repo>/hyprdebian-archive-keyring.pgp /usr/share/keyrings/hyprdebian-archive-keyring.pgp
```
