#!/bin/bash

source ./setup_env.sh
cd ${THUNDER_ROOT}

# Enable whatever plugins you wish to build here
cmake -Hrdkservices -Bbuild/rdkservices \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCOMCAST_CONFIG=OFF

make -j4 -C build/rdkservices && make -C build/rdkservices install

