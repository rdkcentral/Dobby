# Dobby Subsystem Documentation

This directory contains self-contained subsystem guides for the C/C++ code discovered in the repository. The root [README.md](../README.md) provides the project introduction and links here.

- [Daemon](daemon.md)
- [Bundle and OCI configuration](bundle.md)
- [IPC and D-Bus](ipc.md)
- [Client](client.md)
- [RDK plugin launcher](plugin-launcher.md)
- [RDK plugins](rdk-plugins.md)
- [AppInfrastructure](app-infrastructure.md)
- [Settings, utilities, tracing, and protocol](settings-utils-tracing-protocol.md)
- [Legacy plugins](plugins.md)
- [IPC utilities, runtime schema, and test infrastructure](ipc-utils-and-schema.md)

The source is platform-oriented and conditional: exact runtime behavior can differ with `USE_SYSTEMD`, `LEGACY_COMPONENTS`, plugin options, generated schemas, and target hardware. Where the inspected source was insufficient to establish an exact detail, the relevant path is named rather than inferred.
