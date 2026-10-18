"""The settings of hd-network, shared by hd-network-render, hd-network-iface and
hd-network-bridge.

The package hd-network installs this as /usr/lib/hd-network/hd_network.py.

The settings are the key hd-network in /etc/hyprdebian/config.json. They are read from that
file directly and written with hd-config-update, or removed with hd-config-remove when nothing
is left.
"""

import ipaddress
import json
import os
import random
import re
import subprocess
import sys

CONFIG_FILE = "/etc/hyprdebian/config.json"
CONFIG_KEY = "hd-network"
CONFIG_UPDATE = "/usr/sbin/hd-config-update"
CONFIG_REMOVE = "/usr/sbin/hd-config-remove"

DEFAULT_METRIC = 100
# An interface name, or a glob that matches interface names.
NAME_RE = re.compile(r"^[A-Za-z0-9._:*?\[\]-]{1,15}$")
MAC_RE = re.compile(r"^[0-9a-f]{2}(:[0-9a-f]{2}){5}$", re.IGNORECASE)
ADDRESS_FIELDS = ["dhcp", "addresses", "gateway", "dns", "metric"]


def fail(message):
    print(f"Error: {message}", file=sys.stderr)
    sys.exit(1)


def require_root():
    if os.geteuid() != 0:
        fail("This script must be run as root.")


def is_glob(name):
    return any(c in name for c in "*?[")


# The .network file of an interface. networkd uses the first file that matches an interface,
# so the globs come after the exact names, and an exact name overrides a glob.
def interface_file(name):
    if is_glob(name):
        return "59-" + re.sub(r"[^A-Za-z0-9._-]", "_", name) + ".network"
    return f"50-{name}.network"


def random_mac():
    # Locally administered and unicast: bit 1 of the first byte set, bit 0 clear.
    first = (random.randrange(256) & 0xFC) | 0x02
    return ":".join(f"{b:02x}" for b in [first] + [random.randrange(256) for _ in range(5)])


def load_settings():
    if not os.path.exists(CONFIG_FILE):
        return {}
    try:
        with open(CONFIG_FILE, encoding="utf-8") as f:
            config = json.load(f)
    except (OSError, ValueError) as e:
        fail(f"Cannot read {CONFIG_FILE}: {e}")
    if not isinstance(config, dict):
        fail(f"{CONFIG_FILE} does not hold a JSON object.")
    settings = config.get(CONFIG_KEY, {})
    if not isinstance(settings, dict):
        fail(f"{CONFIG_KEY} in {CONFIG_FILE} is not a JSON object.")
    return settings


# Writes the settings with the hd-config tools. Empty sections are dropped, and the key is
# removed when nothing is left.
def save_settings(settings):
    for section in ("interfaces", "bridges"):
        if section in settings and not settings[section]:
            del settings[section]
    if settings:
        subprocess.run([CONFIG_UPDATE, CONFIG_KEY, json.dumps(settings)], check=True)
    else:
        subprocess.run([CONFIG_REMOVE, CONFIG_KEY], check=True)


def check_object(where, value):
    if not isinstance(value, dict):
        fail(f"{where} is not a JSON object.")


def check_fields(where, entry, allowed):
    check_object(where, entry)
    unknown = sorted(set(entry) - set(allowed))
    if unknown:
        fail(f"{where} has unknown fields: {', '.join(unknown)}")


def as_list(where, value):
    if isinstance(value, str):
        return [value]
    if isinstance(value, list) and all(isinstance(v, str) for v in value):
        return value
    fail(f"{where} is not a string or a list of strings.")


# Checks the address fields of an interface or a bridge and fills in the defaults.
def addressing(where, entry):
    dhcp = entry.get("dhcp", True)
    if not isinstance(dhcp, bool):
        fail(f"{where}.dhcp is not true or false.")
    metric = entry.get("metric", DEFAULT_METRIC)
    if not isinstance(metric, int) or isinstance(metric, bool) or metric < 0:
        fail(f"{where}.metric is not a number of 0 or more.")
    addresses = as_list(f"{where}.addresses", entry.get("addresses", []))
    gateways = as_list(f"{where}.gateway", entry.get("gateway", []))
    dns = as_list(f"{where}.dns", entry.get("dns", []))
    try:
        for address in addresses:
            if "/" not in address:
                raise ValueError(f"{address} has no prefix length, as in 192.168.1.10/24")
            ipaddress.ip_interface(address)
        for address in gateways + dns:
            ipaddress.ip_address(address)
    except ValueError as e:
        fail(f"{where}: {e}")
    return {"dhcp": dhcp, "metric": metric, "addresses": addresses, "gateways": gateways,
            "dns": dns}


