# Dobby RDK LocalTime Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to automatically symlink the mentioned path to the container's rootfs.

```json
{
    "rdkPlugins": {
        "localtime": {
            "required": true,
            "data": {
                "path": "<path>",
                "setTZ": "<path>"
            }
        }
    }
}
```

If you already have other RDK plugins in the bundle, then just add the localtime plugin. Do not create multiple `rdkPlugin` sections.

## Options
### path
Optional parameter, if set, localtime symlink path should be set, else the default path `etc/localtime` is set.

### setTZ
Optional parameter, if set it should contain a path to file holding time stamp. This time stamp will be placed in containers env variable called TZ
