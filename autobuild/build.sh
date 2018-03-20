#!/bin/bash
set -ex

yum install -y snappy snappy-devel zlib zlib-devel

export OPENSSL="openssl-1.0.2n"

pushd /tmp

wget ftp://ftp.openssl.org/source/${OPENSSL}.tar.gz && \
    tar -xzf ${OPENSSL}.tar.gz && \
    cd ${OPENSSL} && \
    ./config --prefix=/usr --openssldir=/usr/openssl threads shared && \
    make -j4 && make install
popd

pushd /io/source
git submodule update --init

pushd src
make
popd

pushd python

# hack setup.py to change relative to absolute path
cp setup.py setup.py.original
sed -i "s#\.\.#/io/source/#g" setup.py

# building wheels
for PYBIN in `ls -1d /opt/python/*/bin | grep -v cpython`; do
    # skipping python 2.7 not supported
    if [[ ${PYBIN} = *"cp27"* ]]; then
        continue;
    fi

    "${PYBIN}/pip" wheel --no-deps . -w /io/wheelhouse/
done

# bundle shared libraries
for whl in /io/wheelhouse/*.whl; do
    auditwheel repair "$whl" -w /io/wheelhouse/
done

# Restoring original setup.py
mv setup.py.original setup.py

popd

