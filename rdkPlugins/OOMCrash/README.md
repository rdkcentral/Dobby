# Dobby RDK OOMCrash Plugin

For the shared hook contract and plugin loading flow, see the [RDK plugin guide](../README.md) and [plugin launcher guide](../../pluginLauncher/README.md). This README remains the source for OOMCrash configuration.

## Quick Start
The OOMCrash Plugin detects when a container crashed due to Out of Memory and logs it. If a `path` is configured, it also creates an OOM crash file named `oom_crashed_<container_name>.txt` in that directory, and deletes the file when the container exits normally or no OOM is detected.

The plugin is enabled by default: it is listed in `defaultPlugins` in the Dobby settings file (`{ "oomcrash": {} }`), so it is injected into every bundle without bundle configuration. OOM detection and logging need no further setup.

To also enable the mount and crash file, add the following section to your OCI runtime configuration `config.json` file.

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
| `path`              | Optional. Directory (host namespace) to which oom crash file should be created                                                          |

If `path` is omitted or empty, OOM detection and logging still run, but the bind mount is not added and no crash file is created or removed.

Note : When set, the same `path` will be created inside the container namespace and will be mounted (read-only) together.
