- [Preface](#preface)
- [Log Files](#log-files)
  - [Dobby Logs](#dobby-logs)
  - [Crun log](#crun-log)
  - [Container log](#container-log)
- [Common Issues](#common-issues)
  - [Veth devices](#veth-devices)
  - [CGroups](#cgroups)
    - [Raspberry Pi](#raspberry-pi)
  - [Networking](#networking)
  - [Permissions](#permissions)
  - ["container is not in a 'stopped' state" Error Message](#container-is-not-in-a-stopped-state-error-message)
- [Running commands inside the container](#running-commands-inside-the-container)

# Preface
There are a number of things that could go wrong when running containers. This document explains some common issues encountered when starting containers, and where to look for logs to help troubleshoot

# Log Files
If a container does not start, there are a few important log files to look at.

## Dobby Logs
Dobby produces its own log file. When running as a systemd service, Dobby's logs can be viewed by running
```
journalctl -u dobby
```
By default, Dobby will only log `Warning` events or higher. To increase the verbostiy of Dobby's logs, edit the systemd service file (typically at `/lib/systemd/system/dobby.service`) and add `-v` to the command to increase logging level. Verbosity can be increased multiple times (`-vv`) for more detailed logs.

## Crun log
Any issues with crun may be reported in crun's own log file, which is located at `/var/log/crun.log`

## Container log
If the issue preventing container startup is inside one of the OCI hooks, then the output from the hooks will be saved to the container log file, assuming the container is using the Dobby logging plugin.

If the container is not using the logging plugin, add the following to your OCI config as per the Logging [README](./rdkPlugins/Logging/README.md) and restart the container
```json
{
    "rdkPlugins": {
        "logging": {
            "required": true,
            "data": {
                "sink": "file",
                "fileOptions": {
                    "path": "/tmp/container.log"
                }
            }
        }
    }
}
```
You can then look at the `/tmp/container.log` file to see exactly what went wrong during startup. The verbosity of the hook output is directly tied to the verbosity of the daemon, so to see more detailed logs for the hooks, increase the daemon log level.

# Common Issues
## Veth devices
To support any networking mode other than `open`, the platform's kernel must have support for virtual ethernet devices (veths). To enable it, set the kernel config option `CONFIG_VETH` to `y` - see https://cateee.net/lkddb/web-lkddb/VETH.html.

## CGroups
For containers to work correctly, the platform must have the appropriate cgroups enabled.

To see which cgroups are enabled, run
```
cat /proc/cgroups
```

The output should look similar to the below, where all cgroups are enabled
```
# cat /proc/cgroups
#subsys_name	hierarchy	num_cgroups	enabled
cpuset          8           1           1
cpu             4           1           1
cpuacct         4           1           1
blkio           5           1           1
memory          2           3           1
devices         6           72          1
freezer         3           1           1
net_cls         7           1           1
```

### Raspberry Pi
On the Raspberry Pi, the memory cgroup may be disabled. To enable it, in
```
meta-raspberrypi/recipes-kernel/linux/linux-raspberrypi.inc
```
add the following
```
CMDLINE_append = "cgroup_enable=cpuset cgroup_enable=memory cgroup_memory=1"
```

## Networking
There are known issues with the Networking plugin. See the Networking [README](./rdkPlugins/Networking/README.md) file for more details

## Permissions
When user namespacing is enabled, it is important the permissions on the bundle are set correctly. User namespacing will map users/groups of given IDs into the container as different IDs (e.g. ID 1000 on the host could map to ID 0 in the container). As a result, the permissions on the bundle must be set to the correct ID. More info on user namespacing [here](https://www.man7.org/linux/man-pages/man7/user_namespaces.7.html).

The UID/GID map is defined in the OCI runtime config:
```json
"uidMappings": [
    {
        "containerID": 0,
        "hostID": 1000,
        "size": 1
    }
],
"gidMappings": [
    {
        "containerID": 0,
        "hostID": 1000,
        "size": 1
    }
]
```
Here, the bundle should be owned by 1000:1000 on the host, so that the user inside the container has the correct permissions to read/write/execute files in the container rootfs.

To disable user namespacing for a container:

* If using BundleGen to create the bundle...
  * Set `disableUserNamespacing: true` in the platform template and regenerate the bundle
* If using pre-made bundles:
  * Remove the `user` namespace type from the `namespaces` section of the config
  * Remove the `uidMappings` and `gidMappings` sections

## "container is not in a 'stopped' state" Error Message
Sometimes, if a container crashes, when restarting the container Dobby will produce the following error:

`container is not in the 'stopped' state`

The root cause is still being investigated, but it is often possible to fix with one of the following methods

* Delete the contents of the following directories:
  * `/run/rdk/crun/`
  * `/var/run/rdk/crun`

* Delete the OCI bundle and re-install the container


# Running commands inside the container
If the container is starting but not behaving as expected, it can often be useful to run commands inside the container. The easiest way to do this is with `DobbyTool exec` (assuming `DobbyTool` is installed on the STB).

The `exec` command accepts two parameters - the id of the running container and the command(s) to run inside the container. For example:

```
$ DobbyTool start sleep sleepy-thunder_bundle
started 'sleep' container, descriptor is 734

$ DobbyTool exec sleep ls
executed command in 'sleep' container, descriptor is 734
```
To view the output of the command, view the container logfile - as configured in the Logging plugin.

Note the command to be executed in the container must be accessible from within the container - any paths provided in the command will resolve within the container's rootfs.