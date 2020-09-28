# Dobby RDK HTTP Proxy Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to setup a http proxy in the container.

The HttpProxy plugin sets HTTP proxy environment variables and add additional root CA certificates to the container.

```json
{
    "rdkPlugins": {
        "httpproxy": {
            "required": true,
            "data": {
                "proxy": {
                    "host": "192.168.0.1",
                    "port": 8080
                },
                "ignoreProxy": [
                    "*.google.com",
                    "localhost"
                ],
                "ignoreProxyOnBridge": true,
                "proxyRootCACert": "-----BEGIN CERTIFICATE-----\nMIIFUjCCBDqgAwIBAgIGAXMRbOeHMA0GCSqGSIb3DQEBCwUAMIGtMT4wPAYDVQQDDDVDaGFybGVz\nIFByb3h5IENBICgyIEp1bCAyMDIwLCBCZW5zLU1hY0Jvb2stUHJvLmxvY2FsKTElMCMGA1UECwwc\naHR0cHM6Ly9jaGFybGVzcHJveHkuY2..."
            }
        }
    }
}
```
If you already have other RDK plugins in the bundle, then just add the networking plugin. Do not create multiple `rdkPlugin` sections.

## Options

### Proxy host address and port number

The given `proxy.host` and `proxy.port` data fields will be set to the `http_proxy` environment variable in the container.

This environment variable is used to point to the proxy server.

#### Example

```json
"data": {
    "proxy": {
        "host": "192.168.0.1",
        "port": 8080
    }
}
```

### Ignore domains

The domains given in the `ignoreProxy` data field are added to the `no_proxy` environment variable in the container.

The ignored domains are excluded from proxying.

#### Example
```json
"data": {
    "proxy": {
        "host": "192.168.0.1",
        "port": 8080
    },
    "ignoreProxy": [
        "*.google.com",
        "localhost"
    ]
}
```

### Ignore proxy on the Dobby bridge device

If `ignoreProxyOnBridge` is set to true, the dobby bridge device's address is added to the `no_proxy` environment variable in the container.

#### Example
```json
"data": {
    "proxy": {
        "host": "192.168.0.1",
        "port": 8080
    },
    "ignoreProxyOnBridge": true
}
```

### Additional root CA certificate

The `proxyRootCACert` field is optional. If it is included, the certificate is prepended to the host's root CA certificate in the container.

Use the full certificate, starting with `-----BEGIN CERTIFICATE-----` and ending with `-----END CERTIFICATE-----`.

Note that any newline control character will need to be replaced with `\n` in the certificate string.

#### Example
```json
"data": {
    "proxy": {
        "host": "192.168.0.1",
        "port": 8080
    },
    "proxyRootCACert": "-----BEGIN CERTIFICATE-----\nMIIFUjCCBDqgAwIBAgIGAXMRbOeHMA0GCSqGSIb3DQEBCwUAMIGtMT4wPAYDVQQDDDVDaGFybGVz\nIFByb3h5IENBICgyIEp1bCAyMDIwLCBCZW5zLU1hY0Jvb2stUHJvLmxvY2FsKTElMCMGA1UECwwc\naHR0cHM6Ly9jaGFybGVzcHJveHkuY2..."
}
```