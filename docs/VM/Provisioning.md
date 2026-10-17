## Introduction
A HyprDebian vm is provisioned on the local computer and installed there with the live cd. Once it runs, it can be moved to the server where it will live.

`make provision-vm`, in the root of the repository, asks the questions for a new vm, then defines it in libvirt (`qemu:///system`) and starts it from the live ISO.

## Before provisioning
1. Build the live ISO with `make -C livecd iso` (see [Live cd](../Livecd/Overview.md)). The vm boots from `livecd/out/hyprdebian-live-amd64.iso`.
2. Make the zvol of the vm, for example:
```
sudo zfs create -V 64G rpool/vmdk/<name>
```

## Provisioning
```
make provision-vm
```

The questions:

| Question | Answers | Default |
| --- | --- | --- |
| Name of the vm | A lower case letter, then lower case letters, digits and `-`. It must not be the name of a vm in libvirt yet. | |
| Zvol of the vm | One of the zvols of the computer, by number. Zvols that are a disk of another vm cannot be picked. | The zvol with the name of the vm, otherwise the first. |
| Number of cpus | 1 up to the number of cpus of the computer. | 2 |
| Memory of the vm | 4, 8, 16 or 32 GiB, by number. | 4 GiB |
| Network bridge | A network bridge of the computer. | `br0` |

It then shows the answers and asks whether to define and start the vm; anything but `y` changes nothing. Open the console of the vm, for example with:
```
virt-manager --connect qemu:///system --show-domain-console <name>
```

and install HyprDebian with `sudo hd-installer` (see [Live cd](../Livecd/Overview.md)).

`python3 vm/provision-vm.py --dry-run` asks the same questions, but only prints the domain XML.

## The vm
The domain is rendered from the jinja2 template `vm/domain.xml.j2`:
- UEFI firmware with secure boot enabled and no keys enrolled. libvirt picks the firmware (`OVMF_CODE_4M.secboot.fd`) and makes the nvram from the empty varstore (`OVMF_VARS_4M.fd`), so the firmware starts in setup mode: secure boot is not enforced until keys are enrolled.
- The q35 machine, with the cpu of the host passed through and SMM, which secure boot needs.
- The zvol as a virtio disk with discard, booted first, and the live ISO as a cdrom, booted when the zvol is still empty.
- A virtio network card on the bridge, a serial console, the qemu guest agent channel, spice graphics with a virtio gpu, a watchdog, a memory balloon and a random number generator.

libvirt makes the uuid, the MAC address and the PCI addresses.

The files involved are:
```
Makefile
vm/provision-vm.py
vm/domain.xml.j2
```
