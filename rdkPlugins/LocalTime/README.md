# Dobby RDK LocalTime Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to automatically symlink the real `/etc/localtime` file to the container's rootfs at `/etc/localtime`.

```json
{
    "rdkPlugins": {
        "localtime": {
            "required": true,
            "data": {
                "setTZ": "<path>"
            }
        }
    }
}
```

If you already have other RDK plugins in the bundle, then just add the localtime plugin. Do not create multiple `rdkPlugin` sections.

## Prerequisites

`/etc/localtime` symlink must be available and point to the correct file based on locale.

## Options
### setTZ
Optional parameter, if set it should contain a path to file holding time stamp. This time stamp will be placed in containers env variable called TZ