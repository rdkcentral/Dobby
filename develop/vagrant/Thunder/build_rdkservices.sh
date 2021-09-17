#!/bin/bash

source ./setup_env.sh
cd ${THUNDER_ROOT}

cmake -Hrdkservices -Bbuild/rdkservices \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCOMCAST_CONFIG=OFF -DPLUGIN_SECURITYAGENT=ON -DPLUGIN_OCICONTAINER=ON

make -j4 -C build/rdkservices && make -C build/rdkservices install

