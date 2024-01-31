#!/bin/bash

echo "starting memcr service"

DUMPSDIR=/media/apps/memcr
LOCATOR=/tmp/memcrcom

sudo mkdir -p ${DUMPSDIR}
sudo chmod -R 777 ${DUMPSDIR}
find ${DUMPSDIR} -mindepth 1 -exec rm {} \;
sudo ~/memcr/memcr -d ${DUMPSDIR} -N -l ${LOCATOR} -f -z -e
