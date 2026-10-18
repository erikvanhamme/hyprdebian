# Migrating beast from the old installer to HyprDebian 1.13

A one-off plan for this computer (`beast`). Not part of `docs/`, and not meant to be committed: the
hd-* packages are a fresh set and their docs do not mention the old installer.

## Verdict

An in-place migration with the .deb packages is feasible. Nothing in the disk layout stops it: the
pool is still `rpool` with the root on `rpool/hyprdebian`, the ESP is partition 1, and the packages do
not care about partition numbers. Each part of the old setup either maps onto a package, is a file to
delete, or is a decision to make.

Two things make it safe:

- **ZFS snapshots** of `rpool/hyprdebian`, `bpool/hyprdebian` and the home dataset, taken just before
  the migration. A rollback from the live ISO takes a minute.
- **grub stays installed and bootable** until the new bootloader has proven itself. The new
  bootloader gets its own firmware boot entry next to the `debian` and `hyprdebian` grub entries.

Expect about two hours, most of it the bootloader phase and checking. Work **at the console, not over
ssh**: the network and the bootloader change underneath you.

## What is on this machine now

Found by looking at the system read-only on 2026-10-18:

| Area | State |
|---|---|
| Old installer | v0.198, checkout in `/usr/src/hyprdebian` at 3879aaf, migrations 0001 to 0023 applied (no 0024 Secure Boot keys, no 0025 art/font packages) |
| Disk | one NVMe (`nvme-Samsung_SSD_990_PRO_4TB_S7DPNU0X405865A`): p1 ESP 1 GiB (1013 MiB free), p2 swap 16 GiB with a random key at every boot, p3 `bpool` 4 GiB, p4 `rpool` 3.6 TiB, encrypted |
| Datasets | `rpool/hyprdebian` (/), `bpool/hyprdebian` (/boot), `rpool/home/rsq`, `rpool/vmdk/*` (zvols of the VMs dash, git, newrepo, repo, test) |
| Boot | grub through shim, `BootCurrent` 0001 `debian` (`\EFI\debian\shimx64.efi`), 0000 `hyprdebian`. Secure Boot **off**, firmware in **Setup Mode** (no PK). DKMS MOK in `/var/lib/dkms/mok.pub` |
| Kernel command line | `root=ZFS=rpool/hyprdebian video=3840x2160-32@120 quiet` |
| Network | `br0` with MAC `ba:99:20:c9:ab:16`, DHCP, port `enp16s0`; files `10-ethernet.link`, `30-br0.netdev`, `40-br0.network`, `50-ethx.network` in `/etc/systemd/network` |
| VMs | dash, git, **repo** and test running. `http://repo/` is the VM on this machine, so it is down whenever beast's network or libvirt is |
| Apt | `debian.sources`, `hyprdebian.local.sources` (`http://repo/`, unstable), `claude-desktop.list` |
| Old packages | `hyprdebian-art` 1, `yazi` 26.8.15, `wiremix` 0.11.0, `ouch` 0.8.1, `orcaslicer` 2.4.2, `zram-tools`, `desktop-base`, grub + shim, `rsyslog`, docker.io, cups, libvirt/qemu, open-iscsi, mbuffer, mt-st |
| Local additions | ollama (`/usr/local/bin/ollama`, `/usr/local/lib/ollama`, `ollama.service`), claude-desktop, `/stor` NFS mount in fstab, `systemd-poweroff.service.d/override.conf`. **Keep all of these** |

## Issues and pitfalls

Ordered by how badly they bite.

### Blockers: these make apt fail or the system misbehave if not handled

1. **`/etc/issue` has a local diversion; the postinst of hd-base fails on it.** The old installer ran
   `dpkg-divert --local --rename --divert /etc/issue.debian --add /etc/issue`. The postinst of hd-base
   adds the same diversion for the package hd-base, and dpkg-divert refuses. Checked with a dry run
   against a copy of the diversions database:
   `'diversion of /etc/issue to /etc/issue.debian by hd-base' clashes with 'local diversion of /etc/issue to /etc/issue.debian'`.
   hd-base is then left half-configured, and so is everything that depends on it.
   **Fix:** remove the local diversion before installing (phase 2).

