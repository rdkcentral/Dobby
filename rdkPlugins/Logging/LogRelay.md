# Log Relay Support
Dobby has a feature called Log Relay, which is designed to forward messages sent to syslog or journald from inside the container to the appropriate sockets on the host

## The need for a Log Relay
If an application calls `sd_journal_print()` to directly log a message to journald, that message is sent down the `/run/systemd/journal/socket` socket to the journald daemon. Whilst it is possible to mount that socket directly into the container, there is an issue with filtering the logs.

When `/run/systemd/journal/socket` is directly mounted into the container, the log message arrives to journald, but it is not tagged with a `_SYSTEMD_UNIT` label.

This means when filtering logs with `journalctl -fu dobby`, the log messages do not appear as they are not part of the dobby unit. Messages captured by the Logging plugin (or EthanLog on Sky builds) are tagged with the Dobby unit, and the DumpLogs script on RDK makes use of the unit to filter and sort log files.

## Solution
To enable the Dobby log relay, add the following to the settings file at /etc/dobby.json

```json
    "logRelay": {
      "syslog": {
        "enable": true,
        "socketPath": "/tmp/dobby-syslog"
      },
      "journald": {
        "enable": true,
        "socketPath": "/tmp/dobby-journald"
      }
    }
```

Then add bind mounts for the dobby sockets into the container. E.G
```json
        {
            "source": "/tmp/dobby-syslog",
            "destination": "/dev/log",
            "options": [
                "rbind",
                "rprivate",
                "nosuid",
                "nodev"
            ],
            "type": "bind"
        },
        {
            "source": "/tmp/dobby-journald",
            "destination": "/run/systemd/journal/socket",
            "options": [
                "rbind",
                "rprivate",
                "nosuid",
                "nodev"
            ],
            "type": "bind"
        }
```

Log messages will now appear when doing `journalctl -fu dobby`