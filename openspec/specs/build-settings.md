# Build System & Settings

## Overview
Dobby uses CMake as its build system with extensive configuration options for platform targeting, plugin selection, and feature flags. Settings are loaded from a JSON file at runtime.

## CMake Structure

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
- **Default ON**: Logging, Networking, IPC, Storage, Minidump
- **Default OFF**: AppServices, DeviceMapper, Gamepad, GPU, HttpProxy, IONMemory, LocalTime, OOMCrash, RtScheduling, TestPlugin, Thunder

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

## Runtime Settings

### IDobbySettings (Interface)
- **File**: `settings/include/IDobbySettings.h`
- Workspace directory, persistent directory
- Console socket path
- GPU settings (device nodes, memory limits)
- Network settings (external interfaces, address range)
- Log relay settings (syslog/journald socket paths, enable flags)
- Extra environment variables and device nodes

### Settings Class
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

## Dependencies
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

## Testing
- **L1 tests**: `tests/L1_testing/` - Unit tests
- **L2 tests**: `tests/L2_testing/` - Integration tests with example Dobby specs
- Toolchain files: `tests/clang.cmake`, `tests/gcc-with-coverage.cmake`
- Code coverage: `cov_build.sh`

## Systemd Service
- **File**: `daemon/process/dobby.service`
- Service unit for starting DobbyDaemon at boot
- Configured via `ExecStart` with appropriate CLI options