2. **The old scripts in `/usr/local/bin` shadow the packaged ones.** `/usr/local/bin` comes before
   `/usr/bin` in `PATH` and in sudo's `secure_path`. `hd-up`, `hd-reboot`, `hd-poweroff`, `hd-backup*`,
   `hd-restore-vm`, `hd-mount*`, `hd-yazi`, the screensaver and volume scripts would keep running the
   old versions without any warning, and `sudo hd-up` would too. `hd-mode-select` there even belongs to
   `rsq` and is writable by it.
   **Fix:** delete them once the packages are in (phase 5). Keep `ollama`.

3. **Your Hyprland config calls `/usr/local/bin/...` by full path.** `hyprland.lua` lines 235 to 240
   and `hypridle.conf` lines 7 and 15. Once the old scripts are gone, the reboot, poweroff, mount,
   unmount, yazi and mode-select bindings and the screensaver stop working.
   **Fix:** point them at `/usr/bin` before deleting the old scripts (phase 5).

4. **`hd-art` and `hyprdebian-art` ship the same four wallpapers.** hd-art has no Replaces, so dpkg
   stops with "trying to overwrite ... which is also in package hyprdebian-art".
   **Fix:** remove `hyprdebian-art` first (phase 2).

5. **`/etc/systemd/system-preset/00-ignore-all.preset` is still there.** hd-base removed it in 1.1, but
   only a migration would have deleted it here. While it is there, the services of the new packages
   are not enabled when they are installed (hd-report.timer, hd-dashboard.service and every Debian
   service you add later).
   **Fix:** delete it before installing (phase 2).

6. **Seven unowned files in `/etc` are conffiles of the new packages and differ from them.** dpkg
   stops and asks about each one ("File on system created by you or by a script"): in `/etc/skel`,
   `.bash_aliases`, `.config/hypr/hypridle.conf`, `hyprland.lua`, `hyprlock.conf`, `hyprpaper.conf`
   and `.config/yazi/yazi.toml`, plus `/etc/update-motd.d/00-hyprdebian`. Nobody edited them; they are
   just older copies. The other 20 match the packages byte for byte, so dpkg takes them without asking.
   **Fix:** move the seven aside before installing (phase 2).

7. **The machine ID is replaced at every shutdown**, by the old `rotate-machine-id.service`, with
   `systemd-machine-id-commit.service` and `systemd-preset-all.service` masked. The hd-base postinst
   unmasks those two only on an upgrade from hd-base before 1.1, never on a first installation like
   this one. Left alone, beast keeps a behaviour that new installs no longer have, and journald stays
   volatile (see 10).
   **Fix:** disable and delete the rotation, unmask the two units (phase 5).

### Risky steps: they work, but can cut you off

8. **The network moves to hd-network.** hd-network has no postinst and renders nothing until it is
   told to, so installing it changes nothing. But **hd-qemu-kvm sets up br0 itself on its first
   installation** when the hd-network settings have no br0 yet: it makes a bridge with a **new random
   MAC**, which gets a different DHCP lease, and with more than one wired interface it asks a debconf
   question.
   **Fix:** put the existing bridge in the hd-network settings, **with the old MAC**, before
   installing hd-qemu-kvm. hd-qemu-kvm then leaves it alone. The render writes `30-br0.netdev` and
   `40-br0.network` with the same names (adding `RouteMetric=100` to `[IPv6AcceptRA]`) and a new
   `50-enp16s0.network`. It leaves the old `50-ethx.network` alone because that file lacks its header,
   so delete that by hand. `networkctl reload` may make br0 drop for a moment, and with it the VMs.

