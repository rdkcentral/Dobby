# Proposal: Cgroups v2 Support for Dobby

**Ticket**: RDKEMW-14901  
**Status**: Implemented 

## Summary

Add cgroups v2 (unified hierarchy) support to Dobby plugins and daemon components. All hardcoded cgroups v1 paths are now guarded by proper version detection checks, allowing Dobby to run correctly on modern kernels that default to cgroups v2.

## Problem Statement

The Linux kernel is migrating to cgroups v2 (unified hierarchy) as the default. While `crun` already supports both v1 and v2, several Dobby plugins and daemon components contained hardcoded cgroups v1 paths and file names that break on cgroups v2 systems:

- Hardcoded paths like `/sys/fs/cgroup/memory/<id>/memory.failcnt`
- Mount table scans that only matched `mnt_type == "cgroup"` (missing `"cgroup2"`)
- v1-only control file names (`memory.limit_in_bytes`, `cpuacct.usage`, etc.)
- OCI template with unconditional `"swappiness": 60` (not supported on v2)

## Detection Strategy

All locations use `stat("/sys/fs/cgroup/cgroup.controllers")` — if the file exists, the system is running cgroups v2. This is the same method already used in the project's CI workflow.

## Changes Implemented

### 1. IDobbyEnv Interface (`utils/include/IDobbyEnv.h`)

- Added `CgroupVersion` enum: `{ V1, V2 }`
- Added pure virtual `cgroupVersion()` method

### 2. DobbyEnv (`daemon/lib/source/DobbyEnv.cpp`, `daemon/lib/source/include/DobbyEnv.h`)

- Added `detectCgroupVersion()` — checks for `/sys/fs/cgroup/cgroup.controllers`
- Updated `getCgroupMountPoints(CgroupVersion)`:
  - On v1: scans `/proc/mounts` for `mnt_type == "cgroup"` per-controller mounts
  - On v2: scans for `mnt_type == "cgroup2"`, maps all controllers to the single unified path
- Added `mCgroupVersion` member initialized at construction

### 3. DobbyStats (`daemon/lib/source/DobbyStats.cpp`)

Branches on `cgroupVersion()` for file names:

| Metric | v1 file | v2 file |
|--------|---------|---------|
| Memory limit | `memory.limit_in_bytes` | `memory.max` |
| Memory usage | `memory.usage_in_bytes` | `memory.current` |
| Memory peak | `memory.max_usage_in_bytes` | `memory.peak` |
| Memory fail count | `memory.failcnt` | N/A (null) |
| CPU usage | `cpuacct.usage` | `cpu.stat` |
| GPU/ION stats | v1 controller files | Skipped (not available on v2) |

### 4. DobbyInit (`daemon/init/source/InitMain.cpp`)

- Added `isCgroupV2()` helper
- Added `readCgroupKeyValue()` for parsing v2's key-value format files
- `checkForOOM()` reads `memory.events` `oom_kill` field on v2, `memory/memory.failcnt` on v1

### 5. OOMCrash Plugin (`rdkPlugins/OOMCrash/source/OOMCrashPlugin.cpp`)

- `readCgroup()` detects v1/v2 at runtime
- On v2: reads `memory.events`, parses `oom_kill` count
- Includes fallback to `system.slice/dobby-<id>.scope/` path (common with systemd on v2)
- On v1: reads `memory.failcnt` as before

### 6. GPU Plugin (`rdkPlugins/GPU/source/GpuPlugin.cpp`)

- `getGpuCgroupMountPoint()` detects v2 early and returns empty string with a warning
- The `gpu` custom cgroup controller does not exist in v2's unified hierarchy
- Plugin gracefully degrades — container still starts, just without GPU memory limits

### 7. IONMemory Plugin (`rdkPlugins/IONMemory/source/IonMemoryPlugin.cpp`)

- `findIonCGroupMountPoint()` detects v2 early and returns empty string with a warning
- The `ion` custom cgroup controller does not exist in v2's unified hierarchy
- Plugin gracefully degrades — container still starts, just without ION memory limits

