#!/bin/bash

SKIP_BUILD=false

usage () {
  echo "$0 bVNC|freebVNC|aSPICE|freeaSPICE|aRDP|freeaRDP|Opaque"
  exit 1
}

DIR=$(dirname $0)
pushd $DIR

PRJ="$1"

if [ "$PRJ" != "bVNC" -a "$PRJ" != "freebVNC" \
  -a "$PRJ" != "aSPICE" -a "$PRJ" != "freeaSPICE" \
  -a "$PRJ" != "aRDP" -a "$PRJ" != "freeaRDP" -a "$PRJ" != "Opaque" ]
then
  usage
fi

mkdir -p remoteClientLib/java
rsync -a remoteClientLib/prebuilt/ remoteClientLib/java/

popd

echo
echo "Done copying prebuilt sqlitegen and Gstreamer files."