9. **The bootloader switch (grub → hd-bootloader-gfx).** This is the part that can leave beast
   unbootable, and the reason grub stays until the end.
   - The postinst does not write the ESP. But in the same apt run, the initramfs trigger of
     hd-plymouth (or any kernel update) runs `update-esps`, which writes `EFI/BOOT/BOOTX64.EFI`,
     `EFI/BOOT/assets.efi` and `EFI/Linux/HyprDebian-Kernel-*.efi`. That is harmless: grub lives in
     `EFI/debian` and `EFI/hyprdebian`, which are not touched. It does mean the defaults are written
     first, so give the bootloader its settings and run `update-esps` again.
   - **No firmware boot entry is made.** On a fresh install the installer adds it (`pkg_boot_entries`);
     here you add it yourself with efibootmgr.
   - Default settings are for 1920x1080. Beast needs `res_x` 3840, `res_y` 2160 and the 3840x2160
     wallpaper, or plymouth and the desktop start in the wrong mode (the graphical frontend adds
     `video=<res_x>x<res_y>`).
   - UKI size: each one holds the 106 MB initrd; at most 4 kernels fit easily in the 1013 MiB free.
   - Secure Boot is off, so signing does not matter yet. hd-secboot generates keys in
     `/etc/hyprdebian/secboot` that are used for signing, and enrolling them (Setup Mode makes that
     possible) is a separate decision, best made after the switch works.
   - `/boot` stays on `bpool`. update-bootloader reads `/boot/vmlinuz-*` from wherever `/boot` is
     mounted, so that works; the grub compatibility of bpool just stops mattering. Folding /boot into
     rpool, as on fresh installs, is optional (phase 7).

10. **Logging changes.** The old `/etc/systemd/journald.conf` (a modified systemd conffile) sets
    `Storage=volatile` and `ForwardToSyslog=yes`, with rsyslog installed. New systems keep the logs in
    the persistent journal and have no syslog daemon. Nothing breaks if you leave it, but beast would
    be the odd one out.

11. **The apt source keeps its old name.** hd-base renames `hyprdebian.local.sources` to
    `hyprdebian.sources` only on an upgrade from hd-base before 1.11, not on a first installation.
    `hd-link-repo`, `hd-issue` and `hd-report` look for `hyprdebian.sources`. Running `hd-link-repo`
    writes the new file but leaves the old one, which gives apt the same source twice.

12. **The repo VM is on this machine.** While br0 or libvirt is being changed, `http://repo/` is
    unreachable, and the repo does not have the hd-* packages yet anyway (`apt-cache policy hd-base`
    finds nothing). Install from the `build/` folder of the checkout instead, and link the repo
    afterwards.

### Minor: cleanups and one-time effects

13. **zram:** hd-base conflicts with zram-tools, so apt removes it and stops `zramswap.service`
    (`swapoff` of the zram, which moves its pages into RAM or the disk swap). systemd-zram-generator
    sets up zram0 at the next boot. Delete `/etc/sysctl.d/50-zram.conf` (the same file is in
    `/usr/lib/sysctl.d`) and purge zram-tools for `/etc/default/zramswap`.
14. **desktop-base:** hd-plymouth (a Recommends of hd-desktop) conflicts with it, so apt removes it.
    Only the plymouth/grub theme came from it.
15. **greetd:** the hd-desktop postinst diverts `/etc/greetd/config.toml` with `--rename`, which
    moves your old rendered config to `config.toml.greetd`, then renders a new one for the user with
    the lowest UID (rsq). The result is the same. `/etc/greetd/config.toml.dpkg-dist` is an old leftover.
16. **Docker:** the hd-docker postinst restarts docker.service on its first installation. No containers
    are running now. `/etc/docker/daemon.json` matches the package, and `/etc/ufw/after.rules` already
    has the `# BEGIN/END DOCKER UFW FIX` block, which the postinst replaces in place.
17. **libvirt:** the hd-qemu-kvm postinst diverts `/etc/libvirt/libvirtd.conf` and `secret.conf`
    with `--rename` (your current ones become `*.libvirt`), writes its own and runs
    `systemctl try-restart libvirtd`. Running VMs survive a libvirtd restart. The masks and the
    `10-secret.conf` drop-in become files of the package. Delete `/etc/modules-load.d/br_netfilter.conf`
    and `/etc/sysctl.d/50-bridge-netfilter.conf` (now in `/usr/lib`).
