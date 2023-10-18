# Dobby
![GitHub release (with filter)](https://img.shields.io/github/v/release/rdkcentral/Dobby)![GitHub](https://img.shields.io/github/license/rdkcentral/Dobby)![Github](https://img.shields.io/github/issues-pr-raw/rdkcentral/Dobby)![Github](https://img.shields.io/github/issues/rdkcentral/Dobby)
  

Dobby “Docker based Thingy” is a tool for managing and running OCI containers using [crun](https://github.com/containers/crun)

# Getting Started
## Requirements
Dobby has the following dependencies

* CMake (>3.7)
* crun (>=0.13)
* jsoncpp
* yajl 2 (for libocispec)
* ctemplate (if using `LEGACY_COMPONENTS`)
* libsystemd
* libnl (if using Networking plugin)
* libnl-route (if using Networking plugin)
* libdbus
* boost (1.61)

## Build
Dobby is a CMake project and can be built with the standard CMake workflow. To build, run the following commands.

```
cd /location/of/dobby/repo
mkdir build && cd build

cmake -DCMAKE_BUILD_TYPE=Debug ../

make -j$(numproc)
sudo make install
```

During the CMake configure stage, CMake will also configure the `libocispec` submodule. This is used to generate the necessary C headers for parsing and manipulating OCI bundle specifications.

If the schema files in the `bundle/runtime-schemas` directory are changed, then you will need to re-run CMake again to regenerate the headers.

When building for the development VM, use the following CMake command:
```
cmake -DCMAKE_BUILD_TYPE=Debug -DRDK_PLATFORM=DEV_VM -DCMAKE_INSTALL_PREFIX:PATH=/usr ../
```

### CMake Configuration Settings
| Option                      | Valid Options                                        | Description                                                                                                                                                                                                                                                         |
| :-------------------------- | :--------------------------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `-CMAKE_BUILD_TYPE`         | Debug/Release                                        | Build the Debug or Release version                                                                                                                                                                                                                                  |
| `-DENABLE_DOBBYTOOL`        | ON/OFF                                               | Include DobbyTool in the build. For Debug builds, defaults to ON. For Release builds, defaults to OFF.                                                                                                                                                              |
| `-DPLUGIN_PATH`             | Valid UNIX path                                      | Specify a different location to load RDK plugins from. Defaults to `/usr/lib/plugins/dobby` if not set                                                                                                                                                              |
| `-DRDK_PLATFORM`            | `GENERIC`<br/> `DEV_VM` | Specify which platform Dobby is running on. Defaults to `GENERIC` if none specified. DEV_VM changes some settings suitable for Vagrant |
| `-DSETTINGS_FILE`             | Valid UNIX path                                               |Path to the settings JSON file to use for Dobby (will be installed to /etc/dobby.json)                                                                                                                           |
| `-DSETTINGS_APPEND`             | Valid UNIX path                                               |Settings to append to the base settings file as defined in `-DSETTINGS_FILE`                                                                                                                           |
| `-DLEGACY_COMPONENTS`       | ON/OFF                                               | Enable or disable legacy components (legacy plugins, Dobby specs, ...). Defaults to OFF                                                                                                                                                                             |
| `-DENABLE_LTO`              | ON/OFF                                               | Enable Link Time Optimisation (https://gcc.gnu.org/onlinedocs/gccint/LTO-Overview.html). Requires GCC >4,5 although versions >6 are strongly recommended. Defaults to `OFF`                                                                                         |
| `-DENABLE_PERFETTO_TRACING` | ON/OFF                                               | Option to enable or disable Perfetto tracing. Can not be enabled for release builds. Requires Perfetto SDK to be installed, and Legacy Components enabled                                                                                                           |
| `-DDOBBY_SERVICE`           | Reverse domain name string                           | Specify the Dobby dbus service name. Defaults to `org.rdk.dobby` if none specified.                                                                                                                                                                                 |
| `-DDDOBBY_OBJECT`           | Valid UNIX path                                      | Specify the Dobby dbus object path. Defaults to `/org/rdk/dobby` if none specified.                                                                                                                                                                                 |
| `-DUSE_STARTCONTAINER_HOOK` | ON/OFF                                               | Whether to use the startcontainer OCI hook or not. Defaults to OFF.                                                                                                                                                                                                 |
| `-DUSE_SYSTEMD`             | ON/OFF                                               | Build with systemd support (recommended). When disabled, uses libdbus API instead of sd-bus. Defaults to ON.                                                                                                                                                        |
| `-DEXTERNAL_PLUGIN_SCHEMA`  | Valid file path(s)                                   | Path(s) to external json schema definitions for extra rdk plugins. Paths should be seperated by `;`.                                                                                                                                                                |


#### Enable/Disable Plugins
In addition to all the above, each RDK plugin has a setting for enabling it for builds. The `Logging`, `Networking`, `IPC`, `Storage` and `Minidump` plugins are enabled by default.

Use `-DPLUGIN_[PLUGINNAME]=[ON|OFF]` to enable or disable plugins for your build.

# Development
If you with to develop Dobby further, detailed instructions on setting up a development environment can be found in the `develop` directory in this repo, including a Vagrant VM with all the necessary dependencies pre-installed.

# Documentation
* A high level overview of Dobby can be found at RDKCentral [here](https://wiki.rdkcentral.com/display/ASP/Dobby)
* Code documentation can be generated with Doxygen by running `doxygen ./Doxyfile`
* Doxygen documentation is hosted here: https://rdkcentral.github.io/Dobby/

# Usage
## DobbyDaemon
This is the main component of Dobby. This daemon is responsible for managing, controlling and creating containers. The daemon runs in the background and communicates over dbus. It connects on a few dbus addresses - one for admin, one for debugging and the other for Dobby commands.

DobbyDaemon can be started as a systemd service at system boot (the systemd service file can be found at `daemon/process/dobby.service`) or run manually on the command line by running:

```
DobbyDaemon --nofork
```

Additional `-v` flags can be passed to the daemon to increase it's log verbosity for troubleshooting as needed

```
Usage: DobbyDaemon <option(s)>
  Daemon that starts / stops / manages containers.

  -h, --help                    Print this help and exit
  -v, --verbose                 Increase the log level
  -V, --version                 Display this program's version number

  -f, --settings-file=PATH      Path to a JSON dobby settings file [/etc/dobby.json]
  -a, --dbus-address=ADDRESS    The dbus address to put the admin service on [system bus]
  -p, --priority=PRIORITY       Sets the SCHED_RR priority of the daemon [RR,12]
  -n, --nofork                  Do not fork and daemonise the process
  -k, --noconsole               Disable console output
  -g, --syslog                  Send all initial logging to syslog rather than the console
  -j, --journald                Enables logging to journald

  Besides the above options the daemon checks for the follow environment variables

  AI_WORKSPACE_PATH=<PATH>      The path to tmpfs dir to use as workspace
  AI_PERSISTENT_PATH=<PATH>     The path to dir that is persistent across boots
  AI_PLATFORM_IDENT=<IDENT>     The 4 characters than make up the STB platform id
```

## DobbyBundleGenerator
This is a command-line wrapper around the `DobbyBundleGen` library, and allows Dobby spec JSON files to be converted to extended OCI bundles (aka Bundle*'s) with RDK plugin sections. It does not require the DobbyDaemon to be running, so can be run on or off-box.

```
Usage: DobbyBundleGenerator <option(s)>
  Tool to convert Dobby JSON spec to OCI bundle without needing a running Dobby Daemon

  -h, --help                    Print this help and exit
  -v, --verbose                 Increase the log level

  -s, --settings=PATH           Path to Dobby Settings file for STB
  -i, --inputpath=PATH          Path to Dobby JSON Spec for container
  -o, --outputDirectory=PATH    Where to save the generated OCI bundle
```

## DobbyTool
This is a simple command line tool that is used for debugging purporses. It connects to the Dobby daemon over dbus and allows for debugging and testing containers.

```
vagrant@dobby-vagrant:~$ DobbyTool help
quit              quit
help              help [command]
shutdown          shutdown
start             start [options...] <id> <specfile/bundlepath> [command]
stop              stop <id> [options...]
pause             pause <id>
resume            resume <id>
exec              exec [options...] <id> <command>
list              list
info              info <id>
dumpspec          dumpspec <id> [options...]
bundle            bundle <id> <specfile> [options...]
set-dbus          set-dbus <private>|<public> <address>
```
For more information about a command, run `DobbyTool help [command]`. For example:
```
$ DobbyTool help start
start             start [options...] <id> <specfile/bundlepath> [command]

Starts a container using the given spec file or bundle path. Can optionally specify the command to run inside the container. Any arguments after command are treated as arguments to the command.
```

## DobbyPluginLauncher
This is designed to be run from the OCI runtime to execute Dobby RDK plugins. It loads plugins from the `/usr/lib/plugins/dobby` directory (configurable at build-time), and runs the appropriate methods based on the specified hook and plugin data.

```
Usage: DobbyPluginLauncher <option(s)>
  Tool to run Dobby plugins loaded from /usr/lib/plugins/dobby

  -H, --help                    Print this help and exit
  -v, --verbose                 Increase the log level

  -h, --hook                    Specify the hook to run
  -c, --config=PATH             Path to container OCI config
```

To use Dobby RDK plugins in a container launched via `DobbyDaemon`, the OCI bundle config should have `ociVersion` set to `1.0.2-dobby` and an `rdkPlugins` object with plugins present. `DobbyDaemon` will set up Dobby and OCI hooks to execute the plugins' hookpoints with `DobbyPluginLauncher`.

An example of the `rdkPlugins` object's syntax with the `networking` plugin:

```javascript
{
   "rdkPlugins":{
      "networking":{        // plugin name
         "required":true,   // whether to throw an error if the plugin is missing, or just flag a warning and carry on
         "data":{           // data to be passed to the plugin - can contain any valid JSON, depending on the plugin
            "type":"nat"    // each plugin has its own accepted data structure
         }
      }
   }
}
```

## DobbyInit
This tool is a simple 'init' process for containers. Instead of relying on runc to manage the lifecycle of processes within the container, DobbyInit is responsible for reaping adopted child processes and forwarding signals to child processes.
The motivation for this tool is described here: https://blog.phusion.nl/2015/01/20/docker-and-the-pid-1-zombie-reaping-problem. It is installed in `/usr/libexec`.

```
Usage: DobbyInit <process-to-run> <arg1> <arg2> ... <argN>
```

---
# Copyright and license

If not stated otherwise in this file or this component's LICENSE file the following copyright and licenses apply:

Copyright 2020 Sky UK

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

## Note
Dobby makes use of the [libocispec](https://github.com/containers/libocispec) library to parse and generate OCI configurations from JSON schemas. This is a code-generator and used only at build-time. `libocispec` is GPLv3 licensed but includes a specific exception for this purpose.
