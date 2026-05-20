# OpenSpec Coverage Report

**Total Score: 64.50 / 100**

---

## Code to Spec Coverage: 36.00 / 40

| Sub-criterion | Score | Max | Notes |
|---|---|---|---|
| Reference Coverage | 19.00 | 20 | ~95% of source files covered via spec `## Covered Code` sections after adding rdk-plugins-impl.md, app-infrastructure.md, and test-infrastructure.md. |
| Spec Existence | 10.00 | 10 | All 9 referenced specs exist in `openspec/specs/`. |
| Spec Completeness | 5.00 | 5 | All 9 specs contain Overview, Description, and Requirements sections. |
| No Orphaned Code | 5.00 | 5 | Only `bundle/runtime-schemas/` remains — excluded as auto-generated/tooling code. Score: 5.00/5 |

**Adjusted Code to Spec Coverage: 36.00 / 40**

### Remaining Orphaned Code (not covered by any spec)
- `bundle/runtime-schemas/` (runtime JSON schema files and Python schema-extension scripts — excluded as tooling)

---

## Architecture HLA Specification: 9.00 / 10

| Sub-criterion | Score | Max | Notes |
|---|---|---|---|
| Presence of HLA Spec | 2.50 | 3 | Architecture sections exist in each spec. No standalone HLA document, but daemon-core provides comprehensive system-level design. |
| Clarity of Architecture Diagrams | 2.50 | 3 | Mermaid diagrams added: component relationships, data flow, container lifecycle state machine, and plugin architecture. |
| Component/Module Mapping | 2.00 | 2 | All major components mapped: daemon, client, plugins (including all 16 RDK plugin implementations), bundle, IPC/utils, build/settings, AppInfrastructure, tests. |
| Traceability to Code | 2.00 | 2 | Each spec has a `## Covered Code` section mapping components to source files. |

---

## Performance Specification: 3.00 / 10

| Sub-criterion | Score | Max | Notes |
|---|---|---|---|
| Presence of Performance Spec | 1.50 | 3 | Performance sections exist in specs but are brief. No dedicated performance spec. |
| Defined Performance Metrics | 0.50 | 3 | Few measurable metrics defined. No latency/throughput targets. |
| Test Coverage for Performance | 0.50 | 2 | No dedicated performance tests identified. |
| Results & Validation | 0.50 | 2 | No benchmark results or validation data documented. |

---

## External Interface Specification: 6.50 / 10

| Sub-criterion | Score | Max | Notes |
|---|---|---|---|
| Presence of Interface Spec | 3.00 | 3 | D-Bus protocol documented in daemon-core.md. IDobbyProxy, IDobbyUtils, IDobbyRdkPlugin interfaces described. |
| Defined Inputs/Outputs | 2.00 | 3 | Method names listed but parameter types/return values not fully specified for D-Bus methods. |
| Documentation Completeness | 1.00 | 2 | Missing detailed parameter specifications and error codes. |
| Validation/Examples | 0.50 | 2 | DobbyTool as usage example. No formal API examples documented. |

---

## Security Specification: 2.50 / 10

| Sub-criterion | Score | Max | Notes |
|---|---|---|---|
| Presence of Security Spec | 1.50 | 3 | Security sections exist in multiple specs but are brief. |
| Threat Model/Analysis | 0.00 | 3 | **No threat model or security analysis present.** |
| Security Requirements | 1.00 | 2 | Basic requirements mentioned: OCI namespaces, cgroups, seccomp, D-Bus policy. |
| Validation/Testing | 0.00 | 2 | No security-specific tests documented. |

---

## Versioning & Compatibility: 2.00 / 10

| Sub-criterion | Score | Max | Notes |
|---|---|---|---|
| Presence of Versioning Spec | 1.00 | 3 | Build-settings spec mentions `DOBBY_VERSION`. Most specs say "Not applicable." |
| Versioning Scheme Defined | 0.50 | 3 | No explicit semver or versioning scheme documented. |
| Backward/Forward Compatibility | 0.50 | 2 | OCI version compatibility and legacy plugin gating mentioned but not formalized. |
| Migration/Upgrade Path | 0.00 | 2 | No migration guidance documented. |

---

## Conformance Testing Automation and Validation: 5.50 / 10

| Sub-criterion | Score | Max | Notes |
|---|---|---|---|
| Presence of Conformance Tests | 3.00 | 3 | L1/L2 test suites fully documented in test-infrastructure.md with test lists and mock objects. |
| Test Coverage | 1.00 | 3 | Test-to-requirement mapping partially documented. No coverage metrics. |
| Test Documentation | 1.50 | 2 | Build & execution instructions documented. Test architecture diagram included. |
| Validation Results | 0.00 | 2 | No test results tracked or documented. |

