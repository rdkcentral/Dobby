# Dobby RDK LocalTime Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to automatically symlink the real `/etc/localtime` file to the container's rootfs at `/etc/localtime`.

```json
{
    "rdkPlugins": {
        "gpu": {
            "required": true,
            "data": {}
        }
    }
}
```

**Note:** This plugin takes no data, so the `data` field can be left empty.

If you already have other RDK plugins in the bundle, then just add the localtime plugin. Do not create multiple `rdkPlugin` sections.

## Prerequisites

`/etc/localtime` symlink must be available and point to the correct file based on locale.
