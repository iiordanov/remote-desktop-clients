#!/bin/bash -e

SKIP_BUILD=false

usage () {
  echo "Usage: $0 Opaque|libs /path/to/your/android/ndk /path/to/your/android/sdk"
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
export ANDROID_NDK="$2"
export ANDROID_SDK="$3"

if [ "$PRJ" != "Opaque" -a "$PRJ" != "libs" \
  -o "$ANDROID_NDK" == "" -o "$ANDROID_SDK" == "" ]
then
  usage
fi

if [ "$PRJ" == "libs" ]
then
  PRJ=Opaque
  BUILDING_DEPENDENCIES=true
fi

if [ "$SKIP_BUILD" == "false" ]
then
  pushd jni/libs
  ./build-deps.sh -j 4 -n $ANDROID_NDK build $PRJ
  popd

  if echo $PRJ | grep -q "SPICE\|Opaque"
  then
    ${ANDROID_NDK}/ndk-build
  fi
fi

if [ -n "$BUILDING_DEPENDENCIES" ]
then
  echo "Done building libraries"
  exit 0
fi

popd
echo
echo "Now please switch to your IDE, select the bVNC project, refresh with F5,"
echo "and then clean and rebuild the project."
