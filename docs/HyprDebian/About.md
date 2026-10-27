
**HyprDebian** is a custom, lightweight, CLI/TUI-centric Linux distribution designed as an reproducible, minimal desktop platform. Built on top of **Debian Unstable (Sid)**, it combines modern Wayland compositor performance with enterprise-grade filesystem reliability and declarative system management.

It is developed by Erik Van Hamme (erik.vanhamme@gmail.com) to serve as a daily driver OS for laptop, desktop, VM and server alike. The main goal is to be able to run this as the complete system stack for all my computers.

HyprDebian is intended to have only 1 user.

## Core System Architecture

- **Base & Package Management:** Built on Debian Sid, utilizing a tailored set of official and custom-compiled Debian packages (`.deb`) hosted on a HyprDebian HTTP repository ([hd-repo](../packages/hd-repo.md)). A system without the desktop can also run on Debian trixie.
- **Storage & Encryption:** Deployed on **ZFS on root** featuring native encryption, on a single drive or a mirror of two, with a snapshot before every upgrade by `hd-up` for system rollbacks.
- **Boot:** **hd-bootloader**, a small UEFI bootloader that boots unified kernel images from the EFI system partitions, signed with Secure Boot keys of the owner ([hd-bootloader](../packages/hd-bootloader.md), [hd-secboot](../packages/hd-secboot.md)).
- **Session & Process Management:** Uses **`systemd`** as the init system and process supervisor, paired with **`uwsm`** (Universal Wayland Session Manager) to handle session lifecycle management, environment variable scoping, and clean service teardown.
- **Display & Login:** Uses **`greetd`** to log the user in to the graphical session at boot, without a password ([hd-desktop](../packages/hd-desktop.md)).
- **Login Banner:** Console and SSH logins show the HyprDebian logo with the installed version and suite, the maintainer, and the state of the system: kernel, uptime, load, memory, ZFS pools, RAID arrays and failed units (`/etc/update-motd.d/00-hyprdebian`). The console shows the version and suite above the login prompt (`/etc/issue`).

## Desktop & UI Stack

- **Compositor:** **Hyprland** (Wayland) providing a dynamic, tiled workspace environment.
- **Terminal Emulator:** **kitty**, configured as the primary interface for both local commands and terminal application workflows.
- **Application Launcher:** **hd-menu**, the menu of HyprDebian, for key-driven application launching and the menu prompts.
- **Notification Daemon:** **mako** for minimal desktop notifications.
- **Theme:** **hd-theme** gives the desktop programs, the boot splash and the bootloader the colours, font and geometry of one theme ([hd-theme](../packages/hd-theme.md)).

## Audio & Networking

- **Audio Infrastructure:** Powered by **PipeWire**, utilizing **wiremix** as a TUI mixer for fine-grained audio interface and stream management.
- **Network Stack:** **`systemd-networkd`**, with files rendered from the HyprDebian configuration ([hd-network](../packages/hd-network.md)), and **`iwd`** (iNet wireless daemon) for wifi. Interfaces are named by their path for predictable interface configuration.

## Key Workflow Principles

- **TUI/CLI First:** Minimal GUI overhead, prioritizing keyboard-driven productivity tools such as **Yazi** for file management and **MPV** / **audacious**, with its terminal interface, for media playback.
- **Automated & Maintainable:** Installed by [hd-installer](../packages/hd-installer.md), a tree of Bash tasks that asks all questions first and continues after a failure, ensuring precise build environments and reproducible deployments.
