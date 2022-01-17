# Dobby RDK Multicast Sockets Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to setup multicast sockets in the container.

#### Example
```json
{
    "rdkPlugins": {
        "multicastsockets": {
            "required": true,
            "data": {
                "serverSockets": [
                    {
                        "name": "NAME",
                        "ip": "239.255.255.250",
                        "port": 1900
                    }
                ],
                "clientSockets": [
                    {
                        "name": "NAME1"
                    }
                ]
            }
        },
    }
}
```
If you already have other RDK plugins in the bundle, then just add the networking plugin. Do not create multiple `rdkPlugin` sections.

When enabled, the plugin will create the server sockets on the host, and pass the file descriptor of the socket into the container. Environment variables will then be set
to allow applications to retrieve these file descriptors:

```
MCAST_SERVER_SOCKET_<NAME>_FD=5
MCAST_CLIENT_SOCKET_<NAME>_FD=6
```

Used for Netflix MDX on certain devices.

## Options
### Server Sockets
Creates sockets on the host system and binds them to the multicast IP and port specified

### Client Sockets
Creates a client UDP socket