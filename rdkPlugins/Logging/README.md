# Dobby RDK Logging Plugin

## Quick Start
Add the following section to your OCI runtime configuration `config.json` file to enable logging to a file. This will capture all the stdout/err from the container (in addition to any output from `DobbyPluginLauncher`) and redirect it to the specified file.

```json
{
    "rdkPlugins": {
        "logging": {
            "required": true,
            "data": {
                "sink": "file",
                "fileOptions": {
                    "path": "/tmp/container.log",
                    "limit": 65536
                }
            }
        }
    }
}
```
If you already have other RDK plugins in the bundle, then just add the logging plugin. Do not create multiple `rdkPlugin` sections.

## Options
### File Logging
To send container logs to a file, set the following options in the `data` section of the plugin configuration (as shown in the Quick Start section):

| Option              | Value                                                                                                                                   |
| ------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| `sink`              | `file`                                                                                                                                  |
| `fileOptions.path`  | Path to save config file. File will be created if it doesn't already exist. Each time the container is run, the file will be overridden |
| `fileOptions.limit` | Maximum size of the file in bytes. Note this is only a guide, and the file may exceed this size by a small amount                       |

If no path is provided or the file cannot be created, the plugin will send logs to /dev/null (this will be reported as a warning in the daemon logs)

#### Example
```json
"data": {
    "sink": "file",
    "fileOptions": {
        "path": "/tmp/container.log",
        "limit": 65536
    }
}
```


### Journald Logging
To send the container output to journald, set the following options:

| Option                     | Value                                                                                                                                                                                                                                      |
| -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `sink`                     | `journald`                                                                                                                                                                                                                                 |
| `journaldOptions.priority` | Priority level (severity) for container log messages. Value should be a string. Note it is not possible to distinguish between stdout and stderr output.

Each container is identified in journald by its hostname, which is set by Dobby to the container ID when first starting the contianer.

Note that the PID shown next to each container message in `journalctl -f` will be the PID of the DobbyDaemon, not the PID of the container. The container PID can be retrieved from the `OBJECT_PID` field of the message:

```json
{
	...
	"SYSLOG_IDENTIFIER" : "log",
	"_COMM" : "DobbyDaemon",
	"_EXE" : "/usr/local/sbin/DobbyDaemon",
	"_CMDLINE" : "DobbyDaemon -v --nofork",
	"MESSAGE" : "Hello World 3",
	"CODE_LINE" : "277",
	"_PID" : "7371",
	"OBJECT_PID" : "7614",
	"SYSLOG_PID" : "7614",
    ...
}
```

#### Example
```json
"data": {
    "sink": "journald",
    "journaldOptions": {
        "priority": "LOG_INFO"
    }
}
```

#### Valid Priority Options
| Log Level   | Description                        |
|-------------|------------------------------------|
| LOG_EMERG   | system is unusable                 |
| LOG_ALERT   | action must be taken immediately   |
| LOG_CRIT    | critical conditions                |
| LOG_ERR     | error conditions                   |
| LOG_WARNING | warning conditions                 |
| LOG_NOTICE  | normal, but significant, condition |
| LOG_INFO    | informational message [DEFAULT]    |
| LOG_DEBUG   | debug-level message                |


### /dev/null
To send the container output to /dev/null (for example for production apps that don't require any logging), set the following options:

| Option | Value     |
| ------ | --------- |
| `sink` | `devNull` |

#### Example
```json
"data": {
    "sink": "devNull"
}
```

## Extending the Plugin
Unlike other Dobby RDK plugins, logging plugins are developed against the `IDobbyRdkLoggingPlugin` interface. They can still use all the normal OCI/Dobby hook points provided by `IDobbyRdkPlugin`, but gain a new public method called `LoggingLoop`:

```cpp
void LoggingLoop(ContainerInfo containerInfo,
                 const bool isBuffer,
                 const bool createNew)
```
This method is called by `DobbyLogger` when a container is started, and is run as a thread throughout the container's life.

In this method, you should read data from the (blocking) fd `containerInfo.pttyFd` until the fd is closed (i.e. `read(2)` returns < 0). If `isBuffer` is true, `pttyFd` will point to a memFd, which should be read until the end of the file, at which point the method should return. This allows Dobby to send small in-memory buffers of data to the logging plugin that will be logged immediately.

Note there can only be one logging plugin per container to prevent syncronisation issues reading from the same file descriptors.

See the comments in `LoggingPlugin.cpp` for more information.