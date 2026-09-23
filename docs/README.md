# Dobby Subsystem Documentation

This directory contains self-contained subsystem guides for the C/C++ code discovered in the repository. The root [README.md](../README.md) provides the project introduction and links here.

- [Daemon](../daemon/README.md)
- [Bundle and OCI configuration](../bundle/README.md)
- [IPC and D-Bus](../AppInfrastructure/IpcService/README.md)
- [Client](../client/README.md)
- [RDK plugin launcher](../pluginLauncher/README.md)
- [RDK plugins](../rdkPlugins/README.md)
- [AppInfrastructure](../AppInfrastructure/README.md)
- [Settings, utilities, tracing, and protocol](../settings/README.md)
- [Legacy plugins](../plugins/README.md)
- [IPC utilities, runtime schema, and test infrastructure](../ipcUtils/README.md)

The source is platform-oriented and conditional: exact runtime behavior can differ with `USE_SYSTEMD`, `LEGACY_COMPONENTS`, plugin options, generated schemas, and target hardware. Where the inspected source was insufficient to establish an exact detail, the relevant path is named rather than inferred.
