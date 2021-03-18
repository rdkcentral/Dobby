# Dobby RDK ION Memory Limits Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to set gpu memory limits for your container. This will create a gpu cgroup for the container and set limits

```json
{
    "rdkPlugins": {
        "ionmemory": {
            "required": true,
            "data": {
                "defaultLimit": 0,
                "heaps": [
                    {
                        "name": "System",
                        "limit": 64345345
                    }
                ]
            }
        }
    }
}
```

If you already have other RDK plugins in the bundle, then just add the ionmemory plugin. Do not create multiple `rdkPlugin` sections.

## Prerequisites
ION is a large memory allocator driver that is used on Android and other platforms to allocate big chunks of memory for media and graphics buffers. It divides memory into pools / heaps and allows both kernel drivers and userspace apps to allocate memory buffers.  The buffers can then be passed around and mapped using file descriptors.  More information on ION is available [here](https://lwn.net/Articles/480055/).

The ION plugin assumes that a custom `ion` cgroup is in place, with the kernel supporting the behaviour of using at least `ion.<HEAP>.limit_in_bytes` to limit the memory allocated from the given heap.

Requires a version of crun with [PR 609](https://github.com/containers/crun/pull/609) applied to ensure cgroup controllers are mounted correctly.

## Options

### Default Limit [Required]
This limit (in bytes) applies to all heaps by default. If a specific heap limit is not provided, the heap limit is set to this value.

### Heaps
An array of heaps with their name and the memory limit (in bytes) for that specific heap. This overrides the default limit for that heap.