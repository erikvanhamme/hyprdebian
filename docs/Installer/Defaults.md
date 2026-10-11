## Introduction
Most [questions](Questions.md) of the installer have a default answer. The defaults depend on the machine type the user selects: a laptop needs wifi, a server gets redundant drives, and so on.

The defaults are defined in:
```
scripts/defaults.sh
```

There is one table with the defaults shared by all machine types, `DEFAULTS_common`, and one table per machine type, `DEFAULTS_<type>`. The keys of the tables are the names of the question variables. Yes/no questions use `y` and `n`.

The machine types are listed in `MACHINE_TYPES`, in the order in which they are offered:
```
MACHINE_TYPES=(desktop laptop server vm)
```

## Common defaults
These defaults are the same for every machine type (`DEFAULTS_common`):

| Key | Question | Default |
| --- | --- | --- |
| `Q_OS` | Which OS to install | `hyprdebian` |
| `Q_TYPE` | What type of computer are you installing | `desktop` |
| `Q_DEFAULTS` | Use machine type defaults | `y` |
| `Q_DE` | Which desktop environment (Debian only) | `kde` |
| `Q_SWAP` | Size of swap partition in GiB | `2` |
| `Q_USER` | Username | `rsq` |
| `Q_FULLNAME` | Full name | `Erik Van Hamme` |
| `Q_HOSTNAME` | Hostname | `hyprdebian` |
| `Q_SPECIFIC_KERNEL` | Do you wish to download a specific kernel | `n` |
| `Q_KERNEL_REPO` | Which repository to download a kernel from | `stable` |
| `Q_FIREWALL` | Install firewall | `y` |
| `Q_CUPS` | Enable CUPS | `n` |
| `Q_RUST` | Enable rust and syscargo support | `n` |
| `Q_OPENSSH` | Enable OpenSSH server | `y` |
| `Q_REPO` | Local hyprdebian repository URL | `http://repo.rsq-online.ddns.net:8080/` |

## Machine type defaults
These defaults differ per machine type (`DEFAULTS_desktop`, `DEFAULTS_laptop`, `DEFAULTS_server` and `DEFAULTS_vm`):

| Key | Question | desktop | laptop | server | vm |
| --- | --- | --- | --- | --- | --- |
| `Q_REDUNDANT` | Install on redundant drives | `n` | `n` | `y` | `n` |
| `Q_IFACE` | Wired interface for DHCP | | | | `enp1s0` |
| `Q_WIFI` | Enable wifi | `n` | `y` | `n` | `n` |
| `Q_DESKTOP` | Install hyprland desktop environment | `y` | `y` | `n` | `n` |
| `Q_ENCRYPT` | Encrypt root filesystem | `y` | `y` | `y` | `n` |
| `Q_DOCKER` | Enable docker | `n` | `n` | `y` | `n` |
| `Q_QEMU_KVM` | Enable Qemu/KVM | `n` | `n` | `y` | `n` |
| `Q_BACKUPTOOLS` | Install backup tools | `y` | `y` | `y` | `n` |
| `Q_REPO_ENABLED` | Enable hyprdebian repo | `y` | `y` | `n` | `n` |

An empty cell means the machine type has no default for that question. Only the `vm` type has a default wired interface, because the interface name of a virtual machine is predictable.

## Questions without a default
These questions have no default, and are always asked:
- `Q_DISK`, or `Q_DISK_A` and `Q_DISK_B` for redundant drives: the target disks.
- `Q_DESTROY`: the confirmation that all data on a disk will be destroyed.
- `Q_PASSWORD`: the password for the user account and disk encryption.
- `Q_IFACE`: the wired interface, except on the `vm` machine type.
- `Q_WIFACE`: the wifi interface, when wifi is enabled.
- `Q_KERNEL`: the kernel version, when a specific kernel is downloaded.

## How the defaults are resolved
### Looking up a default
Each question looks up its default with `get_default`, passing the machine type when it is known:
```
local default="$(get_default Q_SWAP "${Q_TYPE:-}")"
```

`get_default key [type]` works as follows:
1. If a machine type is given and its table has the key, that value is used.
2. Otherwise, if `DEFAULTS_common` has the key, that value is used.
3. Otherwise there is no default.

So a machine type table only needs the keys where that type differs from the common defaults, and a value in a machine type table overrides the value in `DEFAULTS_common`. At the moment the two sets of keys do not overlap.

The machine type is not known yet for the first questions: `Q_OS` and `Q_TYPE` are asked before it is selected, so on a fresh run they only use `DEFAULTS_common`.

A question can also have its own fallback when there is no default in the tables. `Q_FQDN` has no entry in the tables and falls back to the hostname followed by `.rsq-online.ddns.net`.

### Answering with the default
The default is then passed to one of the ask functions. With the answer to "Use machine type defaults" (`Q_DEFAULTS`), the user decides whether the defaults are used without asking:
- **`Q_DEFAULTS` is no:** every question is asked. The default is shown as the suggested answer, and pressing Enter accepts it.
- **`Q_DEFAULTS` is yes:** every question that has a default is answered with it without asking. The answer is printed, so the user can still see what was chosen. Questions without a default are still asked.

Some questions are asked even when `Q_DEFAULTS` is yes, because the machine type defaults cannot know the right answer. They are listed in `DEFAULTS_ALWAYS_ASK`:
```
DEFAULTS_ALWAYS_ASK=(
    Q_OS
    Q_TYPE
    Q_DEFAULTS
    Q_REDUNDANT
    Q_HOSTNAME
)
```

The check whether a question is answered without asking is done by `accept_default`.

### Values that are already set
When the variable of a question already has a value, for example because it was loaded from the [configuration file](Idempotency.md#state), `ask` and `ask_options` use that value instead of the default, both as the suggested answer and as the automatic answer. `ask_yes_no` always uses the default.

### Checks
The defaults are checked so that mistakes in the tables are found early:
- When `scripts/defaults.sh` is loaded, every machine type in `MACHINE_TYPES` must have a `DEFAULTS_<type>` table. Otherwise the installer stops with an error.
- `get_default` fails for a machine type that is not in `MACHINE_TYPES`.
- `ask_options` fails when the default is not one of its options.
- `ask_yes_no` fails when the default is not `y`, `n` or empty.

## Changing the defaults
- **To change a default:** change the value in `DEFAULTS_common` or in the table of the machine type.
- **To give a question a default:** add its variable to `DEFAULTS_common`, or to the machine type tables when it differs per type. Make sure the question passes the result of `get_default` to its ask function.
- **To add a machine type:** add it to `MACHINE_TYPES` and declare a `DEFAULTS_<type>` table for it. The table only needs the keys where the new type differs from `DEFAULTS_common`, but every key that the other machine type tables have should get a value, or that question has no default for the new type.
