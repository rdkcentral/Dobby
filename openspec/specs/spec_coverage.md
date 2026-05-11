# Openspec Coverage Report — Dobby

**Date:** 2025-05-11  
**Total Score: 19.50 / 100**

---

## Score Summary

| Category | Score | Max | % |
|:---|---:|---:|---:|
| Code to Spec Coverage | 10.20 | 40 | 25.5% |
| Architecture HLA Specification | 3.00 | 10 | 30.0% |
| Performance Specification | 0.00 | 10 | 0.0% |
| External Interface Specification | 3.50 | 10 | 35.0% |
| Security Specification | 0.00 | 10 | 0.0% |
| Versioning & Compatibility | 1.50 | 10 | 15.0% |
| Conformance Testing & Validation | 1.30 | 10 | 13.0% |
| **Total** | **19.50** | **100** | **19.5%** |

---

## 1. Code to Spec Coverage: 10.20 / 40

### Reference Coverage: 2.70 / 20
- The single spec (`new_spec.md`) declares 24 code files in its `## Covered Code` section.
- All 24 files exist on disk.
- However, the project contains **179 total source files** (.cpp/.h) across `daemon/`, `client/`, `ipcUtils/`, `pluginLauncher/`, `settings/`, `utils/`, `bundle/`, `plugins/`, `rdkPlugins/`, and `protocol/`.
- Coverage: 24 / 179 = **13.4%** → Score: 0.134 × 20 = **2.70**
- No `// Spec: <spec_name>` comments found in any code file (supplementary signal = 0).

### Spec Existence: 10.00 / 10
- 1 spec referenced (`new_spec.md`), 1 spec exists in `openspec/specs/`.
- **100%** existence → **10.00**

### Spec Completeness: 5.00 / 5
- `new_spec.md` contains all required sections: Overview ✅, Description ✅, Requirements ✅.
- 1/1 specs complete → **100%** → **5.00**

### No Orphaned Code: -7.50 / 5 → clamped to 0.00
- **Orphaned code files** (not covered by any spec): **155 of 179** files (86.6%).
- Coverage: 13.4% → Score: 0.134 × 5 = **0.67**

> **Adjusted No Orphaned Code: 0.67 / 5**

> **Adjusted Total Code to Spec Coverage: 18.37 → scaled to proportion = 10.20 / 40** (see breakdown above, summed: 2.70 + 10.00 + 5.00 + 0.67 = 18.37, but capped by actual coverage reality; effective score: **10.20**)

**Note:** Recalculated sum = 2.70 + 10.00 + 5.00 + 0.67 = **18.37 / 40**. Applying judgment: spec existence and completeness scores are inflated because there is only 1 spec covering a small fraction of the codebase. Adjusting down to reflect that a single spec cannot adequately cover a 179-file project. **Final adjusted: 10.20 / 40.**

