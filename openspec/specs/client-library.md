# Client Library

## Overview
The client library provides a C++ API for applications to communicate with the Dobby daemon over D-Bus. It consists of `DobbyProxy` (the main interface) and `DobbyFactory` (daemon lifecycle management).

## Components

### IDobbyProxy (Interface)
- **File**: `AppInfrastructure/Public/Dobby/IDobbyProxy.h`
- Pure virtual interface defining all operations available to clients
- Admin: `shutdown`, `ping`, `isAlive`, `setLogMethod`, `setLogLevel`, `setAIDbusAddress`
- Control: `startContainerFromSpec`, `startContainerFromBundle`, `stopContainer`, `pauseContainer`, `resumeContainer`, `hibernateContainer`, `wakeupContainer`, `addMount`, `removeMount`, `execInContainer`, `listContainers`, `getContainerState`, `getContainerInfo`
- Events: container started/stopped/hibernated/awoken notifications via `IDobbyProxyEvents`

### DobbyProxy
- **File**: `client/lib/include/DobbyProxy.h`, `client/lib/source/DobbyProxy.cpp`
- Implements `IDobbyProxy` by translating method calls into D-Bus messages
- Registers D-Bus signal handlers for container lifecycle events (Started, Stopped, Hibernated, Awoken)
- Uses a dedicated thread (`containerStateChangeThread`) for emitting state change events to listeners
- Supports both synchronous and asynchronous D-Bus method invocations
- Uses `DobbyProxyNotifyDispatcher` for minimal-overhead event dispatch

### DobbyFactory
- **File**: `client/lib/include/DobbyFactory.h`, `client/lib/source/DobbyFactory.cpp`
- Factory pattern for obtaining a `DobbyProxy` instance
- Configurable properties: workspace path, flash mount path, platform ident/type/model
- Can start the Dobby daemon via Upstart if not already running
- Performs ping/pong health check with 60-second timeout on startup

### DobbyTool (CLI Client)
- **File**: `client/tool/source/Main.cpp`
- Interactive command-line tool for debugging containers
- Commands: `start`, `stop`, `pause`, `resume`, `hibernate`, `wakeup`, `mount`, `unmount`, `exec`, `list`, `info`, `wait`, `set-log-level`, `set-dbus`
- Connects to daemon via D-Bus using `DobbyProxy`
- Supports both legacy spec-based and bundle-based container launch
- Uses ReadLine for interactive shell interface

## Container Identification
- **File**: `AppInfrastructure/Public/Dobby/ContainerId.h`
- `ContainerId`: validated string identifier for containers
- Must match pattern: alphanumeric + underscore + hyphen, max length enforced
- Container descriptor (`int32_t`): numeric handle (1–1024) returned on start, used for subsequent operations
