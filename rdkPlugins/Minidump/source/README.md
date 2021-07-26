# Dobby RDK Minidump Plugin

## Quick Start
The main task of Minidump Plugin is to allow the collection of minidump core files from container bounds.

Please note that there are some prerequisites that must be met before this plugin becomes usable.

Minidump Plugin must be:
* compiled with breakpad library support,
* using the latest version of breakpad-wrapper library with `BREAKPAD_FD` support.

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

#### Example
```json
"data": {
    "destinationPath": "/minidumps",
}
```
