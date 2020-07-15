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

Each container then gets a unique IPv4 address from an address pool range of 100.64.11.2 - 100.64.11.250.

```json
"data": {
    "type": "nat"
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

```json
    "data": {
        "dnsmasq": "true"
    }
```

### Holepuncher
TODO:

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

## Troubleshooting

### Bridge creation issues (libnl v3.3.x - 3.4.x)

With some libnl versions, creating a bridge interface causes issues (see `BridgeInterface.cpp`). To fix this, uncomment the line in `./CMakeLists.txt` to add `ENABLE_LIBNL_BRIDGE_WORKAROUND` compile definition to use workaround functions when using libnl v3.3.x - 3.4.x.

The issue has been fixed in versions 3.5.0 and onwards.