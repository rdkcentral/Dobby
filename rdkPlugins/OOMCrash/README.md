# Dobby RDK OOMCrash Plugin

## Quick Start
The main task of OOMCrash Plugin is to create a OOM crash file named `oom_crashed_<container_name>.txt` on the configured path when a container is crashed due to Out of Memory and also delete that file when the container exits normally or if OOM not detected.

Add the following section to your OCI runtime configuration `config.json` file to trigger oomcrash files lookup.

```json
{
    "rdkPlugins": {
        "oomcrash": {
            "required": true,
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

Note : The same `path` will be created inside the container namespace and will be mounted together.
