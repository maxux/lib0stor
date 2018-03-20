#!/bin/bash
set -x

if [ ! -d src ] || [ ! -d python ]; then
	echo "This should should be run from repository root"
	echo "  ./autobuild/run.sh"
	exit 1
fi

mkdir -p ./autobuild/wheelhouse
docker run --rm -it \
  -v $(pwd)/:/io/source \
  -v $(pwd)/autobuild/wheelhouse:/io/wheelhouse \
  quay.io/pypa/manylinux1_x86_64 \
  bash /io/source/autobuild/build.sh
