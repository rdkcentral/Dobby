# Bundle & Configuration System

## Overview
The bundle system handles converting container specifications (Dobby JSON specs or OCI bundles) into valid OCI bundles that can be executed by `crun`/`runc`. It manages rootfs creation, config.json generation, and OCI schema validation.

## Description
Dobby supports two container specification formats: legacy Dobby JSON specs (converted via `DobbySpecConfig` using ctemplate) and modern OCI bundles with an extended `rdkPlugins` section (parsed by `DobbyBundleConfig`). The `DobbyBundle` class manages temporary bundle directories, `DobbyRootfs` handles rootfs creation/validation, and `DobbyConfig` provides the abstract base for configuration parsers. A standalone CLI tool (`DobbyBundleGenerator`) allows offline bundle generation without a running daemon.

### DobbyBundle
- Creates a temporary directory for OCI bundles in the workspace
- Auto-cleans directory on destruction (unless persistence is enabled)
- Holds an open directory file descriptor for `*at()` syscall family operations
- Legacy mode: creates bundle named after container ID in bundles dir
- Bundle mode: uses pre-existing bundle path from caller

### DobbyConfig (Base Class)
- Abstract base class for container configuration parsers
- Provides common utilities: `scanDevNodes`, `addMount`, `addEnvironmentVariable`, `writeConfigJson`, `changeProcessArgs`
- Defines `NetworkType` enum: `None`, `Nat`, `Open`
- Defines `LoopMount` and `DevNode` structs
- OCI version: `1.0.2` (standard) or `1.0.2-dobby` (extended with RDK plugins)
- Plugin name constants: `networking`, `logging`, `ipc`, `storage`, `gpu`, `rtscheduling`

### DobbySpecConfig (Legacy)
- Parses Dobby-specific JSON spec format and converts to OCI config.json
- Uses `ctemplate` for OCI JSON generation from templates
- Handles Dobby-specific fields: `memLimit`, `swapLimit`, `etc` (inline /etc files), `network`, `cpu`, `gpu`, `vpu`, `seccomp`, `dbus`, `plugins`
- Generates rootfs with /etc files (passwd, group, hosts, services, ld.so.preload)
- Only available when `LEGACY_COMPONENTS` is enabled

### DobbyBundleConfig
- Parses existing OCI bundle `config.json` (extended with `rdkPlugins` section)
- Extracts RDK plugin and legacy plugin configurations
- Handles config recovery: if config.json is corrupted, falls back to `config-dobby.json` backup
- Supports custom UID/GID mapping via `setUidGidMappings`
- Parses console settings, restart-on-crash flag, rootfs path

### DobbyRootfs
- Creates the `rootfs` directory within an OCI bundle
- Legacy mode: populates rootfs with standard mount points and /etc files from spec
- Bundle mode: validates existing rootfs directory from pre-built bundle
- Supports unmounting all filesystems at a path prefix during cleanup

### DobbyTemplate (Legacy)
- Singleton that holds OCI config.json template (ctemplate format)
- Injects platform environment variables, device nodes, and CPU RT scheduling settings
- Two template variants: standard and VM (`DEV_VM`)
- Only available when `LEGACY_COMPONENTS` is enabled

### DobbyBundleGenerator (CLI Tool)
- Standalone command-line tool for converting Dobby JSON specs to OCI bundles
- Does not require a running daemon
- Options: `--settings`, `--inputpath`, `--outputDirectory`

### Runtime Schema
- **File**: `bundle/runtime-schemas/dobby_schema.json`
- Extended OCI runtime specification schema
- Standard OCI fields: `ociVersion`, `root`, `process`, `mounts`, `hooks`, `annotations`, `hostname`
- Extended Dobby fields: `rdkPlugins` section for RDK plugin configuration
- Schema generation via `libocispec` submodule produces C headers (`rt_dobby_schema.h`)

### Schema Extension Tools
- `add_external_plugin_schema.py`: Adds external plugin JSON schemas to the Dobby schema
- `add_plugin_tables.py`: Generates plugin data structure definitions from schema files

## Requirements
- A valid OCI bundle or Dobby JSON spec must be provided to create a container.
- The workspace directory must be writable for temporary bundle creation.
- Legacy mode requires `LEGACY_COMPONENTS` to be enabled at build time and `ctemplate` to be installed.
- The `libocispec` submodule must be available for schema generation.

## Architecture / Design
- `DobbyConfig` is the abstract base; `DobbySpecConfig` (legacy) and `DobbyBundleConfig` (modern) are concrete implementations.
- `DobbyBundle` manages the lifecycle of the bundle directory on disk.
- `DobbyRootfs` creates or validates the rootfs within the bundle.
- The pipeline is: Spec/Bundle input → Config parser → Rootfs setup → OCI config.json output → ready for `crun`.

## External Interfaces
- **OCI config.json**: Standard OCI runtime spec format (v1.0.2) with optional `rdkPlugins` extension.
- **Dobby JSON spec**: Legacy proprietary format for container definitions.
- **dobby_schema.json**: Extended OCI schema used for validation.

## Performance
_Not applicable — bundle creation is a one-time setup operation per container launch._

## Security
- Config recovery mechanism prevents container launch with corrupted configurations.
- UID/GID mapping support enables user namespace isolation.

## Versioning & Compatibility
- OCI version `1.0.2` for standard bundles, `1.0.2-dobby` for extended bundles.
- Legacy format support gated behind `LEGACY_COMPONENTS` flag for backward compatibility.

## Conformance Testing & Validation
_Not applicable — validation is performed via OCI schema conformance at runtime._

## Covered Code
- bundle/lib/include/DobbyBundle.h
- bundle/lib/include/DobbyBundleConfig.h
- bundle/lib/include/DobbyConfig.h
- bundle/lib/include/DobbyRootfs.h
- bundle/lib/include/DobbySpecConfig.h
- bundle/lib/include/DobbyTemplate.h
- bundle/lib/source/DobbyBundle.cpp
- bundle/lib/source/DobbyBundleConfig.cpp
- bundle/lib/source/DobbyConfig.cpp
- bundle/lib/source/DobbyRootfs.cpp
- bundle/lib/source/DobbySpecConfig.cpp
- bundle/lib/source/DobbyTemplate.cpp
- bundle/tool/source/Main.cpp

---

## Open Queries
_No open queries._

## References
- [OCI Runtime Specification](https://github.com/opencontainers/runtime-spec)

## Change History
- 2025-05-18 - openspec-templater - Restructured to match spec template.
