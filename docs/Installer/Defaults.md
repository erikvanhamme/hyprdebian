## Introduction
Most [questions](Questions.md) of the installer have a default answer. The defaults depend on the machine type: a desktop gets the hyprland desktop, a server Qemu/KVM, and so on. The machine type itself, `Q_TYPE`, and the suite, `Q_SUITE`, are asked by `hd-installer` (see [hd-installer](../packages/hd-installer.md)).

The defaults are defined in:
```
scripts/defaults.sh
```

There is one table with the defaults shared by all machine types, `DEFAULTS_common`, and one table per machine type, `DEFAULTS_<type>`. The keys of the tables are the names of the question variables. Yes/no questions use `y` and `n`.

The machine types are listed in `MACHINE_TYPES`:
```
MACHINE_TYPES=(desktop laptop server vm)
```

## Common defaults
These defaults are the same for every machine type (`DEFAULTS_common`):

| Key | Question | Default |
| --- | --- | --- |
| `Q_DEFAULTS` | Use machine type defaults | `y` |
| `Q_SPECIFIC_KERNEL` | Do you wish to download a specific kernel | `n` |
| `Q_KERNEL_REPO` | Which repository to download a kernel from | `stable` |
| `Q_CUPS` | Enable CUPS | `n` |
| `Q_RUST` | Enable rust and syscargo support | `n` |
| `Q_REPORT` | Send status reports to the hyprdebian dashboard | `y` |
| `Q_REPORT_URL` | Hyprdebian dashboard URL | `http://dash.rsq-online.ddns.net/` |

## Machine type defaults
These defaults differ per machine type (`DEFAULTS_desktop`, `DEFAULTS_laptop`, `DEFAULTS_server` and `DEFAULTS_vm`):

| Key | Question | desktop | laptop | server | vm |
| --- | --- | --- | --- | --- | --- |
| `Q_DESKTOP` | Install hyprland desktop environment | `y` | `y` | `n` | `n` |
| `Q_QEMU_KVM` | Enable Qemu/KVM | `n` | `n` | `y` | `n` |
| `Q_BACKUPTOOLS` | Install backup tools | `y` | `y` | `y` | `n` |

## Questions without a default
`Q_KERNEL`, the kernel version when a specific kernel is downloaded, has no default, and is always asked.

## How the defaults are resolved
### Looking up a default
Each question looks up its default with `get_default`, passing the machine type when it is known:
```
local default="$(get_default Q_QEMU_KVM "${Q_TYPE:-}")"
```

`get_default key [type]` works as follows:
1. If a machine type is given and its table has the key, that value is used.
2. Otherwise, if `DEFAULTS_common` has the key, that value is used.
3. Otherwise there is no default.

So a machine type table only needs the keys where that type differs from the common defaults, and a value in a machine type table overrides the value in `DEFAULTS_common`. At the moment the two sets of keys do not overlap.


### Answering with the default
The default is then passed to one of the ask functions. With the answer to "Use machine type defaults" (`Q_DEFAULTS`), the user decides whether the defaults are used without asking:
- **`Q_DEFAULTS` is no:** every question is asked. The default is shown as the suggested answer, and pressing Enter accepts it.
- **`Q_DEFAULTS` is yes:** every question that has a default is answered with it without asking. The answer is printed, so the user can still see what was chosen. Questions without a default are still asked.

Some questions are asked even when `Q_DEFAULTS` is yes, because the machine type defaults cannot know the right answer. They are listed in `DEFAULTS_ALWAYS_ASK`:
```
DEFAULTS_ALWAYS_ASK=(
    Q_DEFAULTS
)
```

The check whether a question is answered without asking is done by `accept_default`.

### Values that are already set
When the variable of a question already has a value, for example because it was loaded from the [configuration file](Idempotency.md#state), the ask functions use that value instead of the default, both as the suggested answer and as the automatic answer. For `ask_yes_no`, a saved `true` counts as `y` and a saved `false` as `n`.

Whether a question is answered without asking still depends on the default only: a question without a default is always asked, even when its variable already has a value. The value is then only the suggested answer.

### Checks
The defaults are checked so that mistakes in the tables are found early:
- When `scripts/defaults.sh` is loaded, every machine type in `MACHINE_TYPES` must have a `DEFAULTS_<type>` table. Otherwise the installer stops with an error.
- `get_default` fails for a machine type that is not in `MACHINE_TYPES`.
- `ask_yes_no` fails when the default is not `y`, `n` or empty.

## Changing the defaults
- **To change a default:** change the value in `DEFAULTS_common` or in the table of the machine type.
- **To give a question a default:** add its variable to `DEFAULTS_common`, or to the machine type tables when it differs per type. Make sure the question passes the result of `get_default` to its ask function.
- **To add a machine type:** add it to `MACHINE_TYPES` and declare a `DEFAULTS_<type>` table for it. The table only needs the keys where the new type differs from `DEFAULTS_common`, but every key that the other machine type tables have should get a value, or that question has no default for the new type.
