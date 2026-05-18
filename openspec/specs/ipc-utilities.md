# IPC & Utilities

## Overview
Dobby uses D-Bus for inter-process communication between the daemon, client tools, and plugins. The IPC infrastructure is provided by the `AppInfrastructure` libraries and the `ipcUtils` module. The `utils` module provides filesystem, cgroup, timer, and namespace utilities.

## Description
The IPC layer abstracts D-Bus communication via the `IIpcService` interface, with `DobbyIPCUtils` providing a plugin-facing facade for method invocation, signal emission, and service monitoring across multiple bus connections (System, AIPublic, AIPrivate). The utilities module (`DobbyUtils`) offers filesystem operations, loop device management, namespace entry, and timer services. `DobbyTimer` implements an efficient timer queue using `timerfd`. Supporting classes handle file permission fixes (`DobbyFileAccessFixer`) and container start state management (`DobbyStartState`).

## IPC Components

### IIpcService (Interface)
- **File**: `AppInfrastructure/IpcService/include/IIpcService.h`
- Abstract interface for D-Bus IPC operations
- Method invocation (sync and async), signal emission, signal handler registration
- Service availability monitoring

### IpcFactory
- **File**: `AppInfrastructure/IpcService/`
- Creates `IIpcService` instances connected to D-Bus addresses
- Supports system bus and custom bus addresses

### DobbyIPCUtils
- Implements `IDobbyIPCUtils` interface
- Manages three D-Bus connections: SystemBus, AIPublicBus, AIPrivateBus
- Provides method invocation, signal emission, service monitoring to plugins
- AI bus addresses can be set dynamically at runtime

### DobbyIpcBus
- Wraps a single `IIpcService` D-Bus connection
- Supports dynamic connect/disconnect with automatic handler re-registration
- Service change monitoring via dedicated thread
- Provides address and socket path accessors

### IDobbyIPCUtils (Interface)
- Plugin-facing interface for IPC operations
- Bus types: `SystemBus`, `AIPublicBus`, `AIPrivateBus`, `NoneBus`
- Methods: `ipcInvokeMethod`, `ipcEmitSignal`, `ipcServiceAvailable`, `ipcRegisterServiceHandler`, `ipcRegisterSignalHandler`

## Utilities

### DobbyUtils
- Implements `IDobbyUtils` interface
- **Loop devices**: associate file to loop device, open/attach loop devices
- **Filesystem**: `mkdirRecursive`, `rmdirRecursive`, `rmdirContents`, `cleanMountLostAndFound`
- **Ext filesystem tools**: `checkExtImageFile`, `formatExtImageFile` (wraps e2fsck/mkfs.ext4)
- **File I/O**: `writeTextFile`, `writeTextFileAt`
- **Namespace operations**: `getNamespaceFd`, `callInNamespace` (executes function in another process's namespace)
- **Device whitelist**: maintains allowed device nodes for containers
- **Timer management**: delegates to `DobbyTimer`

### IDobbyUtils (Interface)
- Provides timer API: `startTimer` (one-shot and repeating), `cancelTimer`
- Loop device and filesystem utilities
- Namespace entry functions with type-safe templates

### DobbyTimer
- Timer queue using `timerfd` and `eventfd` for wake-up
- Supports one-shot and repeating timers (max 63 concurrent)
- Single thread processes all timer callbacks
- Uses monotonic clock for reliability
- Timers auto-removed when handler returns `false`

### DobbyFileAccessFixer
- Fixes file permissions broken by overzealous security hardening
- Targets: DobbyInit binary, `/opt/runtime` directory, graphics driver permissions, core dump filter

### DobbyStartState
- Implements `IDobbyStartState` for the post-construction hook phase
- Allows plugins to add file descriptors, environment variables, and mounts before container launch
- File descriptors are `dup`'d with `FD_CLOEXEC` and tracked per-plugin
- Disposed after container successfully starts

### IDobbyStartState (Interface)
- `addFileDescriptor(pluginName, fd)` - returns container-side FD number (starting at 3)
- `addEnvironmentVariable(envVar)` - appends to container environment
- `addMount(source, target, fsType, mountFlags, mountOptions)` - adds mount entry to config.json

## AppInfrastructure Libraries

### Common
- **File**: `AppInfrastructure/Common/`
- Shared utilities: `FileUtilities`, `Mutex`, `ConditionVariable`, `PollLoop`, `IPollSource`
- Thread-safe mutex and condition variable wrappers

### Logging
- **File**: `AppInfrastructure/Logging/`
- `AI_LOG_*` macros for leveled logging (Fatal, Error, Warn, Milestone, Info, Debug)
- Configurable log printer callback

### Tracing
- **File**: `tracing/include/DobbyTraceCategories.h`
- Optional Perfetto-based tracing (enabled via `AI_ENABLE_TRACING`)
- Categories: Dobby, Plugins, NatNetwork, Containers

## Requirements
- D-Bus must be available for IPC operations.
- The system must support `timerfd` and `eventfd` for timer functionality.
- Namespace operations require appropriate kernel support and capabilities (CAP_SYS_ADMIN).
- Loop device operations require `/dev/loop*` availability.

## Architecture / Design
- `DobbyIPCUtils` aggregates multiple D-Bus connections and presents a unified interface to plugins.
- `DobbyUtils` is a singleton-like utility class injected into plugins and daemon components.
- `DobbyTimer` uses a single thread with `timerfd` to efficiently multiplex up to 63 timers.
- `DobbyStartState` acts as a transient accumulator during container pre-start phase.

## External Interfaces
- **D-Bus**: Three bus connections (System, AIPublic, AIPrivate) for IPC.
- **IDobbyUtils API**: Plugin-facing utility interface.
- **IDobbyIPCUtils API**: Plugin-facing IPC interface.
- **IDobbyStartState API**: Plugin hook interface for pre-start resource injection.

## Performance
- Timer implementation uses monotonic clock and `timerfd` for efficient kernel-level scheduling.
- Single-threaded timer processing avoids contention.

## Security
- File descriptor passing uses `FD_CLOEXEC` to prevent leakage.
- Namespace operations are scoped and require explicit capability grants.

## Versioning & Compatibility
_Not applicable — utilities versioning follows the overall Dobby project version._

## Conformance Testing & Validation
_Not applicable — tested as part of overall Dobby L1/L2 test suites._

## Covered Code
- ipcUtils/include/DobbyIPCUtils.h
- ipcUtils/include/DobbyIpcBus.h
- ipcUtils/include/IDobbyIPCUtils.h
- ipcUtils/source/DobbyIPCUtils.cpp
- ipcUtils/source/DobbyIpcBus.cpp
- utils/include/DobbyUtils.h
- utils/include/IDobbyUtils.h
- utils/include/DobbyFileAccessFixer.h
- utils/include/ContainerId.h
- utils/include/IDobbyEnv.h
- utils/source/DobbyUtils.cpp
- utils/source/DobbyTimer.h
- utils/source/DobbyTimer.cpp
- utils/source/ContainerId.cpp
- utils/source/DobbyFileAccessFixer.cpp
- daemon/lib/source/include/DobbyStartState.h
- daemon/lib/source/DobbyStartState.cpp
- daemon/lib/include/IDobbyStartState.h

---

## Open Queries
_No open queries._

## References
- [D-Bus Specification](https://dbus.freedesktop.org/doc/dbus-specification.html)

## Change History
- 2025-05-18 - openspec-templater - Restructured to match spec template.
