# Dobby RDK IPC Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to enable dbus inside the container.

```json
{
    "rdkPlugins": {
        "ipc": {
            "data": {
                "session" : "/path/to/dbus/session"
            }
        }
    }
}
```
If you already have other RDK plugins in the bundle, then just add the ipc plugin. Do not create multiple `rdkPlugin` sections.

## Options
### Possible connections
To create dbus connection, set the following options in the `data` section of the plugin configuration (as shown in the Quick Start section):

| Option              | Value                                                                                                                                   |
| ------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| `system`            | This will create access to system wide bus, inside container one can use "DBUS_SYSTEM_BUS_ADDRESS" to access it                         |
| `session`           | This will create session bus, to use it inside container use "DBUS_SESSION_BUS_ADDRESS"                                                 |
| `debug`             | This will create debug bus, to use it inside container use "DBUS_DEBUG_BUS_ADDRESS"                                                     |

As the all dbus address are expected to be of the form 'unix:path=<path_to_socket>' our path provided in plugin config should be the part of it called "<path_to_socket>".
Because we would like to support some legacy code there are some valid arguments that are not paths. Those are: "system", "ai-private", "ai-public", but last two are deprecated and if possible they should not be used. Making assigment:
```json
"data": {
    "system" : "system"
}
```
will allow container to use default system dbus (without defining path to it).

### Debug option
Note that dbus itself doesn't have "debug" bus. There are only system and session bus. Debug bus is implemented as another session bus. This means container can use 2 different sessions at once - "session" will be defined as DBUS_SESSION_BUS_ADDRESS, and debug (another session bus) will be defined as DBUS_DEBUG_BUS_ADDRESS.

#### Example
```json
{
    "rdkPlugins": {
        "ipc": {
            "data": {
                "system" : "/var/run/dbus/system_bus_socket"
            }
        }
    }
}
```

## Warning
There is known problem with dbus handling user namespacing and how handshaking is supported by dbus. It usually occurs with message like this:
```
Failed to open connection to "system" message bus: Did not receive a reply. Possible causes include: the remote application did not send a reply, the message bus security policy blocked the reply, the reply timeout expired, or the network connection was broken.
```
There are 2 different ways to fix it:
1. You can disable user namespacing in your container. To do that in container config.json go to "linux->namespaces" and delete
    ```
    {
        "type": "user"
    }
    ```
2. Apply patch to the dbus. The patch is in this folder named "dbus_user_namespace_fix.txt". This require you to rebuild dbus yourself. IPC plugin sets environment variable "SKY_DBUS_DISABLE_UID_IN_EXTERNAL_AUTH" required by this patch by default. When using this aproach just make sure that your container uid/gid mapping is valid for dbus.