### 8. DobbyTemplate (`bundle/lib/source/DobbyTemplate.cpp`)

- `setTemplateCpuRtSched()` now recognizes `"cgroup2"` mounts
- On v2, breaks out of loop early (RT scheduling handled differently), values default to `null`

### 9. OCI Templates

**Files:**
- `bundle/lib/source/templates/OciConfigJson1.0.2-dobby.template`
- `bundle/lib/source/templates/OciConfigJsonVM1.0.2-dobby.template`

`"swappiness": 60` is now gated behind a `{{#SWAPPINESS_ENABLED}}` template section.

### 10. DobbySpecConfig (`bundle/lib/source/DobbySpecConfig.cpp`)

- Added `SWAPPINESS_ENABLED` template variable
- Section is shown only when cgroups v1 is detected (v2 does not support swappiness)

### 11. Test Mocks

- `tests/L1_testing/mocks/DobbyEnv.h` — added `cgroupVersion()` to mock interface
- `tests/L1_testing/mocks/DobbyEnvMock.h` — added `MOCK_METHOD` for `cgroupVersion()`
- `tests/L1_testing/mocks/DobbyEnvMock.cpp` — added delegation to impl

## Key Differences: cgroups v1 vs v2

| Aspect | v1 (legacy) | v2 (unified) |
|--------|-------------|--------------|
| Mount type | `cgroup` | `cgroup2` |
| Hierarchy | Per-controller (`/sys/fs/cgroup/memory/`, etc.) | Single tree (`/sys/fs/cgroup/`) |
| Memory limit | `memory.limit_in_bytes` | `memory.max` |
| Memory usage | `memory.usage_in_bytes` | `memory.current` |
| Memory fail count | `memory.failcnt` | `memory.events` (`oom_kill` field) |
| Swap limit | `memory.memsw.limit_in_bytes` | `memory.swap.max` |
| Swappiness | `memory.swappiness` | Not supported |
| CPU usage | `cpuacct.usage` | `cpu.stat` (`usage_usec` field) |
| Detection | N/A | `/sys/fs/cgroup/cgroup.controllers` exists |

## Graceful Degradation

- **GPU/ION plugins**: Custom cgroup controllers don't exist in v2. These plugins log a warning and allow the container to start without resource limits.
- **OOMCrash**: Falls back to `system.slice/dobby-<id>.scope/` path for systemd-managed v2 hierarchies.
- **DobbyStats**: Reports `null` for unavailable metrics rather than failing.
- **Backward compatibility**: All v1 paths remain functional. Changes only add v2 branches alongside existing v1 code.

## Files Modified

| File | Type of change |
|------|---------------|
| `utils/include/IDobbyEnv.h` | Interface addition |
| `daemon/lib/source/include/DobbyEnv.h` | Header update |
| `daemon/lib/source/DobbyEnv.cpp` | Detection + mount scanning |
| `daemon/lib/source/DobbyStats.cpp` | v1/v2 branching |
| `daemon/init/source/InitMain.cpp` | v1/v2 branching |
| `rdkPlugins/OOMCrash/source/OOMCrashPlugin.cpp` | v1/v2 branching |
| `rdkPlugins/GPU/source/GpuPlugin.cpp` | v2 early return |
| `rdkPlugins/IONMemory/source/IonMemoryPlugin.cpp` | v2 early return |
| `bundle/lib/source/DobbyTemplate.cpp` | v2 awareness |
| `bundle/lib/source/DobbySpecConfig.cpp` | Conditional swappiness |
| `bundle/lib/source/templates/OciConfigJson1.0.2-dobby.template` | Template section guard |
| `bundle/lib/source/templates/OciConfigJsonVM1.0.2-dobby.template` | Template section guard |
| `tests/L1_testing/mocks/DobbyEnv.h` | Mock update |
| `tests/L1_testing/mocks/DobbyEnvMock.h` | Mock update |
| `tests/L1_testing/mocks/DobbyEnvMock.cpp` | Mock update |
