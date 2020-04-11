#!/usr/bin/env bash
# Script to build wheels for manylinux. This script executes docker.sh inside docker

docker run -it --rm -e PLAT=manylinux2010_x86_64 -v `pwd`:/io quay.io/pypa/manylinux2010_x86_64 sh /io/scripts/docker.sh
