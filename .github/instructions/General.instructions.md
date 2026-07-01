---
applyTo: "**/*.{cpp,h,cc,cxx,hpp},**/CMakeLists.txt,**/*.cmake,**/*.sh"
---

# Instruction Summary
1. Critical Logging
2. Recoverable Error Reporting
3. Null Safety and Pointer Style
4. Plugin Lifecycle Discipline
5. RDK Plugin Hook Contract
6. CMake and Plugin Onboarding Compliance
7. Spec and Schema Synchronization

### Critical Logging

### Requirement

Use Dobby logging macros from `AppInfrastructure/Logging/include/Logging.h` for failures:

```cpp
AI_LOG_ERROR("failed to parse config for plugin '%s'", pluginName.c_str());
```

If the function returns failure, log once with enough context and return the appropriate failure value.

### Incorrect Example

```cpp
printf("failed to parse config\n");
return false;
```

### Recoverable Error Reporting

### Requirement

Use `AI_LOG_WARN` for recoverable or fallback behavior.

```cpp
if (missingOptionalField)
{
    AI_LOG_WARN("optional field missing, using default");
}
```

Do not log expected fallback behavior as hard errors.

### Null Safety and Pointer Style

### Requirement

- Use `nullptr` instead of `NULL` in all new code.
- Preserve nearby comparison style (`nullptr == ptr` or `ptr == nullptr`) to avoid style churn inside existing files.
- Validate pointers before dereference and fail with contextual log messages.

### Plugin Lifecycle Discipline

### Requirement

For Dobby RDK plugins and daemon lifecycle-managed components:

- Keep constructors lightweight.
- Do heavy setup in lifecycle hook methods.
- Release resources in reverse order of allocation.
- Reset internal state after cleanup.

Where hook/teardown fails, log with enough context to diagnose container and plugin state.

### RDK Plugin Hook Contract

### Requirement

- Implement `IDobbyRdkPlugin` contract correctly.
- Keep `name()` stable.
- Set `hookHints()` to match only implemented hooks.
- Prefer inheriting from `RdkPluginBase` and override only needed hooks.
- Declare inter-plugin ordering requirements through `getDependencies()` when required.

### CMake and Plugin Onboarding Compliance

### Requirement

When introducing a new plugin in top-level `CMakeLists.txt`:

1. Add matching `PLUGIN_<NAME>` option and `add_subdirectory(...)`.
2. Update `.github/workflows/L1-tests.yml` plugin build flags.
3. Update `.github/workflows/L2-tests.yml` where integration coverage is needed.
4. Update `cov_build.sh` to include the plugin build flag.

This keeps CI and static analysis coverage aligned with source registration.

### Spec and Schema Synchronization

### Requirement

- Keep openspec docs in `openspec/specs/` in sync with behavior changes.
- For runtime schema changes under `bundle/runtime-schemas/`, re-run CMake so generated headers are refreshed.
- Do not merge behavior changes that leave specs stale.