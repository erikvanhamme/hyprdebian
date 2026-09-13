## Introduction
This section of the documentation describes the general layout of the hyprdebian installed.
## Dependency system
The hyprdebian installer has a dependency system. The root of the installation work is the empty install bash method:
```
install() {
	return 0
}
```

All dependencies are tied to that method.

The system works by attempting to execute the install method. If the system sees that there are dependent tasks it needs to execute, these are executed first.

The dependency handling methods are in the file:
```
scripts/dependencies.sh
```

Notable methods are:
1. add_dependencies method dependency_a dependency_b
2. execute_task method

The names are self-explanatory.

When execute_task is called, the dependency tree of that method is resolved, and all methods marked as a dependencies of the task will be executed before the method supplied as argument will run. Please note that this is recursive. Dependencies can have dependencies.

## Phases

The hyprdebian installer is structured in a set of phases. 

There are the following phases:
1. Questions: User is asked for the main inputs.
2. Prerequisites: Optionally will install the prerequisites for installation. Skipped when running on the hyprdebian live cd.
3. Disk: Handles cleaning and preparation for the disks.
4. Partitions: Sets up the partitions.
5. Filesystems: Creates the filesystems.
6. Base: Sets up the base system and makes it bootable.
7. Optional: Installs the optional component selection.
8. Files: Deploys the files that should be deployed in the target system.
9. Templates: Renders out templates to files on the target system. Includes the user input in the target system files.
10. Services: Enables the needed services in the target system
11. User: Sets up the user account for the main user.
12. Cleanup: Unmounts and cleans in the target system.

Each phase consists of 3 empty bash methods:
1. x_pre
2. x_main
3. x_post

These methods can be used to attach dependencies to. This can be done either statically (by programming in the bash scripts), or dynamically in response to user input.

There is a bash script per phase in the scripts directory.

Below is the template for a phase script:
```
#!/bin/bash

x_pre() {
	return 0
}

x_main() {
	return 0
}

x_post() {
	return 0
}

add_dependencies "install" "x_pre" "x_main" "x_post"
```

The phase scripts are sourced in the main installer script:

```
# Load tasks/deps for all the phases, in order.
source scripts/questions.sh
source scripts/prereqs.sh
source scripts/disk.sh
source scripts/partition.sh
source scripts/filesystem.sh
source scripts/base.sh
source scripts/optional.sh
source scripts/packages.sh
source scripts/files.sh
source scripts/templates.sh
source scripts/services.sh
source scripts/user.sh
source scripts/cleanup.sh
```

## Idempotency

The installer is made to be idempotent. If one of the tasks crashes or fails to execute, it is possible to just start the script again after the bug was corrected.

The installer will skip all the tasks that completed successfully.

At the end of a succesful task, a .done file with the task name is written in /tmp/hyprdebian.