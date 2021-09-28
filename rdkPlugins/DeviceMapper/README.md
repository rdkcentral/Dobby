# Dobby RDK Device Mapper Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to enable logging to a file. This will capture all the stdout/err from the container (in addition to any output from `DobbyPluginLauncher`) and redirect it to the specified file.

```json
{
    "rdkPlugins": {
        "devicemapper": {
            "required": true,
            "data": {
                "devices": [
                    "/path/to/device",
                    "/path/to/other/device"
                ]
            }
        }
    }
}
```
If you already have other RDK plugins in the bundle, then just add the devicemapper plugin. Do not create multiple `rdkPlugin` sections.

## Options
### Devices
This is a list of the devices that are known to change dynamically (e.g. where the device major/minor number is not fixed and can change between boots).

When the container starts, this plugin will check the major/minor IDs in the config compared to the actual major/minor IDs of the device node and make adjustments to the config accordingly. Only devices specificed in the plugin `devices` array will be checked and potentially modified.

Where possible, try to ensure that device nodes are fixed and only use this plugin where necessary