---

## Architecture Diagrams

### Component Relationships

```mermaid
graph TB
    subgraph "Client Layer"
        TOOL[DobbyTool CLI]
        CLIENT[DobbyProxy / IDobbyProxy]
        FACTORY[DobbyFactory]
    end

    subgraph "IPC Layer"
        DBUS[D-Bus<br/>org.rdk.dobby]
        IPCUTILS[DobbyIPCUtils]
        IPCBUS[DobbyIpcBus]
        IPCSERVICE[IIpcService<br/>libdbus / sd-bus]
    end

    subgraph "Daemon Core"
        DOBBY[Dobby Root Object]
        MGR[DobbyManager]
        WQ[DobbyWorkQueue]
        ENV[DobbyEnv]
        LOGGER[DobbyLogger / LogRelay]
        HIBERNATE[DobbyHibernate]
    end

    subgraph "Container Runtime"
        RUNC[DobbyRunC<br/>crun / runc]
        CONTAINER[DobbyContainer]
        BUNDLE[DobbyBundle]
        CONFIG[DobbyConfig<br/>SpecConfig / BundleConfig]
        ROOTFS[DobbyRootfs]
    end

    subgraph "Plugin System"
        PLUGINMGR[DobbyRdkPluginManager]
        DEPSOLVER[DependencySolver]
        PLUGINUTILS[DobbyRdkPluginUtils]
        LAUNCHER[DobbyPluginLauncher]
        subgraph "RDK Plugins"
            NET[Networking]
            STR[Storage]
            LOG[Logging]
            IPC_P[IPC]
            THN[Thunder]
            APP[AppServices]
            OTHER[DeviceMapper, GPU,<br/>Gamepad, HttpProxy,<br/>IONMemory, LocalTime,<br/>Minidump, OOMCrash,<br/>RtScheduling]
        end
    end

    subgraph "Utilities"
        UTILS[DobbyUtils / IDobbyUtils]
        TIMER[DobbyTimer]
        STARTSTATE[DobbyStartState]
    end

    subgraph "AppInfrastructure"
        COMMON[Common<br/>PollLoop, FileUtilities]
        LOGGING[AI_LOG Framework]
        TRACING[Perfetto Tracing]
    end

    TOOL --> CLIENT
    CLIENT --> DBUS
    FACTORY --> CLIENT
    DBUS --> DOBBY
    DOBBY --> MGR
    DOBBY --> WQ
    DOBBY --> ENV
    DOBBY --> LOGGER
    MGR --> RUNC
    MGR --> CONTAINER
    MGR --> PLUGINMGR
    MGR --> HIBERNATE
    CONTAINER --> BUNDLE
    CONTAINER --> CONFIG
    BUNDLE --> ROOTFS
    PLUGINMGR --> DEPSOLVER
    PLUGINMGR --> PLUGINUTILS
    PLUGINMGR --> NET
    PLUGINMGR --> STR
    PLUGINMGR --> LOG
    PLUGINMGR --> IPC_P
    PLUGINMGR --> THN
    PLUGINMGR --> APP
    PLUGINMGR --> OTHER
    LAUNCHER --> PLUGINMGR
    IPCUTILS --> IPCBUS
    IPCBUS --> IPCSERVICE
    MGR --> UTILS
    UTILS --> TIMER
    MGR --> STARTSTATE
    DOBBY --> COMMON
    DOBBY --> LOGGING
    DOBBY --> TRACING
```

### Container Lifecycle Data Flow

