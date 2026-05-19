# Build System & Settings

## Overview
Dobby uses CMake as its build system with extensive configuration options for platform targeting, plugin selection, and feature flags. Settings are loaded from a JSON file at runtime.

## Description
The build system is structured around a top-level CMakeLists.txt that configures the entire Dobby project, including submodules (libocispec), platform-specific options, plugin toggles, and installation targets. Runtime settings are managed via a JSON configuration file parsed by the `Settings` class, implementing the `IDobbySettings` interface. CMake find modules provide discovery for external dependencies.

### Top-Level CMakeLists.txt
- **File**: `CMakeLists.txt`
- Minimum CMake version: 3.7
- Project version defined as `DOBBY_VERSION`
- Configures `libocispec` submodule for OCI schema C header generation
- Installs `DobbyConfig.cmake` and `DobbyConfigVersion.cmake` for downstream CMake consumers

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | - | Debug or Release |
| `ENABLE_DOBBYTOOL` | Debug: ON, Release: OFF | Include DobbyTool CLI |
| `PLUGIN_PATH` | `/usr/lib/plugins/dobby` | RDK plugin shared library directory |
| `RDK_PLATFORM` | `GENERIC` | Platform: `GENERIC` or `DEV_VM` |
| `SETTINGS_FILE` | - | Path to settings JSON installed to `/etc/dobby.json` |
| `SETTINGS_APPEND` | - | Additional settings to merge |
| `LEGACY_COMPONENTS` | OFF | Enable legacy plugins, Dobby specs, ctemplate |
| `ENABLE_LTO` | OFF | Link-Time Optimization (GCC >4.5, recommended >6) |
| `ENABLE_PERFETTO_TRACING` | OFF | Perfetto tracing (debug only, requires SDK) |
| `DOBBY_SERVICE` | `org.rdk.dobby` | D-Bus service name |
| `DOBBY_OBJECT` | `/org/rdk/dobby` | D-Bus object path |
| `USE_STARTCONTAINER_HOOK` | OFF | Enable OCI startContainer hook |
| `USE_SYSTEMD` | ON | Use systemd (sd-bus) vs libdbus |
| `EXTERNAL_PLUGIN_SCHEMA` | - | External JSON schemas for extra RDK plugins (semicolon-separated) |
| `ENABLE_OPT_SETTINGS` | OFF | Search `/opt/` for dobby.json |
| `DOBBY_HIBERNATE_MEMCR_IMPL` | OFF | Enable memcr-based hibernation |
| `DOBBY_HIBERNATE_MEMCR_PARAMS_ENABLED` | OFF | Enable memcr hibernate parameters |

### Plugin Build Flags
Each RDK plugin has `PLUGIN_<NAME>` ON/OFF toggle:
- **Default ON**: Logging, Networking, IPC, Storage, Minidump, Thunder, OOMCrash
- **Default OFF**: AppServices, DeviceMapper, Gamepad, GPU, HttpProxy, IONMemory, LocalTime, RtScheduling, TestPlugin

### CMake Find Modules
- **Directory**: `cmake/`
- `Findbreakpad.cmake` - Google Breakpad crash reporter
- `Findctemplate.cmake` - Google ctemplate (legacy components)
- `Finddbus.cmake` - D-Bus library
- `Findjsoncpp.cmake` - JsonCpp library
- `Findlibnl.cmake` - Netlink library (networking plugin)
- `Findlibocispec.cmake` - OCI spec library
- `FindPerfettoSdk.cmake` - Perfetto tracing SDK
- `Findyajl.cmake` - YAJL JSON parser (libocispec)
- `generateSettingsFile.cmake` - Settings file generation
- `rdk-osx-toolchain.cmake` - macOS cross-compilation toolchain

### Runtime Settings

#### IDobbySettings (Interface)
- **File**: `settings/include/IDobbySettings.h`
- Workspace directory, persistent directory
- Console socket path
- GPU settings (device nodes, memory limits)
- Network settings (external interfaces, address range)
- Log relay settings (syslog/journald socket paths, enable flags)
- Extra environment variables and device nodes

#### Settings Class
- **File**: `settings/source/Settings.cpp`
- Parses JSON settings file (default: `/etc/dobby.json`)
- Structure:
  ```json
  {
    "paths": { "workspaceDir": "...", "persistentDir": "..." },
    "logging": { "consoleSocket": "..." },
    "network": { "externalInterfaces": [...], "addressRange": "..." }
  }
  ```

### Dependencies
- CMake >= 3.7
- crun >= 0.13 (or runc for non-RDK)
- jsoncpp
- yajl 2 (for libocispec)
- ctemplate (legacy components only)
- libsystemd (when `USE_SYSTEMD=ON`)
- libnl + libnl-route (Networking plugin)
- libdbus
- Boost >= 1.61
- `build_dependencies.sh` - Script to install all build dependencies

### Systemd Service
- **File**: `daemon/process/dobby.service`
- Service unit for starting DobbyDaemon at boot
- Configured via `ExecStart` with appropriate CLI options

## Requirements
- CMake >= 3.7 must be available on the build host.
- A valid settings JSON file must be provided for runtime configuration.
- All mandatory dependencies (jsoncpp, yajl, libdbus, Boost >= 1.61) must be installed.
- crun >= 0.13 (or runc) must be available at runtime.
- Plugin shared libraries must be installed at `PLUGIN_PATH`.

## Architecture / Design
- The build system is a single top-level CMake project that aggregates sub-components (daemon, client, plugins, utils, bundle, settings).
- Runtime settings are loaded from a JSON file and exposed via the `IDobbySettings` interface to all daemon components.
- CMake find modules abstract external dependency discovery.
- Plugin build toggles allow platform-specific builds without code changes.

## External Interfaces
- **DobbyConfig.cmake / DobbyConfigVersion.cmake**: Exported CMake package for downstream consumers.
- **Settings JSON** (`/etc/dobby.json`): Runtime configuration file parsed at daemon startup.

## Performance
_Not applicable — build system configuration does not directly impact runtime performance._

## Security
_Not applicable — security concerns are addressed in individual component specs._

## Versioning & Compatibility
- Project version managed via `DOBBY_VERSION` in the top-level CMakeLists.txt.
- `DobbyConfigVersion.cmake` provides version compatibility checks for downstream packages.

## Conformance Testing & Validation
- **L1 tests**: `tests/L1_testing/` - Unit tests
- **L2 tests**: `tests/L2_testing/` - Integration tests with example Dobby specs
- Toolchain files: `tests/clang.cmake`, `tests/gcc-with-coverage.cmake`
- Code coverage: `cov_build.sh`

## Covered Code
- CMakeLists.txt
- settings/include/IDobbySettings.h
- settings/include/Settings.h
- settings/source/Settings.cpp
- cmake/Findbreakpad.cmake
- cmake/Findctemplate.cmake
- cmake/Finddbus.cmake
- cmake/Findjsoncpp.cmake
- cmake/Findlibnl.cmake
- cmake/Findlibocispec.cmake
- cmake/FindPerfettoSdk.cmake
- cmake/Findyajl.cmake
- cmake/generateSettingsFile.cmake
- cmake/rdk-osx-toolchain.cmake
- build_dependencies.sh
- cov_build.sh

---

## Open Queries
_No open queries._

## References
- [CMake Documentation](https://cmake.org/documentation/)
- README.md in repository root

## Change History
- 2025-05-18 - openspec-templater - Restructured to match spec template.
