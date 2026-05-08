# Dobby - OCI Container Management Daemon

## Overview
Dobby ("Docker based Thingy") is a tool for managing and running OCI containers using [crun](https://github.com/containers/crun). It provides a daemon-based architecture for container lifecycle management, plugin-based extensibility, and dbus-based communication.

## Description
Dobby is a container management system designed for RDK (Reference Design Kit) platforms. It consists of a central daemon (`DobbyDaemon`) that manages container creation, execution, pausing, hibernation, and destruction. Containers are defined using OCI bundle specifications, and Dobby extends the standard OCI model with an RDK plugin system for additional functionality such as networking, logging, storage, IPC, and more.

Key components include:
- **DobbyDaemon** — The main background service responsible for managing, controlling, and creating containers. Communicates over dbus (admin, debugging, and command interfaces).
- **DobbyBundleGenerator** — CLI wrapper around `DobbyBundleGen` library for converting Dobby spec JSON files to extended OCI bundles with RDK plugin sections.
- **DobbyTool** — CLI debugging tool that connects to the daemon over dbus for testing and troubleshooting containers.
- **Plugin System** — Extensible plugin architecture with RDK plugins (Logging, Networking, IPC, Storage, Minidump, etc.) and legacy plugin support.
- **libocispec** — Submodule for generating C headers to parse and manipulate OCI bundle specifications.

## Requirements
- CMake (>3.7)
- crun (>=0.13)
- jsoncpp
- yajl 2 (for libocispec)
- ctemplate (if using `LEGACY_COMPONENTS`)
- libsystemd
- libnl (if using Networking plugin)
- libnl-route (if using Networking plugin)
- libdbus
- boost (1.61)

## Architecture / Design
### Build System
Dobby is a CMake project with extensive configuration options:

| Option | Description |
|:---|:---|
| `-DCMAKE_BUILD_TYPE` | Debug/Release build |
| `-DENABLE_DOBBYTOOL` | Include DobbyTool (Debug=ON, Release=OFF) |
| `-DPLUGIN_PATH` | Custom RDK plugin load path (default: `/usr/lib/plugins/dobby`) |
| `-DRDK_PLATFORM` | Platform target: `GENERIC` or `DEV_VM` |
| `-DSETTINGS_FILE` | Path to settings JSON (installed to `/etc/dobby.json`) |
| `-DLEGACY_COMPONENTS` | Enable legacy plugins, Dobby specs (default: OFF) |
| `-DENABLE_LTO` | Link Time Optimisation (default: OFF) |
| `-DENABLE_PERFETTO_TRACING` | Perfetto tracing support (non-release only) |
| `-DDOBBY_SERVICE` | Dbus service name (default: `org.rdk.dobby`) |
| `-DDDOBBY_OBJECT` | Dbus object path (default: `/org/rdk/dobby`) |
| `-DUSE_STARTCONTAINER_HOOK` | Use startcontainer OCI hook (default: OFF) |
| `-DUSE_SYSTEMD` | Systemd support via sd-bus (default: ON) |
| `-DDOBBY_HIBERNATE_MEMCR_IMPL` | Hibernate via memcr service (default: OFF) |

### Plugin Architecture
Each RDK plugin can be enabled/disabled at build time with `-DPLUGIN_[PLUGINNAME]=[ON|OFF]`. Default enabled plugins: Logging, Networking, IPC, Storage, Minidump.

### Key Component Interactions
- DobbyDaemon listens on dbus addresses for admin, debug, and command channels.
- DobbyManager orchestrates container lifecycle via crun.
- DobbyRdkPluginManager loads and executes plugins at container hook points.
- DobbyBundleGenerator converts Dobby JSON specs to OCI bundles offline.

## External Interfaces
### Dbus Interface
- **Service name**: `org.rdk.dobby` (configurable)
- **Object path**: `/org/rdk/dobby` (configurable)
- **Addresses**: Admin bus, Debug bus, Command bus

### DobbyDaemon CLI
```
DobbyDaemon [--nofork] [--verbose] [--settings-file=PATH] [--dbus-address=ADDRESS] [--priority=PRIORITY]
```

### DobbyBundleGenerator CLI
```
DobbyBundleGenerator --settings=PATH --inputpath=PATH --outputDirectory=PATH
```

### DobbyTool CLI
Commands: `start`, `stop`, `pause`, `resume`, `hibernate`, `wakeup`, `mount`, `unmount`, `exec`, `list`, `info`, `wait`, `set-log-level`, `set-dbus`

### Environment Variables
- `AI_WORKSPACE_PATH` — Path to tmpfs workspace directory
- `AI_PERSISTENT_PATH` — Path to persistent directory across boots
- `AI_PLATFORM_IDENT` — 4-character STB platform identifier

## Performance
_Not applicable — No specific performance metrics defined in current documentation._

## Security
_Not applicable — No explicit security requirements defined in current documentation._

## Versioning & Compatibility
- Dobby uses GitHub releases for versioning.
- OCI bundle schema changes in `bundle/runtime-schemas` require CMake reconfiguration to regenerate headers.
- Legacy components can be toggled via `LEGACY_COMPONENTS` build flag for backward compatibility.

## Conformance Testing & Validation
- L1 tests located in `tests/L1_testing/`
- L2 tests located in `tests/L2_testing/`
- Development environment available via Vagrant VM in `develop/vagrant/`

## Covered Code
- daemon/lib/source/Dobby.cpp:
    - Dobby (main daemon class)
- daemon/lib/source/DobbyManager.cpp:
    - DobbyManager (container lifecycle management)
- daemon/lib/source/DobbyContainer.cpp:
    - DobbyContainer (container representation)
- daemon/lib/source/DobbyRunC.cpp:
    - DobbyRunC (crun interface)
- daemon/lib/source/DobbyEnv.cpp:
    - DobbyEnv (environment configuration)
- daemon/lib/source/DobbyStats.cpp:
    - DobbyStats (container statistics)
- daemon/lib/source/DobbyStartState.cpp:
    - DobbyStartState (container start state)
- daemon/lib/source/DobbyLogger.cpp:
    - DobbyLogger (logging subsystem)
- daemon/lib/source/DobbyLogRelay.cpp:
    - DobbyLogRelay (log relay)
- daemon/lib/source/DobbyStream.cpp:
    - DobbyStream (stream handling)
- daemon/lib/source/DobbyHibernate.cpp:
    - DobbyHibernate (hibernation support)
- daemon/lib/source/DobbyAsync.cpp:
    - DobbyAsync (async operations)
- daemon/lib/source/DobbyWorkQueue.cpp:
    - DobbyWorkQueue (work queue)
- daemon/process/source/Main.cpp:
    - main (daemon entry point)
- client/lib/source/DobbyProxy.cpp:
    - DobbyProxy (client-side proxy)
- client/lib/source/DobbyFactory.cpp:
    - DobbyFactory (client factory)
- ipcUtils/source/DobbyIpcBus.cpp:
    - DobbyIpcBus (IPC bus)
- ipcUtils/source/DobbyIPCUtils.cpp:
    - DobbyIPCUtils (IPC utilities)
- pluginLauncher/lib/source/DobbyRdkPluginDependencySolver.cpp:
    - DobbyRdkPluginDependencySolver (plugin dependency resolution)
- settings/source/Settings.cpp:
    - Settings (configuration management)
- utils/include/DobbyUtils.h:
    - DobbyUtils (utility functions)
- utils/include/ContainerId.h:
    - ContainerId (container identification)
- pluginLauncher/lib/include/DobbyRdkPluginManager.h:
    - DobbyRdkPluginManager (plugin manager interface)
- pluginLauncher/lib/include/DobbyRdkPluginUtils.h:
    - DobbyRdkPluginUtils (plugin utilities)

---

## Open Queries
- What are the specific dbus API methods and their signatures exposed by DobbyDaemon?
- Are there defined performance benchmarks or SLAs for container startup/teardown times?
- What is the security model for dbus communication (authentication, authorization)?
- What is the detailed plugin lifecycle and hook execution order?
- What are the supported OCI runtime spec versions?

## References
- [RDKCentral Dobby Wiki](https://wiki.rdkcentral.com/display/ASP/Dobby)
- [Doxygen Documentation](https://rdkcentral.github.io/Dobby/)
- [crun](https://github.com/containers/crun)
- [memcr](https://github.com/LibertyGlobal/memcr)
- [Troubleshooting Guide](./Troubleshooting.md)
- [Contributing Guide](./CONTRIBUTING.md)

## Change History
- 2025-05-08 - openspec-templater - Restructured to match spec template.