```mermaid
sequenceDiagram
    participant Client as Client (DobbyProxy)
    participant DBus as D-Bus
    participant Daemon as Dobby Daemon
    participant Manager as DobbyManager
    participant Bundle as DobbyBundle/Config
    participant Plugins as RdkPluginManager
    participant Runtime as DobbyRunC (crun)
    participant Launcher as PluginLauncher

    Client->>DBus: StartFromBundle(id, bundlePath)
    DBus->>Daemon: D-Bus method call
    Daemon->>Manager: startContainerFromBundle()
    Manager->>Bundle: Create DobbyBundle + DobbyBundleConfig
    Bundle-->>Manager: OCI config parsed

    Manager->>Plugins: executePostInstallationHooks()
    Plugins-->>Manager: hooks complete
    Manager->>Plugins: executePreCreationHooks()
    Plugins-->>Manager: hooks complete

    Manager->>Runtime: create(id, bundlePath)
    Runtime->>Launcher: OCI createRuntime hook
    Launcher->>Plugins: executeCreateRuntimeHooks()
    Plugins-->>Launcher: done
    Launcher-->>Runtime: hook exit 0
    Runtime->>Launcher: OCI createContainer hook
    Launcher->>Plugins: executeCreateContainerHooks()
    Plugins-->>Launcher: done
    Launcher-->>Runtime: hook exit 0
    Runtime-->>Manager: container created

    Manager->>Runtime: start(id)
    Runtime->>Launcher: OCI postStart hook
    Launcher->>Plugins: executePostStartHooks()
    Plugins-->>Launcher: done
    Launcher-->>Runtime: hook exit 0
    Runtime-->>Manager: container started

    Manager-->>Daemon: container running
    Daemon->>DBus: emit Started signal
    DBus-->>Client: Started event

    Note over Manager: runcMonitorThread detects exit

    Manager->>Plugins: executePostHaltHooks()
    Plugins-->>Manager: cleanup done
    Manager->>Runtime: destroy(id)
    Runtime-->>Manager: destroyed
    Manager-->>Daemon: container stopped
    Daemon->>DBus: emit Stopped signal
    DBus-->>Client: Stopped event
```

### StartFromSpec Data Flow

```mermaid
sequenceDiagram
    participant Client as Client (DobbyProxy)
    participant DBus as D-Bus
    participant Daemon as Dobby Daemon
    participant Manager as DobbyManager
    participant Bundle as DobbyBundle
    participant SpecConfig as DobbySpecConfig
    participant Rootfs as DobbyRootfs
    participant StartState as DobbyStartState
    participant Plugins as RdkPluginManager
    participant Runtime as DobbyRunC (crun)
    participant Launcher as PluginLauncher

    Client->>DBus: StartFromSpec(id, jsonSpec, files)
    DBus->>Daemon: D-Bus method call
    Daemon->>Manager: startContainerFromSpec()

    Note over Manager: Check container not already running

    Manager->>Bundle: Create DobbyBundle (temp directory)
    Bundle-->>Manager: bundle path ready

    Manager->>SpecConfig: Parse JSON spec (DobbySpecConfig)
    SpecConfig-->>Manager: Dobby spec → OCI config

    Manager->>Rootfs: Create DobbyRootfs from config
    Note over Rootfs: Populate rootfs from spec<br/>(loop mounts, symlinks)
    Rootfs-->>Manager: rootfs ready

    Manager->>StartState: Create DobbyStartState(config, files)
    Note over StartState: Wrap passed file descriptors
    StartState-->>Manager: start state valid

    Manager->>Manager: Apply Apparmor profile (if enabled)
    Manager->>Manager: Apply pids limit (if enabled)

    alt RDK Plugins present
        Manager->>Plugins: Create DobbyRdkPluginManager
        Plugins-->>Manager: plugins loaded

        Manager->>Manager: onPostConstructionHook() [legacy]
        Manager->>Plugins: onPostInstallationHook()
        Plugins-->>Manager: hooks complete
        Manager->>Plugins: onPreCreationHook()
        Plugins-->>Manager: hooks complete
    else No RDK Plugins
        Manager->>Manager: onPostConstructionHook() [legacy]
    end

    Manager->>Manager: customiseConfig(command, displaySocket, envVars)
    Manager->>SpecConfig: writeConfigJson(bundle/config.json)
    SpecConfig-->>Manager: OCI config.json written

    alt restartOnCrash enabled
        Manager->>Manager: Store file descriptors for respawn
    end

    Manager->>Runtime: create(id, bundlePath)
    Runtime->>Launcher: OCI createRuntime hook
    Launcher->>Plugins: executeCreateRuntimeHooks()
    Plugins-->>Launcher: done
    Launcher-->>Runtime: hook exit 0
    Runtime->>Launcher: OCI createContainer hook
    Launcher->>Plugins: executeCreateContainerHooks()
    Plugins-->>Launcher: done
    Launcher-->>Runtime: hook exit 0
    Runtime-->>Manager: container created

    Manager->>Runtime: start(id)
    Runtime->>Launcher: OCI postStart hook
    Launcher->>Plugins: executePostStartHooks()
    Plugins-->>Launcher: done
    Launcher-->>Runtime: hook exit 0
    Runtime-->>Manager: container started

    Manager-->>Daemon: return container descriptor
    Daemon->>DBus: emit Started signal
    DBus-->>Client: Started event + descriptor

    Note over Manager: runcMonitorThread detects exit

    Manager->>Plugins: executePostHaltHooks()
    Plugins-->>Manager: cleanup done
    Manager->>Runtime: destroy(id)
    Runtime-->>Manager: destroyed
    Manager->>Manager: onPreDestructionHook()
    Manager-->>Daemon: container stopped
    Daemon->>DBus: emit Stopped signal
    DBus-->>Client: Stopped event
```

