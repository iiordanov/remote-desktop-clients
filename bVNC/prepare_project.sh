#!/bin/bash -e

SKIP_BUILD=false

usage () {
  echo "$0 bVNC|freebVNC|aSPICE|freeaSPICE|aRDP|freeaRDP|libs|remoteClientLib /path/to/your/android/ndk /path/to/your/android/sdk"
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

if [ "$PRJ" != "bVNC" -a "$PRJ" != "freebVNC" \
  -a "$PRJ" != "aSPICE" -a "$PRJ" != "freeaSPICE" \
  -a "$PRJ" != "aRDP" -a "$PRJ" != "freeaRDP" \
  -a "$PRJ" != "libs" -a "$PRJ" != "remoteClientLib" \
  -o "$ANDROID_NDK" == "" -o "$ANDROID_SDK" == "" ]
then
  usage
fi

if [ "$PRJ" == "libs" ]
then
  BUILDING_DEPENDENCIES=true
else
  ln -sf AndroidManifest.xml.$PRJ AndroidManifest.xml
  ../copy_prebuilt_files.sh $PRJ
fi

if [ "$SKIP_BUILD" == "false" ]
then
  pushd ../remoteClientLib/jni/libs
  ./build-deps.sh -j 4 -n $ANDROID_NDK build $PRJ
  popd

  if echo $PRJ | grep -q "SPICE\|Opaque\|libs\|remoteClientLib"
  then
    pushd ../remoteClientLib
    ${ANDROID_NDK}/ndk-build
    popd
  fi
fi

if [ -n "$BUILDING_DEPENDENCIES" ]
then
  echo "Done building libraries"
  exit 0
fi

freerdp_libs_dir=../freeRDPCore/src/main/jniLibs
freerdp_libs_link=../freeRDPCore/src/main/libs
if [ "$PRJ" == "bVNC" -o "$PRJ" == "freebVNC" ]
then
  clean_libs "sqlcipher" libs/
  clean_libs "sqlcipher" ../remoteClientLib/libs/
  [ -d ${freerdp_libs_dir} ] && rm -rf ${freerdp_libs_dir}.DISABLED && mv ${freerdp_libs_dir} ${freerdp_libs_dir}.DISABLED
  rm -rf ${freerdp_libs_link}
elif [ "$PRJ" == "aRDP" -o "$PRJ" == "freeaRDP" ]
then
  clean_libs "sqlcipher" libs/
  clean_libs "sqlcipher" ../remoteClientLib/libs/
  [ -d ${freerdp_libs_dir}.DISABLED ] && rm -rf ${freerdp_libs_dir} && mv ${freerdp_libs_dir}.DISABLED ${freerdp_libs_dir}
  rm -rf ${freerdp_libs_link}
  ln -s jniLibs ${freerdp_libs_link}
elif [ "$PRJ" == "aSPICE" -o "$PRJ" == "freeaSPICE" ]
then
  [ -d ${freerdp_libs_dir} ] && rm -rf ${freerdp_libs_dir}.DISABLED && mv ${freerdp_libs_dir} ${freerdp_libs_dir}.DISABLED
  rm -rf ${freerdp_libs_link}
fi

popd
echo
echo "Now please switch to your IDE, select the bVNC project, refresh with F5,"
echo "and then clean and rebuild the project."
