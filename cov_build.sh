#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
git config --global --add safe.directory "${GITHUB_WORKSPACE}"
git submodule update --init --recursive

############################
# Build dobby
echo "======================================================================================"
echo "building dobby"

COVERITY_EXCLUDE_ARGS=()
for coverity_path in \
    "${GITHUB_WORKSPACE}/tests" \
    "${GITHUB_WORKSPACE}/libocispec" \
    "${GITHUB_WORKSPACE}/develop" \
    "${GITHUB_WORKSPACE}/openspec"; do
    if [ -d "${coverity_path}" ]; then
        COVERITY_EXCLUDE_ARGS+=("--exclude-path" "${coverity_path}")
    fi
done

cd $GITHUB_WORKSPACE
mkdir build
cd build
cmake -DRDK_PLATFORM=DEV_VM -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_BUILD_TYPE=Debug -DLEGACY_COMPONENTS=ON -DPLUGIN_TESTPLUGIN=ON -DPLUGIN_GPU=ON -DPLUGIN_LOCALTIME=ON -DPLUGIN_RTSCHEDULING=ON -DPLUGIN_HTTPPROXY=ON -DPLUGIN_APPSERVICES=ON -DPLUGIN_IONMEMORY=ON -DPLUGIN_DEVICEMAPPER=ON -DPLUGIN_OOMCRASH=ON -DLEGACY_COMPONENTS=ON -DRDK=ON -DUSE_SYSTEMD=ON -DDOBBY_HIBERNATE_MEMCR_IMPL=ON -DDOBBY_HIBERNATE_MEMCR_PARAMS_ENABLED=ON ..
if command -v cov-build >/dev/null 2>&1; then
    cov-build --dir "${GITHUB_WORKSPACE}/build/cov-int" "${COVERITY_EXCLUDE_ARGS[@]}" -- make -j $(nproc)
else
    make -j $(nproc)
fi

