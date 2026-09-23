# Dobby RDK Gamepad Plugin

For the shared hook contract and plugin loading flow, see the [RDK plugin guide](../../docs/rdk-plugins.md) and [plugin launcher guide](../../docs/plugin-launcher.md). This README remains the source for gamepad configuration.

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to be able to read /dev/input/events created for gamepad device

```json
{
    "rdkPlugins": {
        "gamepad": {
            "required": false,
            "data": {}
        }
    }
}
```

If you already have other RDK plugins in the bundle, then just add the gamepad plugin. Do not create multiple `rdkPlugin` sections.