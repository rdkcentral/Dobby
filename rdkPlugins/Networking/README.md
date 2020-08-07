# Dobby RDK Networking Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to setup networking in the container.

```json
{
    "rdkPlugins": {
        "networking": {
            "required": true,
            "data": {
                "type": "nat",
                "ipv4": "true",
                "ipv6": "true",
                "dnsmasq": "true",
                "holes": [
                    {
                        "port": 1234,
                        "protocol": "tcp"
                    },
                    {
                        "port": 5678,
                        "protocol": "udp"
                    }
                ]
            }
        }
    }
}
```
If you already have other RDK plugins in the bundle, then just add the networking plugin. Do not create multiple `rdkPlugin` sections.

## Options
### Network type
The network type determines what network access the container is allowed.

To set the network type, set the `type` options in the `data` section of the plugin configuration (as shown in the Quick Start section).

There are three network types to choose between - `open`, `nat` and `none`:

##### Open

In open networking mode, the network is shared with the host network. This should be avoided wherever possible as it is hard to secure.

```json
"data": {
    "type": "open"
}
```

##### NAT

In NAT networking mode, the Networking plugin creates a bridge device `dobby0` with iptables rules to configure NATing. Each container set up with NAT gets its own virtual ethernet adaptor as a port of the `dobby0` bridge device, named `vethX`.

The adaptor is visible inside the container, usually named `eth0`, depending on the external interfaces defined in Dobby settings.

For NAT, you can choose to enable or disable IPv4 and IPv6 as you please. If both are set to false or not set at all, the Networking plugin defaults to IPv4.

With IPv4 enabled, each container gets a unique IPv4 address from an address pool range of `100.64.11.2` - `100.64.11.252` (total pool size = 250).

With IPv6 enabled, each container gets a unique IPv6 address in the subnet `2080:d0bb:1e::0/64`. The unique address is determined by merging the container's IPv4 address into the last 32 bits of the IPV6 address. For example, with `100.64.11.2`, the IPv6 address of the container would be `2080:d0bb:1e::6440:b02`, up to `2080:d0bb:1e::6440:bfc`.

```json
"data": {
    "type": "nat",
    "ipv4": "true",
    "ipv6": "false"
}
```

##### None

In this network mode, the container will only see a loopback device `lo`, which will only loopback inside the container, i.e. the container cannot access anything outside on the host or inside other containers.

```json
"data": {
    "type": "none"
}
```

The external interfaces can be any interfaces on the host device that are to be used in setting up Networking for containers.

### Dnsmasq

The `dnsmasq` data field can be used to allow a container to talk to the dnsmasq server running outside the container. Essentially it just routes traffic sent to the `dobby0` bridge on port 53 to the localhost interface on the host.

If the `dnsmasq` field is empty, Networking plugin defaults to not setting up dnsmasq access to the container.

Only usable with network types 'open' and 'nat'.

```json
    "data": {
        "dnsmasq": "true"
    }
```

### Holepuncher

The `holes` data field can be used to punch holes in the firewall to enable port forwarding to allow containered processes to run servers.

The `protocol` field can be omitted in which case TCP will be specified.

Only usable with network types 'nat' and 'none'.

```json
    "data": {
        "holes": [
            {
                "port": 1234,
                "protocol": "tcp"
            },
            {
                "port": 5678,
                "protocol": "udp"
            }
        ]
    }
```

## Settings

The Networking plugin uses external interfaces defined in the Dobby settings file (default location `/etc/sky/dobby.json`) to create iptables rules and enable port forwarding on the interfaces.

To set up external interfaces for the Networking plugin, add the following to the settings file:

```json
    "network": {
      "externalInterfaces": [ "eth0", "wlan0" ]
    }
```

The first external interface listed will be used for creating virtual ethernet devices for containers to use internally.

## Troubleshooting

### Bridge creation issues (libnl v3.3.x - 3.4.x)

With some libnl versions, creating a bridge interface causes issues (see `BridgeInterface.cpp`). To fix this, uncomment the line in `./CMakeLists.txt` to add `ENABLE_LIBNL_BRIDGE_WORKAROUND` compile definition to use workaround functions when using libnl v3.3.x - 3.4.x.

The issue has been fixed in versions 3.5.0 and onwards.

### Failed to resolve host (IPv6 only)

If the container is configured only to IPv6, i.e. `"ipv6": true, "ipv4": false`, the host machine needs to be able to resolve host addresses with IPv6 only.

On some machines, there is no IPv6 DNS support and to resolve host names, IPv4 may need to be enabled as well.

### No route to host (IPv6)

On some machines, it may take a couple of seconds for IPv6 routing to be set up after launching a container, causing routing failures if attempting to send packets via IPv6 for the first couple of seconds.

A delay of ~2 seconds may be needed before attempting to connect anything outside the container via the IPv6 protocol.