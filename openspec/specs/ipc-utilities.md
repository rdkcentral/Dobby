# IPC & Utilities

## Overview
Dobby uses D-Bus for inter-process communication between the daemon, client tools, and plugins. The IPC infrastructure is provided by the `AppInfrastructure` libraries and the `ipcUtils` module. The `utils` module provides filesystem, cgroup, timer, and namespace utilities.

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
- **File**: `ipcUtils/include/DobbyIPCUtils.h`, `ipcUtils/source/DobbyIPCUtils.cpp`
- Implements `IDobbyIPCUtils` interface
- Manages three D-Bus connections: SystemBus, AIPublicBus, AIPrivateBus
- Provides method invocation, signal emission, service monitoring to plugins
- AI bus addresses can be set dynamically at runtime

### DobbyIpcBus
- **File**: `ipcUtils/include/DobbyIpcBus.h`, `ipcUtils/source/DobbyIpcBus.cpp`
- Wraps a single `IIpcService` D-Bus connection
- Supports dynamic connect/disconnect with automatic handler re-registration
- Service change monitoring via dedicated thread
- Provides address and socket path accessors

### IDobbyIPCUtils (Interface)
- **File**: `ipcUtils/include/IDobbyIPCUtils.h`
- Plugin-facing interface for IPC operations
- Bus types: `SystemBus`, `AIPublicBus`, `AIPrivateBus`, `NoneBus`
- Methods: `ipcInvokeMethod`, `ipcEmitSignal`, `ipcServiceAvailable`, `ipcRegisterServiceHandler`, `ipcRegisterSignalHandler`

## Utilities

### DobbyUtils
- **File**: `utils/include/DobbyUtils.h`, `utils/source/DobbyUtils.cpp`
- Implements `IDobbyUtils` interface
- **Loop devices**: associate file to loop device, open/attach loop devices
- **Filesystem**: `mkdirRecursive`, `rmdirRecursive`, `rmdirContents`, `cleanMountLostAndFound`
- **Ext filesystem tools**: `checkExtImageFile`, `formatExtImageFile` (wraps e2fsck/mkfs.ext4)
- **File I/O**: `writeTextFile`, `writeTextFileAt`
- **Namespace operations**: `getNamespaceFd`, `callInNamespace` (executes function in another process's namespace)
- **Device whitelist**: maintains allowed device nodes for containers
- **Timer management**: delegates to `DobbyTimer`

### IDobbyUtils (Interface)
- **File**: `utils/include/IDobbyUtils.h`
- Provides timer API: `startTimer` (one-shot and repeating), `cancelTimer`
- Loop device and filesystem utilities
- Namespace entry functions with type-safe templates

### DobbyTimer
- **File**: `utils/source/DobbyTimer.h`, `utils/source/DobbyTimer.cpp`
- Timer queue using `timerfd` and `eventfd` for wake-up
- Supports one-shot and repeating timers (max 63 concurrent)
- Single thread processes all timer callbacks
- Uses monotonic clock for reliability
- Timers auto-removed when handler returns `false`

### DobbyFileAccessFixer
- **File**: `utils/include/DobbyFileAccessFixer.h`
- Fixes file permissions broken by overzealous security hardening
- Targets: DobbyInit binary, `/opt/runtime` directory, graphics driver permissions, core dump filter

### DobbyStartState
- **File**: `daemon/lib/source/include/DobbyStartState.h`, `daemon/lib/source/DobbyStartState.cpp`
- Implements `IDobbyStartState` for the post-construction hook phase
- Allows plugins to add file descriptors, environment variables, and mounts before container launch
- File descriptors are `dup`'d with `FD_CLOEXEC` and tracked per-plugin
- Disposed after container successfully starts

### IDobbyStartState (Interface)
- **File**: `daemon/lib/include/IDobbyStartState.h`
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

### ReadLine
- **File**: `AppInfrastructure/ReadLine/`
- Interactive CLI wrapper used by DobbyTool
