# Dobby Daemon Core

## Overview
The Dobby Daemon (`DobbyDaemon`) is the central process that manages OCI container lifecycle on RDK embedded Linux platforms. It exposes a D-Bus API for container control and delegates to `crun`/`runc` for actual OCI runtime operations.

## Components

### Dobby (Root Object)
- **File**: `daemon/lib/include/Dobby.h`, `daemon/lib/source/Dobby.cpp`
- Initializes the daemon environment, utilities, IPC, and manager
- Registers D-Bus method handlers on admin, control, and debug interfaces
- Runs the main work queue event loop
- Handles signal management (SIGTERM, SIGCHLD)
- Supports multiple log targets: Console, SysLog, EthanLog, Journald

### DobbyManager
- **File**: `daemon/lib/source/include/DobbyManager.h`, `daemon/lib/source/DobbyManager.cpp`
- Core container lifecycle management
- Operations: `startContainerFromSpec`, `startContainerFromBundle`, `stopContainer`, `pauseContainer`, `resumeContainer`, `hibernateContainer`, `wakeupContainer`, `addMount`, `removeMount`, `execInContainer`
- Maintains map of `ContainerId` → `DobbyContainer`
- Spawns a `runcMonitorThread` to detect child process exits (via `PR_SET_CHILD_SUBREAPER`)
- Invokes legacy plugin hooks (PostConstruction, PreStart, PostStart, PostStop, PreDestruction) and RDK plugin hooks (postInstallation, preCreation, postHalt)
- Supports `restartOnCrash` for automatic container restart
- Loads plugins from configurable `PLUGIN_PATH` (default: `/usr/lib/plugins/dobby`)

### DobbyContainer
- **File**: `daemon/lib/source/include/DobbyContainer.h`, `daemon/lib/source/DobbyContainer.cpp`
- Stores container state: bundle, config, rootfs, rdkPluginManager
- States: `Starting`, `Running`, `Stopping`, `Paused`, `Hibernating`, `Hibernated`, `Awakening`, `Unknown`
- Descriptors allocated via Fibonacci LFSR (range 1–1024) for pseudo-random, collision-avoiding assignment

### DobbyRunC
- **File**: `daemon/lib/source/include/DobbyRunC.h`, `daemon/lib/source/DobbyRunC.cpp`
- Wraps `crun` (RDK) or `runc` (non-RDK) command-line tool via `fork`/`exec`
- Operations: `create`, `start`, `run`, `destroy`, `killCont`, `pause`, `resume`, `exec`, `state`, `list`
- Working directory: `/var/run/rdk/crun`
- Log output: `/opt/logs/crun.log`

### DobbyWorkQueue
- **File**: `daemon/lib/source/include/DobbyWorkQueue.h`, `daemon/lib/source/DobbyWorkQueue.cpp`
- Serial work queue for processing container events on a single thread
- Supports `doWork` (synchronous, blocks caller until complete) and `postWork` (asynchronous)

### DobbyEnv
- **File**: `daemon/lib/source/include/DobbyEnv.h`, `daemon/lib/source/DobbyEnv.cpp`
- Stores platform environment: workspace path, flash mount path, plugins workspace, cgroup mount points, platform identifier
- Platform identifier read from `AI_PLATFORM_IDENT` environment variable

### DobbyLogger
- **File**: `daemon/lib/source/include/DobbyLogger.h`, `daemon/lib/source/DobbyLogger.cpp`
- Creates a Unix socket (`consoleSocket` from settings) for `crun` to connect to
- Monitors socket for new container TTY connections
- Supports syslog and journald log relaying via `DobbyLogRelay`

### DobbyLogRelay
- **File**: `daemon/lib/source/include/DobbyLogRelay.h`
- Relays datagrams between source and destination Unix sockets (syslog, journald)
- Integrates with `PollLoop` for event-driven I/O

### DobbyHibernate
- **File**: `daemon/lib/source/DobbyHibernate.h`, `daemon/lib/source/DobbyHibernate.cpp`
- Container hibernation via [memcr](https://github.com/LibertyGlobal/memcr) service
- Supports checkpoint/restore of container processes
- Compression algorithms: None, LZ4, Zstd
- Communicates with memcr via Unix or TCP socket

### DobbyAsync
- **File**: `daemon/lib/source/include/DobbyAsync.h`, `daemon/lib/source/DobbyAsync.cpp`
- Utility for spawning async work threads or deferring work
- `DobbyAsync`: executes function in new thread, `getResult()` joins
- `DobbyDeferred`: executes function lazily on `getResult()` call

## D-Bus Protocol
- **File**: `protocol/include/DobbyProtocol.h`
- Service: `org.rdk.dobby` (configurable via `DOBBY_SERVICE_OVERRIDE`)
- Object path: `/org/rdk/dobby` (configurable via `DOBBY_OBJECT_OVERRIDE`)
- **Admin interface** (`org.rdk.dobby.admin1`): Ping, Shutdown, SetLogMethod, SetLogLevel, SetAIDbusAddress
- **Control interface** (`org.rdk.dobby.ctrl1`): Start, StartFromSpec, StartFromBundle, Stop, Pause, Resume, Hibernate, Wakeup, Mount, Unmount, Exec, GetState, GetInfo, List, Annotate, RemoveAnnotation
- **Debug interface** (`org.rdk.dobby.debug1`): CreateBundle, GetSpec, GetOCIConfig, StartInProcessTracing, StopInProcessTracing
- **Events**: Started, Stopped, StoppedWithStatus, Hibernated, Awoken

## Daemon Entry Point
- **File**: `daemon/process/source/Main.cpp`
- Parses CLI args: `--settings-file`, `--dbus-address`, `--priority`, `--nofork`, `--noconsole`, `--syslog`, `--journald`
- Reads settings JSON, creates IPC service, instantiates `Dobby` root object
- Supports `SCHED_RR` real-time priority (default: 12 on non-RDK, disabled on RDK)
- Environment variables: `AI_WORKSPACE_PATH`, `AI_PERSISTENT_PATH`, `AI_PLATFORM_IDENT`

## Settings
- **File**: `daemon/process/settings/dobby.json`
- Default paths: workspace `/var/volatile/rdk`, persistent `/opt/persistent/rdk`
- Console socket: `/tmp/dobbyPty.sock`
- Network: external interfaces (`eth0`, `wlan0`), address range `100.64.11.0`

## DobbyInit
- **File**: `daemon/init/source/InitMain.cpp`
- Lightweight PID 1 init process for containers
- Reaps zombie/adopted child processes
- Forwards signals (except SIGCHLD) to child processes
- Installed at `/usr/libexec`