18. **VM backups:** the old `backup-vms.timer` is enabled, but `/etc/hyprdebian/vm-backups` lists no
    VMs. hd-backup has `hd-backup-vms.timer`, which is not enabled, with its VMs in the key `hd-backup`
    of `/etc/hyprdebian/config.json`. Delete the old units and list.
19. **iSCSI:** `iscsi-login.service` and `/etc/udev/rules.d/99-iscsi-custom.rules` were not carried
    over into any package. Keep them as local files if you still use the tape over iSCSI, or delete
    them along with open-iscsi.
20. **Mount menu:** `/etc/sudoers.d/99-mount-menu` allows `/usr/local/bin/hd-mount-helper`; hd-desktop
    ships `/etc/sudoers.d/hd-desktop` for `/usr/libexec/hd-desktop/hd-mount-helper`. Delete the old one.
21. **Duplicates that override the packages:** `/etc/systemd/network/10-ethernet.link`,
    `/etc/systemd/resolved.conf.d/00-direct-uplink.conf` and
    `/etc/systemd/system/getty@.service.d/numlock.conf` are the same as the files in `/usr/lib` and win
    over them. Delete them, or later package changes will not reach beast.
22. **Fonts:** 98 JetBrainsMono files in `/usr/local/share/fonts` duplicate hd-fonts.
23. **Old leftovers:** `/usr/src/hyprdebian`, `/etc/hyprdebian/installer.config`, `migrations`,
    `vm-backups`, `/etc/skel/.config/hyprdebian/screensaver.txt` (hd-screensaver has its default in
    `/usr/share`), `/etc/skel/Pictures/wallpapers`. Your own `~/.config/hyprdebian/screensaver.txt`
    is still read by hd-screensaver.
24. **Versions:** `ouch`, `orcaslicer` and `yazi` upgrade to the packages in `build/`. `wiremix` does not:
    the build makes 0.9.0-1, older than the 0.11.0 installed from the old repo, so apt keeps 0.11.0.
    That is fine here, but fresh installs get 0.9.0 (see the note at the end).
25. **DHCP:** after the rotation stops, the DHCP client ID (based on the machine ID) changes one last
    time and then stays the same. A reservation on the router by client ID may need updating once.
26. **Root's dotfiles** came from the old skel. Fresh installs copy `/etc/skel` to `/root`; run
    `hd-sync-skel` as root if you want the same.

## Strategy

All commands as root at the console (`sudo -i`), from the checkout at `/home/rsq/Projects/hyprdebian`.
Use `B=/home/rsq/Projects/hyprdebian/build`. Keep a log: `script /root/hd-migration.log`.

### Phase 0: preparation (as rsq, nothing changes on the system)

1. Build every package from a clean tree: `make` in the root of the checkout, so `build/` holds
   exactly what is committed (hd-base 1.13).
2. **Rehearse in a clone (recommended).** Phases 2 to 4 can be run in a `systemd-nspawn` container on
   ZFS clones of the root and /boot. That catches dpkg and postinst failures (1, 4 and 6) without
   touching the real system:
   ```bash
   zfs snapshot rpool/hyprdebian@rehearsal
   zfs snapshot bpool/hyprdebian@rehearsal
   zfs clone -o mountpoint=/mnt/rehearsal rpool/hyprdebian@rehearsal rpool/rehearsal
   zfs clone -o mountpoint=/mnt/rehearsal/boot bpool/hyprdebian@rehearsal bpool/rehearsal
   mkdir -p /mnt/rehearsal/boot/efi
   systemd-nspawn -D /mnt/rehearsal --bind-ro=$B:/build --tmpfs=/boot/efi
   ```
   Inside, run phases 2 to 4 with `B=/build`. Postinsts see no booted system there, so hd-qemu-kvm
   does not touch the bridge, nothing restarts, and `update-esps` writes to the tmpfs. Afterwards:
   `zfs destroy bpool/rehearsal; zfs destroy rpool/rehearsal; zfs destroy rpool/hyprdebian@rehearsal; zfs destroy bpool/hyprdebian@rehearsal`.
