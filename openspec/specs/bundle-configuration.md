# Bundle & Configuration System

## Overview
The bundle system handles converting container specifications (Dobby JSON specs or OCI bundles) into valid OCI bundles that can be executed by `crun`/`runc`. It manages rootfs creation, config.json generation, and OCI schema validation.

## Components

### DobbyBundle
- **File**: `bundle/lib/include/DobbyBundle.h`, `bundle/lib/source/DobbyBundle.cpp`
- Creates a temporary directory for OCI bundles in the workspace
- Auto-cleans directory on destruction (unless persistence is enabled)
- Holds an open directory file descriptor for `*at()` syscall family operations
- Legacy mode: creates bundle named after container ID in bundles dir
- Bundle mode: uses pre-existing bundle path from caller

### DobbyConfig (Base Class)
- **File**: `bundle/lib/include/DobbyConfig.h`, `bundle/lib/source/DobbyConfig.cpp`
- Abstract base class for container configuration parsers
- Provides common utilities: `scanDevNodes`, `addMount`, `addEnvironmentVariable`, `writeConfigJson`, `changeProcessArgs`
- Defines `NetworkType` enum: `None`, `Nat`, `Open`
- Defines `LoopMount` and `DevNode` structs
- OCI version: `1.0.2` (standard) or `1.0.2-dobby` (extended with RDK plugins)
- Plugin name constants: `networking`, `logging`, `ipc`, `storage`, `gpu`, `rtscheduling`

### DobbySpecConfig (Legacy)
- **File**: `bundle/lib/include/DobbySpecConfig.h`, `bundle/lib/source/DobbySpecConfig.cpp`
- Parses Dobby-specific JSON spec format and converts to OCI config.json
- Uses `ctemplate` for OCI JSON generation from templates
- Handles Dobby-specific fields: `memLimit`, `swapLimit`, `etc` (inline /etc files), `network`, `cpu`, `gpu`, `vpu`, `seccomp`, `dbus`, `plugins`
- Generates rootfs with /etc files (passwd, group, hosts, services, ld.so.preload)
- Only available when `LEGACY_COMPONENTS` is enabled

### DobbyBundleConfig
- **File**: `bundle/lib/include/DobbyBundleConfig.h`, `bundle/lib/source/DobbyBundleConfig.cpp`
- Parses existing OCI bundle `config.json` (extended with `rdkPlugins` section)
- Extracts RDK plugin and legacy plugin configurations
- Handles config recovery: if config.json is corrupted, falls back to `config-dobby.json` backup
- Supports custom UID/GID mapping via `setUidGidMappings`
- Parses console settings, restart-on-crash flag, rootfs path

### DobbyRootfs
- **File**: `bundle/lib/include/DobbyRootfs.h`, `bundle/lib/source/DobbyRootfs.cpp`
- Creates the `rootfs` directory within an OCI bundle
- Legacy mode: populates rootfs with standard mount points and /etc files from spec
- Bundle mode: validates existing rootfs directory from pre-built bundle
- Supports unmounting all filesystems at a path prefix during cleanup

### DobbyTemplate (Legacy)
- **File**: `bundle/lib/include/DobbyTemplate.h`, `bundle/lib/source/DobbyTemplate.cpp`
- Singleton that holds OCI config.json template (ctemplate format)
- Injects platform environment variables, device nodes, and CPU RT scheduling settings
- Two template variants: standard and VM (`DEV_VM`)
- Only available when `LEGACY_COMPONENTS` is enabled

### DobbyBundleGenerator (CLI Tool)
- **File**: `bundle/tool/source/Main.cpp`
- Standalone command-line tool for converting Dobby JSON specs to OCI bundles
- Does not require a running daemon
- Options: `--settings`, `--inputpath`, `--outputDirectory`

## Runtime Schema
- **File**: `bundle/runtime-schemas/dobby_schema.json`
- Extended OCI runtime specification schema
- Standard OCI fields: `ociVersion`, `root`, `process`, `mounts`, `hooks`, `annotations`, `hostname`
- Extended Dobby fields: `rdkPlugins` section for RDK plugin configuration
- Schema generation via `libocispec` submodule produces C headers (`rt_dobby_schema.h`)

## Schema Extension Tools
- `add_external_plugin_schema.py`: Adds external plugin JSON schemas to the Dobby schema
- `add_plugin_tables.py`: Generates plugin data structure definitions from schema files