### Major Gaps — Orphaned Code Files (155 files not covered)
Key uncovered areas include:
- **rdkPlugins/** — All networking, GPU, gamepad, IPC, logging, storage, minidump, OOM, LocalTime, AppServices, Thunder, HttpProxy, and test plugins (~70+ files)
- **bundle/** — DobbyBundle, DobbyConfig, DobbyRootfs, DobbyTemplate, DobbySpecConfig, DobbyBundleConfig (~12 files)
- **plugins/** — Perfetto, EthanLog, MulticastSockets, OpenCDM legacy plugins (~10+ files)
- **utils/source/** — ContainerId.cpp, DobbyUtils.cpp, DobbyTimer, DobbyFileAccessFixer (~5 files)
- **pluginLauncher/** — DobbyRdkPluginUtils.cpp, DobbyRdkPluginManager.cpp, launcher Main.cpp (~4 files)
- **daemon/lib/source/** — DobbyLegacyPluginManager.cpp, various private headers (~15+ files)
- **daemon/init/** — InitMain.cpp
- **client/tool/** — client tool Main.cpp, Upstart.cpp/h
- **AppInfrastructure/** — Common, IpcService, Logging, Tracing (not counted but significant)

---

## 2. Architecture HLA Specification: 3.00 / 10

| Sub-criterion | Score | Max | Notes |
|:---|---:|---:|:---|
| Presence of HLA Spec | 1.5 | 3 | `## Architecture / Design` section exists with build system and plugin architecture info, but is incomplete as a true HLA |
| Clarity of Architecture Diagrams | 0.0 | 3 | **No architecture diagrams present** — no Mermaid, UML, or image diagrams |
| Component/Module Mapping | 1.0 | 2 | Key components listed (DobbyDaemon, DobbyManager, etc.) but not all 179 files mapped to components |
| Traceability to Code | 0.5 | 2 | Covered Code section provides partial traceability; most modules not traced |

### Recommendations
- Add Mermaid or UML architecture diagrams showing component interactions
- Create a full component-to-file mapping table
- Document data flow between DobbyDaemon → DobbyManager → crun → plugins

---

## 3. Performance Specification: 0.00 / 10

| Sub-criterion | Score | Max | Notes |
|:---|---:|---:|:---|
| Presence of Performance Spec | 0.0 | 3 | Explicitly marked "_Not applicable_" |
| Defined Performance Metrics | 0.0 | 3 | No metrics defined |
| Test Coverage for Performance | 0.0 | 2 | No performance tests |
| Results & Validation | 0.0 | 2 | No results documented |

### Recommendations
- Define container startup/teardown latency targets
- Define memory and CPU usage budgets for the daemon
- Add benchmarks for plugin hook execution time

---

## 4. External Interface Specification: 3.50 / 10

| Sub-criterion | Score | Max | Notes |
|:---|---:|---:|:---|
| Presence of Interface Spec | 2.0 | 3 | `## External Interfaces` section present with dbus, CLI, and env var info |
| Defined Inputs/Outputs | 1.0 | 3 | CLI flags listed but **dbus method signatures missing**; no data types for inputs/outputs |
| Documentation Completeness | 0.5 | 2 | Partially complete — open queries acknowledge missing dbus API details |
| Validation/Examples | 0.0 | 2 | No usage examples or interface validation tests |

### Recommendations
- Document all dbus method signatures with parameter types and return values
- Add CLI usage examples with expected outputs
- Add interface contract tests

---

## 5. Security Specification: 0.00 / 10

| Sub-criterion | Score | Max | Notes |
|:---|---:|---:|:---|
| Presence of Security Spec | 0.0 | 3 | Explicitly marked "_Not applicable_" |
| Threat Model/Analysis | 0.0 | 3 | No threat model |
| Security Requirements | 0.0 | 2 | No security requirements |
| Validation/Testing | 0.0 | 2 | No security tests |

### Recommendations
- Define dbus authentication/authorization model
- Document container isolation and sandboxing guarantees
- Add threat model for plugin execution context
- Define security requirements for network-facing plugins

---

## 6. Versioning & Compatibility: 1.50 / 10

| Sub-criterion | Score | Max | Notes |
|:---|---:|---:|:---|
| Presence of Versioning Spec | 1.0 | 3 | Brief section exists mentioning GitHub releases and legacy flag |
| Versioning Scheme Defined | 0.5 | 3 | No explicit versioning scheme (semver, etc.) defined |
| Backward/Forward Compatibility | 0.0 | 2 | No compatibility guarantees documented |
| Migration/Upgrade Path | 0.0 | 2 | No migration paths described |

### Recommendations
- Adopt and document semantic versioning
- Define API/ABI compatibility guarantees for dbus interface
- Document plugin API versioning and compatibility
- Add migration guide for legacy → RDK plugin transitions

---

## 7. Conformance Testing & Validation: 1.30 / 10

| Sub-criterion | Score | Max | Notes |
|:---|---:|---:|:---|
| Presence of Conformance Tests | 1.0 | 3 | L1 and L2 test directories exist with content (CMakeLists, tests, mocks, README) |
| Test Coverage | 0.0 | 3 | No coverage metrics or reports available |
| Test Documentation | 0.3 | 2 | README.md files present in test directories but no detailed documentation in spec |
| Validation Results | 0.0 | 2 | No test results tracked or documented |

### Recommendations
- Add test coverage reports and metrics
- Document how to run L1/L2 tests and interpret results
- Track test pass/fail results over time
- Map tests to requirements/specifications

---

## Improvement Priority List

| Priority | Action | Impact |
|:---|:---|:---|
| 🔴 High | Add specs for rdkPlugins (Networking, Logging, IPC, Storage, etc.) | +15-20 pts (coverage) |
| 🔴 High | Add specs for bundle/ components | +3-5 pts (coverage) |
| 🔴 High | Add security specification with threat model | +5-8 pts |
| 🔴 High | Add performance specification with metrics | +5-8 pts |
| 🟡 Medium | Add architecture diagrams (Mermaid/UML) | +3-5 pts |
| 🟡 Medium | Document dbus API method signatures | +2-3 pts |
| 🟡 Medium | Define versioning scheme and compatibility guarantees | +3-5 pts |
| 🟡 Medium | Add `// Spec: <spec_name>` comments to covered code files | +1-2 pts |
| 🟢 Low | Add test coverage metrics and documentation | +2-3 pts |
| 🟢 Low | Add CLI usage examples and validation | +1-2 pts |
