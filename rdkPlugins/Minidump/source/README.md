# Dobby RDK Minidump Plugin

## Quick Start
The main task of Minidump Plugin is to allow the collection of minidump core files from container bounds.

Minidump files are going to be copied from container bounds into `destinationPath` location (host namespace) once container
receives signal connected with core dump (e.g. SEGV, BUS). For further details please check: https://man7.org/linux/man-pages/man7/signal.7.html

Add the following section to your OCI runtime configuration `config.json` file to trigger mindump files lookup.

```json
{
    "rdkPlugins": {
        "minidump": {
            "required": true,
            "data": {
                "destinationPath": "/opt/minidumps"
            }
        }
    }
}
```

If you already have other RDK plugins in the bundle, then just add the minidump plugin. Do not create multiple `rdkPlugin` sections.

## Options
The options inside this object goes as follows:

| Option              | Value                                                                                                                                   |
| ------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| `destinationPath`   | Directory (host namespace) to which minidump file should be copied                                                                      |
|---------------------| ----------------Below this point there are optionals things, with default value in square brackets "[]"---------------------------------|
| `size`              | Size of the image file (in bytes), only valid if image wasn't there before [10485760] (10 MB)                                           |

#### Example
```json
"data": {
    "destinationPath": "/minidumps",
    "size": 12582912
}
```
