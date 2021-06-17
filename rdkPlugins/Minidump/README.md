# Dobby RDK Minidump Plugin

## Quick Start
The main task of Minidump Plugin is to allow the collection of minidump core files located in container namespace.

Minidump files (if any) are going to be copied from `containerSource` path (container namespace) into `hostDestination` path (host namespace) once container
halt event occured (e.g. SEGV). Please mind that Minidump Plugin is going mount file from `image` field into container space `containerSource` to establish
data stream between host and container namespaces.

Add the following section to your OCI runtime configuration `config.json` file to trigger mindump files lookup.

```json
{
    "rdkPlugins": {
        "minidump": {
            "required": true,
            "data": {
                "paths": [
                    {
                        "image": "/tmp/data/data.img",
                        "containerSource": "/opt/minidumps",
                        "hostDestination": "/opt/minidumps"
                    }
                ]
            }
        }
    }
}
```

If you already have other RDK plugins in the bundle, then just add the minidump plugin. Do not create multiple `rdkPlugin` sections.

## Options
### Creating mount
For every potential minidump file location Minidump Plugin should create one item in the array of `paths`. The options inside this object goes as follows:

| Option              | Value                                                                                                                                   |
| ------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| `image`             | Path to the image file which contains all data (if doesn't exist it will get created)                                                   |
| `containerSource`   | Directory (container namespace) in which minidump should be available                                                                   |
| `hostDestination`   | Directory (host namespace) to which minidump file should be copied                                                                      |
|---------------------| ----------------Below this point there are optionals things, with default value in square brackets "[]"---------------------------------|
| `imgsize`           | Size of the image file (in bytes), only valid if image wasn't there before [8388608] (8 MB)                                             |

#### Example
```json
"data": {
    "paths": [
        {
            "image": "/tmp/data/data.img",
            "containerSource": "/minidumps",
            "hostDestination": "/minidumps",
            "imgsize": 10485760
        }
    ]
}
```
