# Dobby RDK Thunder Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to enable Thunder access from within a container.

```json
{
    "rdkPlugins": {
        "thunder": {
            "required": true,
            "data": {}
        }
    }
}
```

Automatically does the following:
* Enables port forwarding from inside the container -> host on port 9998
* Sets `THUNDER_ACCESS` environment variable to `100.64.11.1:9998`
* Edits `/etc/hosts` and `/etc/services` file to add Thunder info

If you already have other RDK plugins in the bundle, then just add the Thunder plugin. Do not create multiple `rdkPlugin` sections.

## Options
### BearerUrl
If `bearerUrl` is set, the plugin will create a security token and set `THUNDER_SECURITY_TOKEN` inside the container to the token value.

The URL defines which roles the container will have based on the Thunder ACL file. If the SecurityAgent plugin is not running, then the plugin will skip the token generation.

```json
"data": {
    "bearerUrl": "http://localhost"
}
```

### Connection Limit
*Note: this feature is currently disabled - change `mEnableConnLimit` in `ThunderPlugin.cpp` to true to enable*

If `connLimit` is set, the amount of parallel connections to Thunder is limited. If `connLimit` is not present in the plugin data, then it will default to 32 connections.


```json
"data": {
    "connLimit": 32
}
```
