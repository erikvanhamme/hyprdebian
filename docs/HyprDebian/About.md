
**HyprDebian** is a custom, lightweight, CLI/TUI-centric Linux distribution designed as an reproducible, minimal desktop platform. Built on top of **Debian Unstable (Sid)**, it combines modern Wayland compositor performance with enterprise-grade filesystem reliability and declarative system management.

It is developed by Erik Van Hamme (erik.vanhamme@gmail.com) to serve as a daily driver OS for laptop, desktop, VM and server alike. The main goal is to be able to run this as the complete system stack for all my computers.

Hyprdebian is intended to have only 1 user.

## Core System Architecture

- **Base & Package Management:** Built on Debian Sid, utilizing a tailored set of official and custom-compiled Debian packages (`.deb`) hosted on a local HTTP repository.  
- **Storage & Encryption:** Deployed on **ZFS on root** featuring native dataset encryption, mirrored/striped pool topologies, and automated snapshotting for system rollbacks.
- **Session & Process Management:** Uses **`systemd`** as the init system and process supervisor, paired with **`uwsm`** (Universal Wayland Session Manager) to handle session lifecycle management, environment variable scoping, and clean service teardown.
- **Display & Login:** Uses **`greetd`** for minimal console/graphical login into the graphical session.

## Desktop & UI Stack

- **Compositor:** **Hyprland** (Wayland) providing a dynamic, tiled workspace environment.
- **Terminal Emulator:** **kitty**, configured as the primary interface for both local commands and terminal application workflows.
- **Application Launcher:** **wofi** for lightweight, key-driven application launching and dynamic menu prompts.
- **Notification Daemon:** **mako** for minimal desktop notifications.

## Audio & Networking

- **Audio Infrastructure:** Powered by **PipeWire**, utilizing **wiremix** as a TUI mixer for fine-grained audio interface and stream management.
- **Network Stack:** Configured via **`iwd`** (iNet wireless daemon) alongside **`netplan`** or **`systemd-networkd`** for predictable interface configuration.

## Key Workflow Principles

- **TUI/CLI First:** Minimal GUI overhead, prioritizing keyboard-driven productivity tools such as **Yazi** for file management and **MPV** / **audacious** for media playback.
- **Automated & Maintainable:** Installed via modular Bash installation scripts with dependency verification, ensuring precise build environments and reproducible deployments.