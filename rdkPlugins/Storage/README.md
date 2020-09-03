# Dobby RDK Storage Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to create loop mount. It will mount file from "source" field into container
space "destination".

```json
{
    "rdkPlugins": {
        "storage": {
            "required": true,
            "data": {
                "loopback": [
                    {
                        "destination": "/home/private",
                        "flags": 14,
                        "fstype": "ext4",
                        "source": "/tmp/data/data.img"
                    }
                ]
            }
        }
    }
}
```
If you already have other RDK plugins in the bundle, then just add the storage plugin. Do not create multiple `rdkPlugin` sections.

## Options
### Creating mount
For every mount point Storage plugin should create the should be one item in the array of "loopback". The options inside this object goes as follows:

| Option              | Value                                                                                                                                   |
| ------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| `destination`       | Directory (inside container space) in which mount should end up                                                                         |
| `source`            | Path to the image file which contains all data (if doesn't exist it will get created)                                                   |
| `flags`             | Mount flags, see linux documentation or "sys/mount.h" for details                                                                       |
|---------------------| ----------------Below this point there are optionals things, with default value in square brackets "[]"---------------------------------|
| `fstype`            | File system type, i.e. "ext4" ["ext4"]                                                                                                  |
| `options`           | Mount options, this corresponds to mount "data" field. []                                                                               |
| `persistent`        | If true image will be persistent between boots, if not image will be deleted after container destruction [true]                         |
| `imgsize`           | Size of the image file (in bytes), only valid if image wasn't there before [12582912] (12 MB)                                           |


#### Example
```json
"data": {
    "loopback": [
        {
            "destination": "/home/",
            "flags": 2,
            "fstype": "ext4",
            "source": "/tmp/data/data.img",
            "persistent": false,
            "imgsize": 10485760
        }
    ]
}
```