3. Shut down the VMs (`virsh shutdown dash git repo test`), or accept that their network drops for
   a moment in phase 3 and that libvirtd restarts in phase 4.

### Phase 1: safety net

The copies come first, so the snapshot holds them and a rollback brings them back too:

```bash
mkdir -p /root/hd-migration
tar -C /boot/efi -cf /root/hd-migration/esp.tar .
efibootmgr -v > /root/hd-migration/efibootmgr.txt
dpkg --get-selections > /root/hd-migration/selections.txt
cp -a /etc /root/hd-migration/etc
# One command per pool: zfs snapshot cannot take snapshots in two pools at once.
zfs snapshot rpool/hyprdebian@pre-hd-migration rpool/home/rsq@pre-hd-migration
zfs snapshot bpool/hyprdebian@pre-hd-migration
```

Make sure you have the HyprDebian live ISO on a USB stick (`make iso`), for a rollback (see the end).

### Phase 2: clear the way

```bash
# 1. The local diversion of /etc/issue. The banner in /etc/issue is written again by hd-issue.
rm /etc/issue
dpkg-divert --local --rename --remove /etc/issue
cat /etc/issue        # now the one of base-files

# 4. The old art package, which holds the files of hd-art.
apt remove hyprdebian-art

# 5. The preset that keeps new services from being enabled.
rm /etc/systemd/system-preset/00-ignore-all.preset

# 6. The unowned old copies of conffiles that differ from the packages.
mkdir -p /root/hd-migration/conffiles
for f in /etc/skel/.bash_aliases /etc/skel/.config/hypr/hypridle.conf \
         /etc/skel/.config/hypr/hyprland.lua /etc/skel/.config/hypr/hyprlock.conf \
         /etc/skel/.config/hypr/hyprpaper.conf /etc/skel/.config/yazi/yazi.toml \
         /etc/update-motd.d/00-hyprdebian; do
    mkdir -p "/root/hd-migration/conffiles$(dirname "$f")"
    mv "$f" "/root/hd-migration/conffiles$f"
done
```

Hyprland keeps running on the wallpaper it has loaded; the new path arrives with hd-art in phase 4.

### Phase 3: base packages and the network

```bash
apt update
apt install $B/hd-config_0.1_all.deb $B/hd-network_0.2_all.deb $B/hd-base_1.13_all.deb
```

apt removes zram-tools and installs systemd-zram-generator. Check that hd-base is configured
(`dpkg -s hd-base | grep Status`), that `/etc/issue` shows the HyprDebian banner and that
`/etc/hyprdebian/version.json` says 1.13.

Then take over the network in hd-network, with the MAC that br0 has now:

```bash
hd-network-bridge set br0 --mac ba:99:20:c9:ab:16 --dhcp
hd-network-iface set enp16s0 --bridge br0
hd-network-render --dry-run      # 30-br0.netdev, 40-br0.network, 50-enp16s0.network
rm /etc/systemd/network/50-ethx.network /etc/systemd/network/10-ethernet.link
hd-network-render
networkctl status br0            # still the same address
```

### Phase 4: the other packages

```bash
apt install $B/hd-art_0.1_all.deb $B/hd-fonts_0.1_all.deb \
    $B/hd-qemu-kvm_0.1_all.deb $B/hd-docker_0.1_all.deb $B/hd-cups_0.1_all.deb \
    $B/hd-backup_0.3_amd64.deb \
    $B/yazi_26.9.1-1_amd64.deb $B/ouch_0.8.3-2_amd64.deb $B/orcaslicer_2.4.2-1_amd64.deb \
    $B/hd-screensaver_0.1_all.deb $B/hd-plymouth_0.2_all.deb $B/hd-desktop_0.5_all.deb
```

