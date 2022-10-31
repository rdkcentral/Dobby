# Dobby RDK Memory Checkpoint Restore Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to set specific mount points needed by your Memory Checkpoint and Restore tool for your container. This will bind all listed mounts within your container.

```json
{
    "memcheckpointrestore": {
        "required": false, 
        "data": {
            "mountpoints": [
                {
                    "source": "/media/apps/memcr", 
                    "destination": "/memcr", 
                    "type": "bind", 
                    "options": [
                        "bind", 
                        "nosuid", 
                        "nodev", 
                        "noexec"
                    ]
                }
            ]
         }
     }
}
```

If you already have other RDK plugins in the bundle, then just add the memcheckpointrestore plugin. Do not create multiple `rdkPlugin` sections.

## Prerequisites

None
