# Plugin System

## Overview
Dobby has two plugin architectures: the modern **RDK Plugin** system (recommended) and the **Legacy Plugin** system (deprecated, behind `LEGACY_COMPONENTS` flag). Plugins are shared libraries loaded at runtime from a configurable directory.

## RDK Plugin System

### IDobbyRdkPlugin (Interface)
- **File**: `pluginLauncher/lib/include/IDobbyRdkPlugin.h`
- Pure virtual interface for RDK plugins
- Hook points (in lifecycle order):
  1. `postInstallation` - After bundle downloaded, before runtime create (runs once per container lifecycle)
  2. `preCreation` - Before runtime create operation (runs every time container is created)
  3. `createRuntime` - During create, after namespaces created, before pivot_root (host namespace, OCI runtime)
  4. `createContainer` - During create, after namespaces, in container namespace (OCI runtime)
  5. `startContainer` - After user-specified process executes, before start (optional, via `USE_STARTCONTAINER_HOOK`)
  6. `postStart` - After start operation returns (OCI runtime)
  7. `postHalt` - After container stopped (Dobby hook)
  8. `postStop` - After container deleted (OCI runtime)
- `HintFlags` bitmask declares which hooks a plugin implements
- `getDependencies()` returns list of plugins this one depends on
- Plugin registration via `REGISTER_RDK_PLUGIN(ClassName)` macro

### IDobbyRdkLoggingPlugin (Interface)
- Extension of `IDobbyRdkPlugin` for logging plugins
- Additional methods: `RegisterPollSources`, `DumpToLog`
- Logging plugins receive container console output via file descriptors

### DobbyRdkPluginManager
- **File**: `pluginLauncher/lib/include/DobbyRdkPluginManager.h`, `pluginLauncher/lib/source/DobbyRdkPluginManager.cpp`
- Scans plugin directory for `.so` files, loads via `dlopen`/`dlsym`
- Looks for `createIDobbyRdkPlugin` and `destroyIDobbyRdkPlugin` symbols
- Separately tracks logging plugins (`createIDobbyRdkLoggingPlugin`)
- Uses `DobbyRdkPluginDependencySolver` for topological ordering
- Executes hooks with optional timeout (kills plugin thread on timeout)
- Manages `rt_dobby_schema` container config shared across plugins

### DobbyRdkPluginDependencySolver
- **File**: `pluginLauncher/lib/source/DobbyRdkPluginDependencySolver.h`, `pluginLauncher/lib/source/DobbyRdkPluginDependencySolver.cpp`
- Uses Boost Graph Library (BGL) adjacency list with topological sort
- Plugin names are case-insensitive (stored lowercase)
- Provides forward and reverse dependency ordering

### DobbyRdkPluginUtils
- **File**: `pluginLauncher/lib/include/DobbyRdkPluginUtils.h`, `pluginLauncher/lib/source/DobbyRdkPluginUtils.cpp`
- Shared utility class passed to all plugins
- Container network info tracking (veth name, IP address)
- Mount/environment variable manipulation on `rt_dobby_schema`
- Namespace operations (`callInNamespace`)
- File descriptor passing via `IDobbyStartState`
- Container ID and exit status tracking

### RdkPluginBase
- **File**: `rdkPlugins/Common/include/RdkPluginBase.h`
- Base class with default no-op implementations for all hooks
- Plugins only need to override hooks they use

### DobbyLoggerBase
- **File**: `rdkPlugins/Common/include/DobbyLoggerBase.h`
- Base class for logging plugins with default no-op hook implementations

### DobbyPluginLauncher (CLI Tool)
- **File**: `pluginLauncher/tool/source/Main.cpp`
- Invoked by OCI runtime at hook points
- Options: `--hook` (which hook to run), `--config` (path to OCI config.json)
- Loads plugins from `/usr/lib/plugins/dobby` (configurable)

## Built-in RDK Plugins

| Plugin | Description | Key Hooks |
|--------|-------------|-----------|
| **Networking** | NAT/open/none network modes, veth pairs, iptables, DNS, port forwarding, multicast, inter-container routing | createRuntime, postHalt |
| **Storage** | Loop-mount devices, dynamic mounts, mount ownership management | preCreation, createRuntime, createContainer, postStart, postStop |
| **Logging** | Container console log routing to file, journald, or /dev/null | postInstallation, RegisterPollSources |
| **IPC** | D-Bus socket bind-mount into container (system, session, debug buses) | postInstallation |
| **AppServices** | iptables routing to Application Services (AS) ports | postInstallation, createRuntime, postHalt |
| **Thunder** | WPEFramework access: iptables routing + security token injection | postInstallation, preCreation, createRuntime, postHalt |
| **DeviceMapper** | Maps host device nodes into container with correct major/minor numbers | preCreation |
| **Gamepad** | Exposes gamepad/joystick input device nodes to container | postInstallation |
| **GPU** | GPU cgroup memory limit setup and enforcement | createRuntime, postStop |
| **HttpProxy** | Sets `http_proxy`/`no_proxy` env vars, adds root CA certificates | postInstallation, preCreation, postHalt |
| **IONMemory** | ION cgroup controller for Android-style raw memory allocation limits | createRuntime, postStop |
| **LocalTime** | Symlinks host `/etc/localtime` into container rootfs | postInstallation, preCreation |
| **Minidump** | Collects minidump crash files from container namespace to host | preCreation, postHalt |
| **OOMCrash** | Detects OOM kills via cgroup and creates marker file | postInstallation, postHalt |
| **RtScheduling** | Sets real-time scheduling priority (SCHED_RR/SCHED_FIFO) on container init process | postInstallation, createRuntime |
| **TestPlugin** | Reference implementation exercising all hook points | all hooks |

## Legacy Plugin System

### IDobbyPlugin (Interface)
- **File**: `daemon/lib/include/IDobbyPlugin.h`
- Hook points: `postConstruction`, `preStart`, `postStart`, `postStop`, `preDestruction`
- Each hook has sync and async variants (via `HintFlags`)
- Plugin registration via `createIDobbyPlugin`/`destroyIDobbyPlugin` symbols

### DobbyLegacyPluginManager
- **File**: `daemon/lib/source/include/DobbyLegacyPluginManager.h`, `daemon/lib/source/DobbyLegacyPluginManager.cpp`
- Scans plugin directory, loads `.so` files via `dlopen`
- Executes hooks with per-plugin JSON config from container spec
- Uses `pthread_rwlock` for thread-safe plugin map access
- Only available when `LEGACY_COMPONENTS` is enabled

### Legacy Plugins
- **EthanLog**: `plugins/EthanLog/` - EthanLog logging integration
- **MulticastSockets**: `plugins/MulticastSockets/` - Multicast socket forwarding
- **OpenCDM**: `plugins/OpenCDM/` - Open Content Decryption Module access
- **Perfetto**: `plugins/Perfetto/` - Perfetto tracing integration
