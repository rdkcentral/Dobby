# Dobby RDK Gamepad Plugin

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