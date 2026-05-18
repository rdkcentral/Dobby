# Client Library

## Overview
The client library provides a C++ API for applications to communicate with the Dobby daemon over D-Bus. It consists of `DobbyProxy` (the main interface) and `DobbyFactory` (daemon lifecycle management).

## Description
Applications interact with the Dobby daemon through the `IDobbyProxy` interface, which abstracts D-Bus communication into type-safe C++ method calls. `DobbyProxy` implements this interface by translating calls into D-Bus messages and handling asynchronous container lifecycle events. `DobbyFactory` manages daemon discovery and startup. A CLI tool (`DobbyTool`) provides an interactive debugging interface. Container identification is handled by the `ContainerId` class with validation rules.

### IDobbyProxy (Interface)
- **File**: `AppInfrastructure/Public/Dobby/IDobbyProxy.h`
- Pure virtual interface defining all operations available to clients
- Admin: `shutdown`, `ping`, `isAlive`, `setLogMethod`, `setLogLevel`, `setAIDbusAddress`
- Control: `startContainerFromSpec`, `startContainerFromBundle`, `stopContainer`, `pauseContainer`, `resumeContainer`, `hibernateContainer`, `wakeupContainer`, `addMount`, `removeMount`, `execInContainer`, `listContainers`, `getContainerState`, `getContainerInfo`
- Events: container started/stopped/hibernated/awoken notifications via `IDobbyProxyEvents`

### DobbyProxy
- Implements `IDobbyProxy` by translating method calls into D-Bus messages
- Registers D-Bus signal handlers for container lifecycle events (Started, Stopped, Hibernated, Awoken)
- Uses a dedicated thread (`containerStateChangeThread`) for emitting state change events to listeners
- Supports both synchronous and asynchronous D-Bus method invocations
- Uses `DobbyProxyNotifyDispatcher` for minimal-overhead event dispatch

### DobbyFactory
- Factory pattern for obtaining a `DobbyProxy` instance
- Configurable properties: workspace path, flash mount path, platform ident/type/model
- Can start the Dobby daemon via Upstart if not already running
- Performs ping/pong health check with 60-second timeout on startup

### DobbyTool (CLI Client)
- Interactive command-line tool for debugging containers
- Commands: `start`, `stop`, `pause`, `resume`, `hibernate`, `wakeup`, `mount`, `unmount`, `exec`, `list`, `info`, `wait`, `set-log-level`, `set-dbus`
- Connects to daemon via D-Bus using `DobbyProxy`
- Supports both legacy spec-based and bundle-based container launch
- Uses ReadLine for interactive shell interface

### Container Identification
- **File**: `AppInfrastructure/Public/Dobby/ContainerId.h`
- `ContainerId`: validated string identifier for containers
- Must match pattern: alphanumeric + underscore + hyphen, max length enforced
- Container descriptor (`int32_t`): numeric handle (1–1024) returned on start, used for subsequent operations

## Requirements
- D-Bus must be available and the Dobby daemon must be running (or startable via Upstart).
- Applications must link against the Dobby client library.
- Container IDs must conform to the alphanumeric + underscore + hyphen pattern.

## Architecture / Design
- Client applications use `DobbyFactory` to obtain a `DobbyProxy` instance.
- `DobbyProxy` communicates with the daemon exclusively via D-Bus (method calls + signal subscriptions).
- A dedicated event thread decouples container state change notifications from the D-Bus dispatch thread.

## External Interfaces
- **IDobbyProxy API**: C++ interface for container lifecycle management.
- **D-Bus**: Communication transport between client and daemon (`org.rdk.dobby`).

## Performance
_Not applicable — the client library is a thin D-Bus wrapper with minimal overhead._

## Security
_Not applicable — security is enforced at the daemon level via D-Bus policy._

## Versioning & Compatibility
_Not applicable — the client library versioning follows the overall Dobby project version._

## Conformance Testing & Validation
_Not applicable — tested as part of overall Dobby L1/L2 test suites._

## Covered Code
- client/lib/include/DobbyProxy.h
- client/lib/include/DobbyFactory.h
- client/lib/source/DobbyProxy.cpp
- client/lib/source/DobbyFactory.cpp
- client/lib/source/Upstart.h
- client/lib/source/Upstart.cpp
- client/tool/source/Main.cpp
- AppInfrastructure/Public/Dobby/IDobbyProxy.h
- AppInfrastructure/Public/Dobby/ContainerId.h

---

## Open Queries
_No open queries._

## References
- D-Bus protocol defined in `protocol/include/DobbyProtocol.h`

## Change History
- 2025-05-18 - openspec-templater - Restructured to match spec template.
