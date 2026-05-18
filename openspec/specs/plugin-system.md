# Plugin System

## Overview
Dobby has two plugin architectures: the modern **RDK Plugin** system (recommended) and the **Legacy Plugin** system (deprecated, behind `LEGACY_COMPONENTS` flag). Plugins are shared libraries loaded at runtime from a configurable directory.

## Description
The RDK plugin system provides a lifecycle-hook-based architecture where plugins implement the `IDobbyRdkPlugin` interface and declare which OCI hook points they participate in. The `DobbyRdkPluginManager` discovers, loads, and executes plugins with dependency-aware ordering via `DobbyRdkPluginDependencySolver`. A shared utility class (`DobbyRdkPluginUtils`) provides common operations. A CLI tool (`DobbyPluginLauncher`) is invoked by the OCI runtime at hook points. The legacy system (`IDobbyPlugin`) is deprecated but maintained behind a build flag.

## RDK Plugin System

### IDobbyRdkPlugin (Interface)
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
- Scans plugin directory for `.so` files, loads via `dlopen`/`dlsym`
- Looks for `createIDobbyRdkPlugin` and `destroyIDobbyRdkPlugin` symbols
- Separately tracks logging plugins (`createIDobbyRdkLoggingPlugin`)
- Uses `DobbyRdkPluginDependencySolver` for topological ordering
- Executes hooks with optional timeout (kills plugin thread on timeout)
- Manages `rt_dobby_schema` container config shared across plugins

### DobbyRdkPluginDependencySolver
- Uses Boost Graph Library (BGL) adjacency list with topological sort
- Plugin names are case-insensitive (stored lowercase)
- Provides forward and reverse dependency ordering

### DobbyRdkPluginUtils
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
- Scans plugin directory, loads `.so` files via `dlopen`
- Executes hooks with per-plugin JSON config from container spec

## Requirements
- Plugin shared libraries must be installed at the configured `PLUGIN_PATH` (default: `/usr/lib/plugins/dobby`).
- Plugins must export `createIDobbyRdkPlugin` and `destroyIDobbyRdkPlugin` symbols.
- Logging plugins must additionally export `createIDobbyRdkLoggingPlugin`.
- Boost Graph Library must be available for dependency resolution.
- Legacy plugins require `LEGACY_COMPONENTS` to be enabled at build time.

## Architecture / Design
- Plugins are discovered at runtime via directory scan and loaded with `dlopen`.
- `DobbyRdkPluginManager` orchestrates hook execution in dependency-sorted order.
- `DobbyRdkPluginDependencySolver` performs topological sort on the plugin dependency graph.
- Each plugin receives a shared `DobbyRdkPluginUtils` instance and access to the container's `rt_dobby_schema`.
- The `DobbyPluginLauncher` CLI is the entry point invoked by `crun` at OCI hook points.

## External Interfaces
- **IDobbyRdkPlugin API**: C++ interface that all RDK plugins must implement.
- **OCI Hooks**: The plugin launcher is invoked by the OCI runtime at defined hook points.
- **Plugin .so ABI**: `createIDobbyRdkPlugin` / `destroyIDobbyRdkPlugin` exported symbols.

## Performance
- Hook execution has configurable timeouts; plugins that exceed timeout are killed.
- Dependency solver uses efficient topological sort for ordering.

## Security
- Plugins run in the daemon process context with full privileges.
- Network plugins manage iptables rules for container isolation.
- Thunder plugin handles security token injection for WPEFramework access control.

## Versioning & Compatibility
- RDK plugin interface is the current standard; legacy interface is deprecated.
- Legacy plugins are only loaded when `LEGACY_COMPONENTS` is enabled.

## Conformance Testing & Validation
- `TestPlugin` serves as a reference implementation exercising all hook points.
- Plugin functionality tested as part of L1/L2 test suites.

## Covered Code
- pluginLauncher/lib/include/IDobbyRdkPlugin.h
- pluginLauncher/lib/include/IDobbyRdkLoggingPlugin.h
- pluginLauncher/lib/include/DobbyRdkPluginManager.h
- pluginLauncher/lib/include/DobbyRdkPluginUtils.h
- pluginLauncher/lib/source/DobbyRdkPluginManager.cpp
- pluginLauncher/lib/source/DobbyRdkPluginUtils.cpp
- pluginLauncher/lib/source/DobbyRdkPluginDependencySolver.h
- pluginLauncher/lib/source/DobbyRdkPluginDependencySolver.cpp
- pluginLauncher/tool/source/Main.cpp
- rdkPlugins/Common/include/RdkPluginBase.h
- rdkPlugins/Common/include/DobbyLoggerBase.h
- daemon/lib/include/IDobbyPlugin.h
- daemon/lib/source/DobbyLegacyPluginManager.cpp
- daemon/lib/source/include/DobbyLegacyPluginManager.h

---

## Open Queries
_No open queries._

## References
- [OCI Runtime Specification - Hooks](https://github.com/opencontainers/runtime-spec/blob/main/config.md#posix-platform-hooks)

## Change History
- 2025-05-18 - openspec-templater - Restructured to match spec template.
