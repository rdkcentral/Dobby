#!/bin/bash
set -e
set -x
#############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
cd ${GITHUB_WORKSPACE}

############################# 
# Install Dependencies and packages

apt update
apt-get install -q -y cmake libsystemd-dev libctemplate-dev libjsoncpp-dev libdbus-1-dev libnl-3-dev libnl-route-3-dev libyajl-dev libcap-dev libboost-dev clang libyaml-dev
############################
