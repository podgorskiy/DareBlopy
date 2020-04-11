#!/usr/bin/env bash
# Script to build wheels for manylinux. This script runs inside docker.
# See build_maylinux_wheels.sh
curl -O -L http://www.nasm.us/pub/nasm/releasebuilds/2.13.01/nasm-2.13.01.tar.bz2

tar xjvf nasm-2.13.01.tar.bz2
cd nasm-2.13.01
curl -O -L https://src.fedoraproject.org/rpms/nasm/raw/0cc3eb244bd971df81a7f02bc12c5ec259e1a5d6/f/0001-Remove-invalid-pure_func-qualifiers.patch
patch include/nasmlib.h < 0001-Remove-invalid-pure_func-qualifiers.patch
./autogen.sh
./configure
make
make install
cd ..

nasm -v

cp -R /io/configs ./
cp -R /io/crc32c* ./
cp -R /io/dareblopy ./
cp -R /io/lib* ./
cp -R /io/lz4 ./
cp -R /io/protobuf ./
cp -R /io/pybind11 ./
cp -R /io/zlib ./
cp -R /io/fsal ./
cp -R /io/sources ./
cp /io/setup.py ./
cp /io/README.rst ./
cp /io/LICENSE ./

for PYBIN in /opt/python/*/bin; do
    #"${PYBIN}/pip" install -r /io/dev-requirements.txt
    #"${PYBIN}/pip" wheel /io/ -w wheelhouse/
    "${PYBIN}/python" setup.py bdist_wheel -d /io/wheelhouse/
done

# Bundle external shared libraries into the wheels
for whl in /io/wheelhouse/*.whl; do
    auditwheel show "$whl"
    auditwheel repair "$whl" --plat $PLAT -w /io/wheelhouse/
done
