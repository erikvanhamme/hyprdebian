## Introduction
The questions phase is special: its tasks mostly ask questions and, depending on the answers, add [dependencies](Dependencies.md) and [queue](Queues.md) entries for the later phases. Nothing is changed on the disks until all questions are answered.

The questions are in:
```
scripts/questions.sh
```

## Asking questions
Every answer is stored in a variable with the `Q_` prefix (`Q_TYPE`, `Q_HOSTNAME`, `Q_ENCRYPT`, ...). The questions are asked with these helpers from `scripts/helpers.sh`:

| Function | Purpose |
| --- | --- |
| `ask var prompt [default]` | Asks for a free text value. |
| `ask_options var prompt default option...` | Asks to pick one of a numbered list of options. |
| `ask_yes_no var prompt [default]` | Asks a yes/no question. Stores `true` or `false`, and succeeds on yes, so it can be used in an `if`. |
| `ask_password var prompt` | Asks for a password twice. The password is never saved. |

Each answer (except the password) is saved in the configuration file immediately. See [Idempotency](Idempotency.md).

Which questions are asked depends on earlier answers. For example, `q_wifi` only asks for the wifi interface when wifi is enabled.

`Q_SUITE`, the Debian suite that is installed, is not asked. `q_suite` sets it to `unstable`, the suite hyprdebian is based on, unless a saved value exists.

## Machine type defaults
The defaults for the questions depend on the machine type (`desktop`, `laptop`, `server` or `vm`). They are defined in:
```
scripts/defaults.sh
```

`DEFAULTS_common` holds the defaults shared by all machine types, and `DEFAULTS_<type>` holds the defaults where a machine type differs. A question looks up its default with `get_default`.

When the user chooses to use the machine type defaults (`Q_DEFAULTS`), all questions that have a default are answered with it without asking, except the ones in `DEFAULTS_ALWAYS_ASK`. Questions without a default are always asked.

See [Defaults](Defaults.md) for the list of defaults and how they are resolved.
