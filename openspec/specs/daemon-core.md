# Dobby Daemon Core

## Overview
The Dobby Daemon (`DobbyDaemon`) is the central process that manages OCI container lifecycle on RDK embedded Linux platforms. It exposes a D-Bus API for container control and delegates to `crun`/`runc` for actual OCI runtime operations.

## Description
The daemon is the heart of Dobby, orchestrating container creation, start, stop, pause, resume, hibernate, and wakeup operations. The `Dobby` root object initializes the environment and registers D-Bus handlers. `DobbyManager` implements core lifecycle logic, maintaining a map of active containers and spawning monitor threads. `DobbyRunC` wraps the OCI runtime CLI. Supporting classes handle work queuing (`DobbyWorkQueue`), environment setup (`DobbyEnv`), logging (`DobbyLogger`, `DobbyLogRelay`), hibernation (`DobbyHibernate`), and async execution (`DobbyAsync`).

### Dobby (Root Object)
- Initializes the daemon environment, utilities, IPC, and manager
- Registers D-Bus method handlers on admin, control, and debug interfaces
- Runs the main work queue event loop
- Handles signal management (SIGTERM, SIGCHLD)
- Supports multiple log targets: Console, SysLog, EthanLog, Journald

### DobbyManager
- Core container lifecycle management
- Operations: `startContainerFromSpec`, `startContainerFromBundle`, `stopContainer`, `pauseContainer`, `resumeContainer`, `hibernateContainer`, `wakeupContainer`, `addMount`, `removeMount`, `execInContainer`
- Maintains map of `ContainerId` → `DobbyContainer`
- Spawns a `runcMonitorThread` to detect child process exits (via `PR_SET_CHILD_SUBREAPER`)
- Invokes legacy plugin hooks (PostConstruction, PreStart, PostStart, PostStop, PreDestruction) and RDK plugin hooks (postInstallation, preCreation, postHalt)
- Supports `restartOnCrash` for automatic container restart
- Loads plugins from configurable `PLUGIN_PATH` (default: `/usr/lib/plugins/dobby`)

### DobbyContainer
- Stores container state: bundle, config, rootfs, rdkPluginManager
- States: `Starting`, `Running`, `Stopping`, `Paused`, `Hibernating`, `Hibernated`, `Awakening`, `Unknown`
- Descriptors allocated via Fibonacci LFSR (range 1–1024) for pseudo-random, collision-avoiding assignment

### DobbyRunC
- Wraps `crun` (RDK) or `runc` (non-RDK) command-line tool via `fork`/`exec`
- Operations: `create`, `start`, `run`, `destroy`, `killCont`, `pause`, `resume`, `exec`, `state`, `list`
- Working directory: `/var/run/rdk/crun`
- Log output: `/opt/logs/crun.log`

### DobbyWorkQueue
- Serial work queue for processing container events on a single thread
- Supports `doWork` (synchronous, blocks caller until complete) and `postWork` (asynchronous)

### DobbyEnv
- Stores platform environment: workspace path, flash mount path, plugins workspace, cgroup mount points, platform identifier
- Platform identifier read from `AI_PLATFORM_IDENT` environment variable

### DobbyLogger
- Creates a Unix socket (`consoleSocket` from settings) for `crun` to connect to
- Monitors socket for new container TTY connections
- Supports syslog and journald log relaying via `DobbyLogRelay`

### DobbyLogRelay
- Relays datagrams between source and destination Unix sockets (syslog, journald)
- Integrates with `PollLoop` for event-driven I/O

