# HyprDebian

HyprDebian is a Debian-based distribution built from `hd-*` packages, for the systems of one owner on one local network.

## Language

### The estate

**Estate**:
All HyprDebian systems of the owner on the local network.
_Avoid_: Fleet, network, park

**Managed system**:
A system of the estate whose agent has a token the dashboard knows.
_Avoid_: Host, machine, node, client

**Dashboard**:
The central server of the estate: it keeps the reports of the managed systems and hands their jobs out to them, without being able to make or change a job. An estate has exactly one.
_Avoid_: Controller, server, master

**Manager**:
A system of the estate that makes and signs jobs with a manager key. An estate may have several.
_Avoid_: Job producer, controller, signer, admin station

**Manager key**:
The key a manager signs jobs with. Agents and the dashboard trust a list of them, and a job signed by any key on that list is valid.
_Avoid_: Signing key, master key

**Agent**:
The part of a managed system that sends its reports to the dashboard and fetches and runs its jobs. It only ever connects out to the dashboard.
_Avoid_: Client, daemon, reporter

**Token**:
The secret that identifies one managed system to the dashboard, registered by its first report.
_Avoid_: Key, password, credential

**Report**:
The state of one managed system at one moment, sent by its agent to the dashboard.
_Avoid_: Status, heartbeat

### Jobs

**Command**:
One of the fixed set of actions an agent knows by name and can run; nothing outside that set can be run.
_Avoid_: Script, task, action

**Job**:
One command for one managed system, signed by the manager, to run once, not before a given time and not after it expires.
_Avoid_: Task, schedule, run

**Sequence number**:
The number of a job, given by the manager, which only goes up across the estate: an agent never accepts a job whose sequence number is not higher than that of the last job it accepted.
_Avoid_: Job id, serial

**Scheduled**:
The state of a job that the agent has accepted and set to run at its time, but that has not ended yet.
_Avoid_: Pending, queued, waiting

**Cancelled**:
The end of a scheduled job that the agent dropped because it accepted a newer job with the same command.
_Avoid_: Superseded, replaced

**Result**:
How a job ended on its managed system: ok, failed, expired or cancelled.
_Avoid_: Status, outcome, not ok

**Upgrade**:
The command that upgrades a managed system as `hd-up --unattended` does: a snapshot, then a full upgrade without removals, never a reboot.
_Avoid_: Update (which in apt only refreshes the package lists)
