#!/bin/bash
set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
git config --global --add safe.directory "${GITHUB_WORKSPACE}"
git submodule update --init --recursive

# Exclude non-product directories from the build source tree so the listed folders
# are not compiled when this script is used by another team for scan prep. Keep
# the build rooted outside the repo checkout to avoid CMake generating include
# paths that point back into the original source directory.
COV_BUILD_ROOT="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"
COV_BUILD_SOURCE="${COV_BUILD_ROOT}/dobby-cov"
rm -rf "${COV_BUILD_SOURCE}"
mkdir -p "${COV_BUILD_SOURCE}"

echo "DEBUG: repo root: ${GITHUB_WORKSPACE}"
echo "DEBUG: using external filtered source tree for scan prep: ${COV_BUILD_SOURCE}"
find "${GITHUB_WORKSPACE}" -mindepth 1 -maxdepth 1 \
    ! -name '.git' \
    ! -name '.cov_build_source' \
    ! -name 'build' \
    ! -name 'tests' \
    ! -name 'libocispec' \
    ! -name 'develop' \
    ! -name 'openspec' \
    -exec cp -a {} "${COV_BUILD_SOURCE}/" \;

echo "DEBUG: excluded directories: .git .cov_build_source build tests libocispec develop openspec; keeping install for dependency resolution"

############################
# Build dobby
echo "======================================================================================"
echo "building dobby"

cd "${COV_BUILD_SOURCE}"
mkdir -p build
cd build
cmake -DRDK_PLATFORM=DEV_VM -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_BUILD_TYPE=Debug -DLEGACY_COMPONENTS=ON -DPLUGIN_TESTPLUGIN=ON -DPLUGIN_GPU=ON -DPLUGIN_LOCALTIME=ON -DPLUGIN_RTSCHEDULING=ON -DPLUGIN_HTTPPROXY=ON -DPLUGIN_APPSERVICES=ON -DPLUGIN_IONMEMORY=ON -DPLUGIN_DEVICEMAPPER=ON -DPLUGIN_OOMCRASH=ON -DLEGACY_COMPONENTS=ON -DRDK=ON -DUSE_SYSTEMD=ON -DDOBBY_HIBERNATE_MEMCR_IMPL=ON -DDOBBY_HIBERNATE_MEMCR_PARAMS_ENABLED=ON ..
make -j $(nproc)

