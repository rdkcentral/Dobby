# Dobby & Thunder Vagrant Environment
## Introduction
This repo contains a Vagrant (https://www.vagrantup.com/) VirtualBox VM that can contains all the necessary dependencies to run and develop Dobby

The Vagrant configuration will automatically install all the dependencies required for Dobby. It will then clone the `dobby` repo from GitHub and build/install Dobby. Thunder and the OCIContainer plugin from [RDKServices](https://github.com/rdkcentral/rdkservices) will also be built and installed for testing. This allows for Dobby to be controlled both from DobbyTool and the OCIContainer JSON-RPC interface.

You could manually run the commands that are inside the Vagrantfile yourself, but Vagrant provides a reproducible environment for testing. Vagrant works on Windows, Mac and various Linux distributions.


## Getting Started
Start by installing Vagrant on your host machine by downloading the relevant version for your OS here: https://www.vagrantup.com/downloads.html.

You must also have VirtualBox installed on your system. Vagrant works with all versions of VirtualBox > 4.0. Download and install the relevant VirtualBox binary from here: https://www.virtualbox.org/wiki/Downloads

You must have SSH keys configured on your host machine such that you can pull code from GitHub

If you wish to clone/build Dobby from your own fork, change the `git clone` URL in the `Clone DOBBY repo and install` section of the Vagrantfile.

Once installed, run the following commands:

```
cd <dobby_repo>/develop/vagrant
vagrant up
```
You will be prompted to choose a network adaptor to bridge into the VM - pick your main network adaptor you use to connect to the internet. Now wait for Vagrant to build the VM - this can take some time so go grab a cup of coffee!

Once Vagrant has built the VM, you can SSH into it by running

```
vagrant ssh
```

You will be greeted with a standard Ubuntu Bash prompt.

You can stop the VM at any time by running `vagrant halt`. When you next start the VM with `vagrant up` it will be much faster as Vagrant won't have to recreate the entire VM. Any changes you made to the VM will still be there. See: https://www.vagrantup.com/intro/getting-started/teardown for more info.

The contents of this directory will be mounted inside the VM in the `/vagrant` directory - this is useful to easily copy files in and out of the VM

### Troubleshooting
If Vagrant complains about `Unknown configuration section 'disksize'`, manually install the disksize plugin with
```
vagrant plugin install vagrant-disksize
```

## Dobby Instructions
### Source Code

The Dobby source code will be cloned to `~/srcDobby` and can be viewed/developed from here. Vagrant will built this code during the VM build, but the code can be rebuilt with the standard CMake workflow:

```
cd ~/srcDobby/build

cmake -DCMAKE_BUILD_TYPE=Debug -DRDK_PLATFORM=DEV_VM ../

sudo make install -j6
```

There are two key components of Dobby that are used for testing - `DobbyDaemon` and `DobbyTool`. `DobbyDaemon` is the main Dobby engine and is responsible for the creation and management of containers. `DobbyTool` is a debugging tool that allows manually creating, inspecting and listing containers. The daemon must be running for the DobbyTool to work.

### Basic Test

Included in the Dobby repo are a number of OCI bundles and (legacy) Dobby spec files for testing. These can be found in the `~/srcDobby/tests/test_runner/bundle` directory on the VM. The bundles will need extracting as they're compressed as tar.gz files.

To launch a container using DobbyTool:

1. Open two SSH sessions

2. In one terminal, run

   `sudo DobbyDaemon -v --nofork`

   to launch the Dobby daemon process.

3. In the other terminal, use `DobbyTool` to launch the "Sleepy" container:

    ```
    $ cd ~
    $ tar -xzvf ~/srcDobby/tests/test_runner/bundle/sleepy_bundle.tar.gz -C .
    $ DobbyTool start sleepy ~/sleepy_bundle
    ```

4. DobbyTool should return `started 'sleepy' container, descriptor is xxx`.

5. Confirm the container is running by executing `DobbyTool list`:

```
 | descriptor | id     | state   |
 | ---------- | ------ | ------- |
 | xxx        | sleepy | running |
```

See the Dobby `README.md` file for more information on the available tools and commands.

## Thunder & OCIContainer

Thunder and the `OCIContainer` NanoService are both installed on the VM. To start Thunder, run `/usr/local/bin/WPEFramework`. You can then issue commands to OCIContainer over curl as per the OCIContainer README.md file. Thunder is configured to listen on port `9998`.

It is possible to access Thunder from the host machine if you enable port forwarding in the Vagrantfile as per the documentation here: https://www.vagrantup.com/docs/networking/forwarded_ports.html


## Known Issues

* DobbyDaemon sometimes fails to start with error `failed to create veth pair ('veth0' : 'enp0s3') (12 - Object not found)`. Rebooting the VM (`vagrant halt && vagrant up`) should fix this.
