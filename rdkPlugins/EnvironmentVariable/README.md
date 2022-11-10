# Dobby RDK Environment Variable Plugin

## Quick Start
The main task of EnvironmentVariable Plugin is to inport hosts environment variable into container. If variable exists
during preCreation hook then its value will be copied into container. If environment variable is not present during
preCreation hook then it will be skipped.

NOTE: In case of value being changed in the host after preCreation hook, it will not be reflected inside container.

Add the following section to your OCI runtime configuration `config.json` file to trigger Environment Variable plugin.

```json
{
    "rdkPlugins": {
        "environmentvariable": {
            "required": false,
            "data": {
                "variables": [
                    "LANG",
                    "HOME"
                ]
            }
        }
    }
}
```

If you already have other RDK plugins in the bundle, then just add the environmentvariable plugin. Do not create multiple `rdkPlugin` sections.

## Options
The options inside this object goes as follows:

| Option              | Value                                                                                                         |
| ------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| `variables`         | Variable which should be copied from host into container                                                        |


