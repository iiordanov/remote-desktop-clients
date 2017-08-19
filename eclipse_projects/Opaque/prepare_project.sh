#!/bin/bash

SKIP_BUILD=false

usage () {
  echo "$0 Opaque /path/to/your/android/ndk"
  exit 1
}

clean_libs () {
  filter=$1
  dir=$2
  for f in $(find $dir -type f -name \*.so)
  do
    if ! echo $f | egrep -q "$filter"
    then
      rm -f $f
    fi
  done
}

if [ "$1" == "--skip-build" ]
then
  SKIP_BUILD=true
  shift
fi

DIR=$(dirname $0)
pushd $DIR

PRJ="$1"
ANDROID_NDK="$2"

if [ "$PRJ" != "Opaque" -o "$ANDROID_NDK" == "" ]
then
  usage
fi

if [ "$SKIP_BUILD" == "false" ]
then
  pushd jni/libs
  ./build-deps.sh -j 4 -n $ANDROID_NDK build
  popd

  ndk-build
fi

popd