- hd-qemu-kvm must print "br0 is already in the network settings; leaving it as it is." If it asks
  the debconf question instead, phase 3 did not take: choose none.
- apt removes desktop-base (hd-plymouth). The initramfs is rebuilt; with no hd-bootloader yet, no ESP
  is touched.
- Optional: `hd-report` (pointed at the dash VM) and anything else this machine should report.

### Phase 5: remove the old setup

First the user config (as rsq), so the key bindings keep working:

```bash
sed -i 's|/usr/local/bin/|/usr/bin/|g' ~/.config/hypr/hyprland.lua ~/.config/hypr/hypridle.conf
hd-sync-skel        # compare the rest of your configs with the new /etc/skel; keep, take or merge
hyprctl reload
```

Then as root:

```bash
# 2. The old scripts. ollama stays.
cd /usr/local/bin && rm blockwrite hd-ask-poweroff hd-ask-reboot hd-backup hd-backup-load \
    hd-backup-prep hd-backup-unload hd-backup-vm hd-backup-vms hd-cmd-screensaver \
    hd-kill-screensaver hd-launch-screensaver hd-mode-select hd-mount hd-mount-helper hd-poweroff \
    hd-reboot hd-restore-vm hd-rotate-machine-id hd-unmount hd-up hd-volume-down hd-volume-mute \
    hd-volume-up hd-yazi
hash -r; type hd-up      # /usr/bin/hd-up

# 7. The machine ID rotation.
systemctl disable rotate-machine-id.service
rm /etc/systemd/system/rotate-machine-id.service
systemctl unmask systemd-machine-id-commit.service systemd-preset-all.service

# 18. The old VM backups.
systemctl disable --now backup-vms.timer
rm /etc/systemd/system/backup-vms.service /etc/systemd/system/backup-vms.timer /etc/hyprdebian/vm-backups

# 20, 21, 13, 17. Old files that the packages now ship elsewhere.
rm /etc/sudoers.d/99-mount-menu
rm /etc/systemd/resolved.conf.d/00-direct-uplink.conf /etc/systemd/system/getty@.service.d/numlock.conf
rm /etc/sysctl.d/50-zram.conf /etc/sysctl.d/50-bridge-netfilter.conf /etc/modules-load.d/br_netfilter.conf
apt purge zram-tools
systemctl daemon-reload

# 10. Logging as on new systems: the persistent journal, no rsyslog. Remove the two lines from
#     [Journal] in /etc/systemd/journald.conf, then:
apt purge rsyslog
systemctl restart systemd-journald

# 11. The apt source under its new name.
mv /etc/apt/sources.list.d/hyprdebian.local.sources /etc/apt/sources.list.d/hyprdebian.sources

# 22, 23. Leftovers.
rm -r /usr/local/share/fonts/JetBrainsMono* /usr/src/hyprdebian
rm /etc/hyprdebian/installer.config /etc/hyprdebian/migrations
rm -r /etc/skel/.config/hyprdebian /etc/skel/Pictures
fc-cache -f
```

Decide on 19 (iSCSI) separately. **Reboot (still through grub)** and check: login banner and motd,
autologin into Hyprland, key bindings, the screensaver, `zramctl`, `swapon` (zram0 and the encrypted
swap), `networkctl` (br0 with the same address), the VMs, docker, cups,
`cat /etc/machine-id` before and after a second reboot (the same now), `journalctl -b -1` (persistent).

### Phase 6: the bootloader

```bash
apt install $B/hd-secboot_0.1_all.deb $B/hd-bootloader-gfx_0.4_amd64.deb
```

The postinst stores the defaults in the key `hd-bootloader`. Set beast's values (see
hd-bootloader(7) for the keys) and write the ESP:

