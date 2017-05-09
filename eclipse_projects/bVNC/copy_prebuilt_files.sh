#!/bin/bash

SKIP_BUILD=false

usage () {
  echo "$0 bVNC|freebVNC|aSPICE|freeaSPICE|aRDP|freeaRDP"
  exit 1
}

DIR=$(dirname $0)
pushd $DIR

PRJ="$1"

if [ "$PRJ" != "bVNC" -a "$PRJ" != "freebVNC" \
  -a "$PRJ" != "aSPICE" -a "$PRJ" != "freeaSPICE" \
  -a "$PRJ" != "aRDP" -a "$PRJ" != "freeaRDP" ]
then
  usage
fi

generated_files="AbstractConnectionBean.java AbstractMetaKeyBean.java MetaList.java MostRecentBean.java SentTextBean.java"
for f in $generated_files
do
  rm -f src/com/iiordanov/*/$f
  cp prebuilt/src/com/iiordanov/$PRJ/$f src/com/iiordanov/$PRJ/$f
done

mkdir -p java
rsync -a prebuilt/org java/

popd

echo
echo "Done copying prebuilt sqlitegen and Gstreamer files."
