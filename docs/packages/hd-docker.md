## Introduction
The package `hd-docker` installs Docker, from `docker.io`, and Docker Compose, from `docker-compose`, and sets them up to work with the ufw firewall, which it also depends on.

Installing the package:
- installs `/etc/docker/daemon.json`, which gives the bridge `docker0` the address `192.168.253.1/24` and the networks that Docker makes a `/24` each from `192.168.240.0/20`, clear of the usual home and office networks;
- puts the Docker rules in `/etc/ufw/after.rules`, so the ports that containers publish do not bypass ufw, and reloads ufw when it is on;
- restarts Docker on the first installation, so it takes the addresses of `daemon.json`. `docker.io` started it with its default addresses before. Later installations do not restart it, as that stops the containers that run.

It does not add anyone to the group `docker`. To use Docker without sudo, add your user to it and log in again:
```
sudo usermod -a -G docker <user>
```

Members of that group control Docker, and so the whole system.

The package comes with a man page: `man hd-docker`.

## Building the package
The package is built from `hd-docker` in this repository:
```
cd hd-docker
make deb
```

This builds `build/hd-docker_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3`, `docker.io`, `docker-compose` and `ufw`. Install it with:
```
sudo apt install ./build/hd-docker_<version>_all.deb
```

The files involved are:
```
hd-docker/Makefile
hd-docker/version.json
hd-docker/docker-ufw.rules
hd-docker/deb/control
hd-docker/deb/postinst
hd-docker/deb/postrm
hd-docker/deb/etc/docker/daemon.json
hd-docker/man/hd-docker.7
```

## Addresses
`/etc/docker/daemon.json` is a conffile of `hd-docker`. Restart Docker after changing it:
```
sudo systemctl restart docker
```

When you change the address pool, change the `RETURN` rule of the firewall block to match; see below.

## Firewall
Docker publishes the ports of containers with rules of its own, which come before those of ufw, so a published port would be open whatever ufw says. `hd-docker` puts rules in the chain `DOCKER-USER`, which Docker checks first:
1. Replies to connections that a container made, or that were allowed, pass.
2. The route rules of ufw are checked, so a container is reached only when one of them allows it.
3. Traffic from the networks of Docker, `192.168.240.0/20`, passes.
4. All other traffic to the containers is dropped.

To open a published port, allow it with a route rule. Docker has already sent the traffic on to the container by then, so the rule names the port inside the container, not the published one. For `-p 8080:80`:
```
sudo ufw route allow proto tcp to any port 80
```

The rules are in a block at the end of `/etc/ufw/after.rules`, between the lines `# BEGIN DOCKER UFW FIX` and `# END DOCKER UFW FIX`. ufw keeps that file up to date with ucf, so it belongs to ufw and `hd-docker` does not ship it. Every installation and upgrade of `hd-docker` puts the block in from `/usr/share/hd-docker/docker-ufw.rules` and keeps the rest of the file; ufw is reloaded when the block changed and the firewall is on. Changes made inside the block are lost then, so put your own rules outside it. Removing the package takes the block out again.

## Files
| Path | Purpose |
| --- | --- |
| `/etc/docker/daemon.json` | The addresses of the networks of Docker, a conffile. |
| `/etc/ufw/after.rules` | The rules of ufw, with the block of `hd-docker`. |
| `/usr/share/hd-docker/docker-ufw.rules` | The block that goes in `/etc/ufw/after.rules`. |
