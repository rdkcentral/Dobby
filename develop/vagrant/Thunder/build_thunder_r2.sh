#!/bin/bash

source ./setup_env.sh
cd ${THUNDER_ROOT}

# Thunder Tools
cmake -HThunder/Tools -Bbuild/ThunderTools -DCMAKE_INSTALL_PREFIX=/usr

make -j4 -C build/ThunderTools && make -C build/ThunderTools install

# Thunder Core
cmake -HThunder -Bbuild/Thunder -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TYPE=Debug -DBINDING=127.0.0.1 -DPORT=9998

make -j4 -C build/Thunder && make -C build/Thunder install

# Thunder Interfaces
cmake -HThunderInterfaces -Bbuild/ThunderInterfaces  -DCMAKE_INSTALL_PREFIX=/usr
make -j4 -C build/ThunderInterfaces && make -C build/ThunderInterfaces install

# Thunder Client Libs (enable the libraries you need)
cmake -HThunderClientLibraries -Bbuild/ThunderClientLibraries \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVIRTUALINPUT=ON

make -j4 -C build/ThunderClientLibraries && make -C build/ThunderClientLibraries install