# Checks the settings and returns them with the defaults filled in:
#   bridges     name -> mac (None when missing) and the address fields
#   interfaces  name -> bridge (None when not a port) and, when not a port, the address fields
#   wait_online True, False or None
# Stops with an error on the first problem.
def check_settings(settings):
    check_fields(CONFIG_KEY, settings, ["interfaces", "bridges", "wait_online"])
    bridges = settings.get("bridges", {})
    interfaces = settings.get("interfaces", {})
    check_object(f"{CONFIG_KEY}.bridges", bridges)
    check_object(f"{CONFIG_KEY}.interfaces", interfaces)
    checked = {"bridges": {}, "interfaces": {}, "wait_online": settings.get("wait_online")}

    for name, entry in bridges.items():
        where = f"{CONFIG_KEY}.bridges.{name}"
        if not NAME_RE.match(name) or is_glob(name):
            fail(f"{where}: {name} is not a valid bridge name.")
        check_fields(where, entry, ADDRESS_FIELDS + ["mac"])
        mac = entry.get("mac")
        if mac is not None and (not isinstance(mac, str) or not MAC_RE.match(mac)):
            fail(f"{where}.mac is not a MAC address, as in 52:54:00:12:34:56.")
        checked["bridges"][name] = {"mac": mac, **addressing(where, entry)}

    files = {}
    for name, entry in interfaces.items():
        where = f"{CONFIG_KEY}.interfaces.{name}"
        if not NAME_RE.match(name):
            fail(f"{where}: {name} is not a valid interface name or glob.")
        if name in bridges:
            fail(f"{where}: {name} is also a bridge.")
        file = interface_file(name)
        if file in files:
            fail(f"{where}: its file {file} is already used by {files[file]}.")
        files[file] = name
        if "bridge" in entry:
            check_fields(where, entry, ["bridge"])
            if entry["bridge"] not in bridges:
                fail(f"{where}.bridge: {entry['bridge']} is not in {CONFIG_KEY}.bridges.")
            # A wifi interface cannot be a port: an access point only accepts frames for the
            # MAC address of the interface itself.
            if name.startswith("wl"):
                fail(f"{where}: a wifi interface cannot be a port of a bridge.")
            checked["interfaces"][name] = {"bridge": entry["bridge"]}
        else:
            check_fields(where, entry, ADDRESS_FIELDS)
            checked["interfaces"][name] = {"bridge": None, **addressing(where, entry)}

    if checked["wait_online"] is not None and not isinstance(checked["wait_online"], bool):
        fail(f"{CONFIG_KEY}.wait_online is not true or false.")
    return checked


# The command line options for the address fields, shared by hd-network-iface and
# hd-network-bridge.
def add_address_options(parser):
    parser.add_argument("--dhcp", action="store_true", default=None,
                        help="get addresses with DHCP and router advertisements (the default)")
    parser.add_argument("--no-dhcp", dest="dhcp", action="store_false",
                        help="do not use DHCP or router advertisements")
    parser.add_argument("--address", action="append", metavar="ADDRESS/PREFIX",
                        help="a static address, as in 192.168.1.10/24; may be given more than "
                        "once, and replaces the addresses")
    parser.add_argument("--gateway", action="append", metavar="ADDRESS",
                        help="the default gateway; may be given more than once, for IPv4 and "
                        "IPv6, and replaces the gateways")
    parser.add_argument("--dns", action="append", metavar="ADDRESS",
                        help="a DNS server; may be given more than once, and replaces the DNS "
                        "servers")
    parser.add_argument("--metric", type=int, metavar="N",
                        help=f"the metric of the routes (default {DEFAULT_METRIC})")


def address_options_given(args):
    return any(getattr(args, option) is not None
               for option in ("dhcp", "address", "gateway", "dns", "metric"))


def apply_address_options(entry, args):
    if args.dhcp is not None:
        entry["dhcp"] = args.dhcp
    if args.address is not None:
        entry["addresses"] = args.address
    if args.gateway is not None:
        entry["gateway"] = args.gateway
    if args.dns is not None:
        entry["dns"] = args.dns
    if args.metric is not None:
        entry["metric"] = args.metric


# One line about the address fields, for the list commands.
def describe_addressing(a):
    parts = ["dhcp" if a["dhcp"] else "no dhcp"]
    if a["addresses"]:
        parts.append("addresses " + ", ".join(a["addresses"]))
    if a["gateways"]:
        parts.append("gateway " + ", ".join(a["gateways"]))
    if a["dns"]:
        parts.append("dns " + ", ".join(a["dns"]))
    parts.append(f"metric {a['metric']}")
    return "; ".join(parts)


def print_table(rows):
    if not rows:
        return
    width = max(len(name) for name, _ in rows)
    for name, text in rows:
        print(f"{name:<{width}}  {text}")
