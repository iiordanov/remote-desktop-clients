#!/bin/bash

DIR=$(dirname $0)
pushd $DIR

mkdir -p remoteClientLib/java
rsync -a remoteClientLib/prebuilt/ remoteClientLib/java/

popd

echo
echo "Done copying prebuilt files."
