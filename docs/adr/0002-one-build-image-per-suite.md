# Everything is compiled in the build image of its suite

Every package, and every test or program tried on the computer itself, is compiled in a container of a build image: `hd-build:trixie` or `hd-build:unstable`, both from `buildenv/Dockerfile` with the suite as an argument. Nothing in the repository compiles on the host. A binary compiled on unstable can need a newer glibc than trixie has, and `dpkg-shlibdeps` writes the Depends of a package from the libraries it was built against, so a package for both repos is built on trixie and one for unstable only on unstable, whatever the host runs. Tests and tryouts are compiled in the image of the suite of their package and run on the host, a HyprDebian system on unstable, which has their runtime dependencies installed; a binary of trixie runs there too. The build server builds the same way as a computer you develop on, in a container of the trixie build image.

## Considered Options

- **Compile on the host, as blockwrite of hd-backup and hd-bootloader did.** Rejected: on an unstable host `blockwrite` could need a newer glibc than trixie has, while the package goes to both repos, and the build would depend on what the host has installed.
- **One image for both suites.** Rejected: it would need a chroot of the other suite inside it, two build environments in one.
- **An image per package, or stock `debian` images that install their build tools on every run.** Rejected: three Dockerfiles and six build scripts held the build dependencies, every build installed its packages and rustup again, and each pull of a changed suite left an untagged image behind.

## Consequences

- The build dependencies of a package are in `buildenv/Dockerfile`, not in its own directory; one Dockerfile holds the packages of both suites, in a shared layer and a layer per suite, as notcurses for example exists in unstable only.
- `make test` and the tryouts of `make all` in a package directory need docker on the host.
- A package goes to both repos only when a system of trixie needs it. ouch, wiremix and yazi, which only hd-desktop depends on, go to unstable only and are built there, as hd-desktop is.
