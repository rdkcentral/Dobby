# Dobby RDK GPU Memory Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to set gpu memory limits for your container. This will create a gpu cgroup for the container and set limits

```json
{
    "rdkPlugins": {
        "gpu": {
            "required": true,
            "data": {
                "memory": 1024564
            }
        }
    }
}
```

If you already have other RDK plugins in the bundle, then just add the gpu plugin. Do not create multiple `rdkPlugin` sections.

## Prerequisites

The GPU plugin assumes that a custom `gpu` cgroup is in place, with the graphics drivers supporting the behaviour of using at least `gpu.limit_in_bytes` to limit the gpu memory.

Requires a version of crun with [PR 609](https://github.com/containers/crun/pull/609) applied to ensure cgroup controllers are mounted correctly.