### DobbyHibernate
- Container hibernation via [memcr](https://github.com/LibertyGlobal/memcr) service
- Supports checkpoint/restore of container processes
- Compression algorithms: None, LZ4, Zstd
- Communicates with memcr via Unix or TCP socket

### DobbyAsync
- Utility for spawning async work threads or deferring work
- `DobbyAsync`: executes function in new thread, `getResult()` joins
- `DobbyDeferred`: executes function lazily on `getResult()` call

### D-Bus Protocol
- **File**: `protocol/include/DobbyProtocol.h`
- Service: `org.rdk.dobby` (configurable via `DOBBY_SERVICE_OVERRIDE`)
- Object path: `/org/rdk/dobby` (configurable via `DOBBY_OBJECT_OVERRIDE`)
- **Admin interface** (`org.rdk.dobby.admin1`): Ping, Shutdown, SetLogMethod, SetLogLevel, SetAIDbusAddress
- **Control interface** (`org.rdk.dobby.ctrl1`): Start, StartFromSpec, StartFromBundle, Stop, Pause, Resume, Hibernate, Wakeup, Mount, Unmount, Exec, GetState, GetInfo, List, Annotate, RemoveAnnotation
- **Debug interface** (`org.rdk.dobby.debug1`): CreateBundle, GetSpec, GetOCIConfig, StartInProcessTracing, StopInProcessTracing
- **Events**: Started, Stopped, StoppedWithStatus, Hibernated, Awoken

### Daemon Entry Point
- Parses CLI args: `--settings-file`, `--dbus-address`, `--priority`, `--nofork`, `--noconsole`, `--syslog`, `--journald`
- Reads settings JSON, creates IPC service, instantiates `Dobby` root object
- Supports `SCHED_RR` real-time priority (default: 12 on non-RDK, disabled on RDK)
- Environment variables: `AI_WORKSPACE_PATH`, `AI_PERSISTENT_PATH`, `AI_PLATFORM_IDENT`

### DobbyInit
- Lightweight PID 1 init process for containers
- Reaps zombie/adopted child processes
- Forwards signals (except SIGCHLD) to child processes
- Installed at `/usr/libexec`

## Requirements
- A valid settings JSON file must be available at startup.
- D-Bus must be running and accessible.
- `crun` >= 0.13 (or `runc`) must be installed and in PATH.
- The workspace directory must be writable.
- Plugin shared libraries must be present at `PLUGIN_PATH`.

## Architecture / Design
- The `Dobby` root object owns all subsystems and runs a serial work queue event loop.
- `DobbyManager` is the central orchestrator, delegating to `DobbyRunC` for OCI operations and plugin managers for hook execution.
- Container state is encapsulated in `DobbyContainer` objects keyed by `ContainerId`.
- A child-subreaper thread monitors container process exits for lifecycle transitions.
- D-Bus provides the external API surface (admin, control, debug interfaces).

## External Interfaces
- **D-Bus API**: Three interfaces (admin, control, debug) exposed on `org.rdk.dobby` service.
- **OCI Runtime CLI**: `crun`/`runc` invoked via fork/exec for container operations.
- **Settings JSON**: Runtime configuration loaded at startup.
- **Console Socket**: Unix socket for container TTY log output.

## Performance
- Serial work queue avoids lock contention for container operations.
- `SCHED_RR` real-time priority available for time-critical operations.
- Fibonacci LFSR descriptor allocation provides O(1) container handle generation.

## Security
- Container isolation via OCI namespaces, cgroups, and seccomp profiles.
- D-Bus policy controls access to admin/control/debug interfaces.
- Signal handling restricted to SIGTERM and SIGCHLD.

## Versioning & Compatibility
_Not applicable — daemon versioning follows the overall Dobby project version._

## Conformance Testing & Validation
- L1 unit tests in `tests/L1_testing/`
- L2 integration tests in `tests/L2_testing/`

## Covered Code
- daemon/lib/include/Dobby.h
- daemon/lib/include/IDobbyPlugin.h
- daemon/lib/include/IDobbyStartState.h
- daemon/lib/source/Dobby.cpp
- daemon/lib/source/DobbyManager.cpp
- daemon/lib/source/DobbyContainer.cpp
- daemon/lib/source/DobbyRunC.cpp
- daemon/lib/source/DobbyWorkQueue.cpp
- daemon/lib/source/DobbyEnv.cpp
- daemon/lib/source/DobbyLogger.cpp
- daemon/lib/source/DobbyLogRelay.cpp
- daemon/lib/source/DobbyHibernate.h
- daemon/lib/source/DobbyHibernate.cpp
- daemon/lib/source/DobbyAsync.cpp
- daemon/lib/source/DobbyStartState.cpp
- daemon/lib/source/DobbyLegacyPluginManager.cpp
- daemon/lib/source/include/DobbyManager.h
- daemon/lib/source/include/DobbyContainer.h
- daemon/lib/source/include/DobbyRunC.h
- daemon/lib/source/include/DobbyWorkQueue.h
- daemon/lib/source/include/DobbyEnv.h
- daemon/lib/source/include/DobbyLogger.h
- daemon/lib/source/include/DobbyLogRelay.h
- daemon/lib/source/include/DobbyAsync.h
- daemon/lib/source/include/DobbyStartState.h
- daemon/lib/source/include/DobbyLegacyPluginManager.h
- daemon/process/source/Main.cpp
- daemon/init/source/InitMain.cpp
- protocol/include/DobbyProtocol.h

---

## Open Queries
_No open queries._

## References
- [OCI Runtime Specification](https://github.com/opencontainers/runtime-spec)
- [memcr - Memory Checkpoint and Restore](https://github.com/LibertyGlobal/memcr)

## Change History
- 2025-05-18 - openspec-templater - Restructured to match spec template.
