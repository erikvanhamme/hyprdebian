# Builds the package hd-qemu-kvm, which installs Qemu with KVM and libvirt and
# sets them up to run virtual machines on the bridge br0.
#
#   make deb    build build/hd-qemu-kvm_$(DEB_VERSION)_all.deb, with the
#               version from version.json. It installs:
#                 /etc/skel/.config/libvirt/libvirt.conf
#                                             connects virsh and virt-manager
#                                             of new users to qemu:///system,
#                                             a conffile
#                 /etc/systemd/system/libvirtd-tls.socket
#                 /etc/systemd/system/libvirtd-tcp.socket
#                                             links to /dev/null, which mask
#                                             the TLS and TCP sockets of
#                                             libvirtd
#                 /etc/systemd/system/libvirtd.service.d/10-secret.conf
#                                             a link to /dev/null, which
#                                             masks the encryption key of the
#                                             secrets that libvirt gives
#                                             libvirtd
#                 /usr/lib/modules-load.d/br_netfilter.conf
#                                             loads br_netfilter at the boot
#                 /usr/lib/sysctl.d/50-bridge-netfilter.conf
#                                             keeps the traffic over bridges
#                                             out of the firewall
#                 /usr/share/hd-qemu-kvm/libvirtd.conf
#                 /usr/share/hd-qemu-kvm/secret.conf
#                 /usr/share/hd-qemu-kvm/libvirt-guests
#                                             the files in libvirt/, which the
#                                             postinst writes to /etc/libvirt
#                                             and /etc/default
#                 /usr/share/man/man7/hd-qemu-kvm.7.gz
#                                             the man page in man/
#               with the control file deb/control, the debconf question
#               deb/templates and the maintainer scripts deb/postinst, which
#               diverts the configs of libvirt, writes those of hd-qemu-kvm
#               and makes the chosen wired interfaces ports of br0 on the
#               first installation, and deb/postrm, which puts the configs
#               of libvirt back on a removal. Raise the version in
#               version.json for every package that is handed out.
#   make deb-path
#               print the path of the package that make deb builds,
#               relative to this directory, for the Makefile in the root
#   make clean  remove build/

BUILD := build
PYTHON := python3

DEB_NAME := hd-qemu-kvm
DEB_VERSION := $(shell $(PYTHON) -c 'import json; v = json.load(open("version.json")); print("%d.%d" % (v["major"], v["minor"]))')
DEB := $(BUILD)/$(DEB_NAME)_$(DEB_VERSION)_all.deb
STAGE := $(BUILD)/deb
SHARE_DIR := /usr/share/$(DEB_NAME)
UNIT_DIR := /etc/systemd/system
LIBVIRT_CONF := /etc/skel/.config/libvirt/libvirt.conf
# The configs of libvirt that the postinst writes.
LIBVIRT_CONFIGS := libvirt/libvirtd.conf libvirt/secret.conf libvirt/libvirt-guests
# The units and drop-ins that the package masks with a link to /dev/null.
MASKS := libvirtd-tls.socket libvirtd-tcp.socket libvirtd.service.d/10-secret.conf
# The maintainer scripts in deb/.
SCRIPTS := deb/postinst deb/postrm
# The man pages, named <page>.<section>.
MANPAGES := man/hd-qemu-kvm.7

.PHONY: deb deb-path clean

deb: $(DEB)

deb-path:
	@echo $(DEB)

$(BUILD):
	mkdir -p $@

# The package is built from a staged root in build/deb. dpkg-deb makes all
# files owned by root.
$(DEB): version.json deb$(LIBVIRT_CONF) $(LIBVIRT_CONFIGS) br_netfilter.conf \
		50-bridge-netfilter.conf deb/control deb/templates $(SCRIPTS) $(MANPAGES) | $(BUILD)
	rm -rf $(STAGE)
	install -d $(STAGE)/DEBIAN $(STAGE)$(SHARE_DIR) $(STAGE)/usr/lib/modules-load.d \
		$(STAGE)/usr/lib/sysctl.d
	install -D -m 0644 deb$(LIBVIRT_CONF) $(STAGE)$(LIBVIRT_CONF)
	printf '%s\n' $(LIBVIRT_CONF) > $(STAGE)/DEBIAN/conffiles
	for unit in $(MASKS); do \
		install -d $$(dirname $(STAGE)$(UNIT_DIR)/$$unit); \
		ln -s /dev/null $(STAGE)$(UNIT_DIR)/$$unit; \
	done
	install -m 0644 br_netfilter.conf $(STAGE)/usr/lib/modules-load.d/
	install -m 0644 50-bridge-netfilter.conf $(STAGE)/usr/lib/sysctl.d/
	install -m 0644 $(LIBVIRT_CONFIGS) $(STAGE)$(SHARE_DIR)/
	for page in $(MANPAGES); do \
		dir=$(STAGE)/usr/share/man/man$${page##*.}; \
		install -d $$dir; \
		gzip -9n < $$page > $$dir/$$(basename $$page).gz; \
		chmod 0644 $$dir/$$(basename $$page).gz; \
	done
	sed -e 's/@VERSION@/$(DEB_VERSION)/' \
		-e "s/@INSTALLED_SIZE@/$$(du -sk --apparent-size --exclude=DEBIAN $(STAGE) | cut -f1)/" \
		deb/control > $(STAGE)/DEBIAN/control
	install -m 0644 deb/templates $(STAGE)/DEBIAN/
	install -m 0755 $(SCRIPTS) $(STAGE)/DEBIAN/
	dpkg-deb --root-owner-group --build $(STAGE) $@

clean:
	rm -rf $(BUILD)
