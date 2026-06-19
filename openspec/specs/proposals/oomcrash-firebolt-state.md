# Proposal: OOMCrash Plugin — Accurate OOM Kill Detection and Firebolt State Logging

**Ticket**: RDKEMW-16955  
**Status**: Implemented  
**Component**: rdkPlugins/OOMCrash

---

## Summary

The OOMCrash plugin's cgroups v1 OOM detection is replaced with a more accurate mechanism, and a new log message is emitted on OOM kill that includes the container's Firebolt application state. This gives field operators immediate visibility into what an app was doing when it ran out of memory.

---

## Problem Statement

### Inaccurate OOM detection on cgroups v1

The plugin previously read `memory.failcnt` to detect OOM events. This counter increments every time the cgroup's memory limit is exceeded — but it does **not** mean the container was killed by the OOM killer. When the container is configured with unlimited swap the kernel satisfies the allocation via swap instead; the container continues running and may later exit for an entirely unrelated reason. Because `postHalt` runs only after the container has terminated, `failcnt > 0` at that point is a false positive: it indicates a past memory pressure event, not that the exit was caused by the OOM killer.

### No Firebolt state visibility on OOM kill

When a container is genuinely OOM killed, the only observable evidence in Dobby logs was the absence of a clean exit. Operators had no way to know what Firebolt application state the container was in at the time, making root-cause triage of memory exhaustion bugs difficult in field deployments.

### Race between AppService annotation update and postHalt

AppService updates the `fireboltState` annotation within milliseconds of detecting a crash (typically transitioning the app to `"background"`). Because `postHalt` runs some time after the actual kill, reading only `fireboltState` at hook time can return the post-death state rather than the state at kill time. PR #454 introduced a timestamp-aware annotation history system (`fireboltState_ts`, `fireboltState_prev`, `fireboltState_prev_ts`) that makes the previous value available.

---

## Root Cause Analysis

### `memory.failcnt` counts exceedances, not kills

```
Container memory limit exceeded → kernel tries swap
  if swap available  → allocation succeeds, failcnt++ but process lives
  if swap unavailable → OOM killer invoked, oom_kill++
```

`memory.failcnt` fires in both paths. `memory.oom_control`'s `oom_kill` field (cgroups v1) and `memory.events`'s `oom_kill` field (cgroups v2) only increment when the OOM killer actually terminates a process.

### Annotation staleness at postHalt time

```
t0: Container running — fireboltState = "foreground"
t1: OOM kill (SIGKILL from kernel)
t2: AppService detects crash → sets fireboltState = "background"   (ms later)
t3: postHalt hook runs
     ↓ reading fireboltState here returns "background", not "foreground"
```

The PR #454 annotation history preserves the previous value as `fireboltState_prev`, which captures `"foreground"` — the state at kill time.

---

## Solution

### 1. OOM detection: `memory.oom_control` on cgroups v1

Replace `memory.failcnt` with the `oom_kill` field from `/sys/fs/cgroup/memory/<id>/memory.oom_control`. This file uses the same key-value format as cgroups v2's `memory.events`, so the parsing logic is symmetric:

| cgroups version | file | key |
|---|---|---|
| v1 | `memory.oom_control` | `oom_kill` |
| v2 | `memory.events` | `oom_kill` |
| v2 (systemd scope) | `system.slice/dobby-<id>.scope/memory.events` | `oom_kill` |

OOM kill is confirmed if and only if `oom_kill > 0`.

### 2. Kernel < 4.13 fallback heuristic

The `oom_kill` field in `memory.oom_control` was introduced in Linux kernel 4.13. On older kernels the key is absent. A two-condition heuristic is used as a fallback:

- `memory.failcnt > 0` — memory limit was exceeded at some point
- Exit status encodes SIGKILL: `WIFSIGNALED(exitStatus) && WTERMSIG(exitStatus) == SIGKILL`

Both conditions together strongly indicate an OOM kill. The heuristic is logged with an explicit warning that detection is approximate. If either condition is not met, OOM reporting is skipped silently.

`mUtils->exitStatus` contains the synthesized `waitpid` status (converted from DobbyInit's `128+signum` exit-code convention by `DobbyManager::synthesizeContainerSignalStatus` before `postHalt` runs), so `WTERMSIG` gives the correct signal without any additional decoding.

### 3. Firebolt state logging

When an OOM kill is confirmed, `checkForOOM()` calls `mUtils->getAnnotations()` and logs both:

- `fireboltState` — the current annotation value (may be post-death state set by AppService)
- `fireboltState_prev` — the value before the last `addAnnotation()` call (most likely the state at kill time)

Example log output:
```
WARN: OOM kill detected in container 'de.sky.ZDF' (oom events = 1), firebolt state: background (prev: foreground)
```

---

## Files Modified

| File | Change |
|---|---|
| `rdkPlugins/OOMCrash/source/OOMCrashPlugin.cpp` | `readCgroup()` v1 path reads `memory.oom_control`; kernel < 4.13 fallback; `checkForOOM()` logs `fireboltState` + `fireboltState_prev` |

## Dependencies

| Dependency | Reason |
|---|---|
| PR #454 (`pluginLauncher/lib/source/DobbyRdkPluginUtils.cpp`) | `addAnnotation()` now stores `_prev` and `_ts` history keys required for accurate state logging |