### Container Lifecycle State Machine

```mermaid
stateDiagram-v2
    [*] --> Starting: startContainer()
    Starting --> Running: crun create + start success
    Starting --> Stopping: create/start failure

    Running --> Stopping: stopContainer()
    Running --> Paused: pauseContainer()
    Running --> Hibernating: hibernateContainer()
    Running --> Stopping: process exit detected

    Paused --> Running: resumeContainer()
    Paused --> Stopping: stopContainer()

    Hibernating --> Hibernated: memcr checkpoint success
    Hibernating --> Stopping: checkpoint failure

    Hibernated --> Awakening: wakeupContainer()
    Hibernated --> Stopping: stopContainer()

    Awakening --> Running: memcr restore success
    Awakening --> Stopping: restore failure

    Stopping --> [*]: cleanup complete
    Stopping --> Starting: restartOnCrash=true

    note right of Running
        Container process active
        runcMonitorThread watching
    end note

    note right of Hibernated
        Process checkpointed to disk
        Container resources released
    end note
```

### Plugin Hook Execution Order

```mermaid
graph LR
    subgraph "Container Lifecycle Hooks"
        A[postInstallation] --> B[preCreation]
        B --> C[createRuntime]
        C --> D[createContainer]
        D --> E[startContainer<br/><i>optional</i>]
        E --> F[postStart]
        F --> G[postHalt]
        G --> H[postStop]
    end

    subgraph "Execution Context"
        A -.- A1[Dobby daemon<br/>host namespace]
        B -.- B1[Dobby daemon<br/>host namespace]
        C -.- C1[OCI runtime<br/>host namespace]
        D -.- D1[OCI runtime<br/>container namespace]
        E -.- E1[OCI runtime<br/>container namespace]
        F -.- F1[OCI runtime<br/>host namespace]
        G -.- G1[Dobby daemon<br/>host namespace]
        H -.- H1[OCI runtime<br/>host namespace]
    end
```

---

## Summary & Recommendations

### Strengths
1. **Complete spec coverage of all modules** — 9 specs covering daemon, client, plugins (architecture + implementations), bundle, IPC/utils, build/settings, AppInfrastructure, and test infrastructure.
2. **Excellent component-to-code traceability** — every spec has a `## Covered Code` section; ~95% of source files covered (only auto-generated schemas excluded).
3. **Well-structured specs** — all contain required Overview, Description, and Requirements sections.
4. **Detailed D-Bus protocol documentation** — interfaces, methods, and events enumerated.
5. **Architecture diagrams** — Mermaid diagrams for component relationships, data flow, lifecycle state machine, and plugin hooks.
6. **Test infrastructure documented** — L1 unit tests and L2 integration tests catalogued with mock objects and execution instructions.

### Gaps & Recommendations

| Priority | Gap | Recommendation |
|---|---|---|
| 🔴 High | No threat model | Create a security spec with threat model (STRIDE), attack surface analysis, and mitigation mapping. |
| 🟡 Medium | No versioning scheme | Document semver policy, API stability guarantees, and deprecation process. |
| 🟡 Medium | No test coverage metrics | Add lcov/gcov coverage targets and track results in CI. |
| 🟡 Medium | D-Bus API lacks parameter details | Add parameter types, return values, and error codes for each D-Bus method. |
| 🟡 Medium | No performance targets | Define measurable KPIs (container start latency, memory overhead, max concurrent containers). |
| 🟢 Low | No migration guidance | Document upgrade path from legacy plugins to RDK plugins. |
| 🟢 Low | No validation results | Track and publish test pass/fail results per release. |

---

## Score Breakdown

| Category | Score | Weight | Weighted |
|---|---|---|---|
| Code to Spec Coverage | 36.00/40 | 40% | 36.00 |
| Architecture HLA | 9.00/10 | 10% | 9.00 |
| Performance | 3.00/10 | 10% | 3.00 |
| External Interfaces | 6.50/10 | 10% | 6.50 |
| Security | 2.50/10 | 10% | 2.50 |
| Versioning & Compatibility | 2.00/10 | 10% | 2.00 |
| Conformance Testing | 5.50/10 | 10% | 5.50 |
| **Total** | | | **64.50 / 100** |
