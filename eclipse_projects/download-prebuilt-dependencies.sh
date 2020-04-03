#!/bin/bash

DIR=$(dirname $0)
pushd $DIR

DEPVER=10

wget -c https://github.com/iiordanov/remote-desktop-clients/releases/download/dependencies/remote-desktop-clients-libs-${DEPVER}.tar.gz

mkdir -p remoteClientLib/jni/libs/deps/FreeRDP/
mkdir -p EXTRACT/

tar xf remote-desktop-clients-libs-${DEPVER}.tar.gz -C EXTRACT/
rsync -avP EXTRACT/ ../
rm -rf EXTRACT/

echo "Done downloading and extracting dependencies."

popd
