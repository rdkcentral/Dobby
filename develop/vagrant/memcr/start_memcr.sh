#!/bin/bash

echo "starting memcr service"

DUMPSDIR_PERSIST=/media/apps/memcr
DUMPSDIR_TEMP=/tmp/memcr
LOCATOR=/tmp/memcrcom

sudo mkdir -p ${DUMPSDIR_PERSIST}
sudo chmod -R 777 ${DUMPSDIR_PERSIST}
sudo mkdir -p ${DUMPSDIR_TEMP}
sudo chmod -R 777 ${DUMPSDIR_TEMP}
find ${DUMPSDIR_PERSIST} -mindepth 1 -exec rm {} \;
find ${DUMPSDIR_TEMP} -mindepth 1 -exec rm {} \;
sudo LD_PRELOAD=~/memcr/libencrypt.so ~/memcr/memcr -d "${DUMPSDIR_PERSIST};${DUMPSDIR_TEMP}" -N -l ${LOCATOR} -f -z -e