```bash
python3 - <<'EOF'
import json, subprocess
s = json.load(open("/etc/hyprdebian/config.json"))["hd-bootloader"]
s.update(res_x=3840, res_y=2160,
         wallpaper="/usr/share/backgrounds/hyprdebian/emerald-3840x2160.png")
subprocess.run(["hd-config-update", "hd-bootloader", json.dumps(s)], check=True)
EOF
/usr/lib/hd-bootloader/update-esps
ls -l /boot/efi/EFI/BOOT /boot/efi/EFI/Linux
efibootmgr --create --disk /dev/nvme0n1 --part 1 --label HyprDebian --loader '\EFI\BOOT\BOOTX64.EFI'
efibootmgr        # HyprDebian first, then debian and hyprdebian (grub)
```

If 120 Hz matters on the console, add `"extra": ["video=3840x2160-32@120"]` to `cmdline`; it comes
after the `video=` of the frontend, and the kernel uses the last one.

Reboot. Expected: the HyprDebian menu, the hd-plymouth splash asking for the ZFS password, Hyprland.
If anything goes wrong, pick `debian` in the firmware boot menu and you are back on grub, with
everything from phases 2 to 5 in place. Keep grub for a few days and at least one kernel update, to
see `zz-hd-bootloader` write new UKIs.

### Phase 7: finish (after a few good days)

1. Remove grub and shim: `apt purge grub-efi-amd64 grub-efi-amd64-signed shim-signed` (mokutil too,
   if nothing else needs it); then `rm -r /boot/grub /boot/efi/EFI/debian /boot/efi/EFI/hyprdebian`,
   `rm /etc/default/grub /etc/grub.d/11_zfs_recovery` if left over, and delete the two old boot
   entries (`efibootmgr -b 0000 -B`, `efibootmgr -b 0001 -B`). The recovery entry of grub is now the
   `(recovery)` profile in the menu of the new bootloader.
2. Link the repository, once it has the hd-* packages (`make update-repo`): `hd-link-repo http://repo/`.
   Check that `/etc/apt/sources.list.d` has only `hyprdebian.sources` next to debian and claude-desktop.
3. Optional, **fold /boot into rpool** as on fresh installs. With the ESP unmounted
   (`systemctl stop boot-efi.automount boot-efi.mount`): copy `/boot` aside, `zfs unmount bpool/hyprdebian`,
   `zfs set canmount=off bpool/hyprdebian`, copy the files back into the now plain `/boot` folder of
   `rpool/hyprdebian`, `mkdir /boot/efi`, start the automount again, `update-initramfs -u -k all`,
   reboot. Once that works, `zpool destroy bpool`. Partition 3 then stays empty; the layout keeps
   differing from a fresh install (swap p2, root p4 instead of p3), which nothing depends on.
4. Optional, Secure Boot: `hd-secboot-enroll`, then turn on Secure Boot in the firmware. Keep a way
   back: the firmware setup can clear the keys again.
5. Destroy the snapshots `@pre-hd-migration` once you are sure.

## Rollback

- **Before phase 6:** boot as usual, or from the live ISO, and roll back:
  `zfs rollback -r rpool/hyprdebian@pre-hd-migration` and `zfs rollback -r bpool/hyprdebian@pre-hd-migration`.
  The home dataset only changes with `hd-sync-skel` and the `sed` of phase 5; roll it back only if you
  want to undo those, as a rollback also loses everything else written to it since.
- **After phase 6:** the same, plus restore the ESP from the copy in the rolled-back root
  (`rm -r /boot/efi/EFI/BOOT /boot/efi/EFI/Linux`, then `tar -C /boot/efi -xf /root/hd-migration/esp.tar`)
  and delete the HyprDebian boot entry. grub and its entries are only gone
  after phase 7, so until then picking `debian` in the firmware boot menu is enough to boot.
- From the live ISO: `zpool import -f -R /mnt rpool` (and `bpool`), `zfs load-key rpool`, roll back,
  then `zpool export -a`.

## Note for the packages (not part of the migration)

- `packages/wiremix` builds 0.9.0, while the repo and beast have 0.11.0. Fresh installs get an older
  wiremix than this machine has. Worth bumping `WIREMIX_VERSION`.
