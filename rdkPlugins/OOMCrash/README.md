# Dobby RDK OOMCrash Plugin

## Quick Start
The main task of OOMCrash Plugin is to create a OOM crash file on the configured path when a container is crashed due to Out of Memory.

Add the following section to your OCI runtime configuration `config.json` file to trigger oomcrash files lookup.

```json
{
    "rdkPlugins": {
        "minidump": {
            "required": false,
            "data": {
                "path": "/opt/dobby_container_crashes"
            }
        }
    }
}
```

If you already have other RDK plugins in the bundle, then just add the oomcrash plugin. Do not create multiple `rdkPlugin` sections.

## Options
The options inside this object goes as follows:

| Option              | Value                                                                                                                                   |
| ------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| `path`              | Directory (host namespace) to which oom crash file should be created                                                                    |

