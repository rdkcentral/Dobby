# Dobby RDK RtSchedulingPlugin

## Quick Start

Add the following section to your OCI runtime configuration `config.json` file to set a real time scheduling priority for the container.

```json
{
    "rdkPlugins": {
        "rtscheduling": {
            "required": true,
            "data": {
                "default": 4,
                "limit": 6
            }
        }
    }
}
```
If you already have other RDK plugins in the bundle, then just add the rtscheduling plugin. Do not create multiple `rdkPlugin` sections.

## Options

The data field contains two optional fields. Neither of them is mandatory. If not supplied, both `default` and `limit` have a default value of 6.

Both `default` and `limit` priorities are only valid in the range 1 - 99 inclusive. The `RtScheduling` plugin will bound any value to that range before applying to a container.

For obvious reasons the default value must be less than or equal to the limit value.

### Default

The default value is the priority at which the init process of the container is run, since priority is inherited this also means that this is the priority at which any apps within the container will run at start-up.

```json
{
    "data": {
        "default": 4
    }
}
```

### Limit

The limit value defines the value to set for the hard and soft RLIMIT_RTPRIO limits, refer to the linux [sched man page](https://linux.die.net/man/2/sched_setscheduler) for more information on priority settings and privileges.

```json
{
    "data": {
        "limit": 6
    }
}
```