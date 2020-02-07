#!/bin/bash -e

SKIP_BUILD=false

function usage () {
  echo "Usage: $0 Opaque|remoteClientLib|libs /path/to/your/android/ndk /path/to/your/android/sdk"
  exit 1
}

function clean_libs () {
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

function install_ndk() {
    DIR=$1
    VER=$2

    pushd ${DIR} >&/dev/null
    if [ ! -e android-ndk-${VER} ]
    then
        wget https://dl.google.com/android/repository/android-ndk-${VER}-linux-x86_64.zip  >&/dev/null
        unzip android-ndk-${VER}-linux-x86_64.zip >&/dev/null
    fi
    popd >&/dev/null
    echo $(realpath ${DIR}/android-ndk-${VER})
}


if [ "$1" == "--skip-build" ]
then
  SKIP_BUILD=true
  shift
else
  export PATH=/opt/cmake-3.5.1-Linux-x86_64/bin:${PATH}
  if ! cmake --version | grep 3.5.1
  then
    echo "In order to build FreeRDP, cmake v3.5.1 is required."
    echo "Please install it from https://github.com/Kitware/CMake/releases/tag/v3.5.1"
    echo "and make it the default cmake on your PATH for this build."
    echo "You can also install it at /opt/cmake-3.5.1-Linux-x86_64/ instead."
    exit 1
  fi
fi

DIR=$(dirname $0)
pushd $DIR

PRJ="$1"
export ANDROID_SDK="$2"

if [ "$PRJ" != "Opaque" -a "$PRJ" != "libs" -a "$PRJ" != "remoteClientLib" \
  -o "$ANDROID_SDK" == "" ]
then
  usage
fi

if [ "$PRJ" == "libs" ]
then
  BUILDING_DEPENDENCIES=true
else
  ../copy_prebuilt_files.sh $PRJ
fi

if [ "$SKIP_BUILD" == "false" ]
then
  pushd ../remoteClientLib/jni/libs/
  echo "Automatically installing Android NDK"
  export ANDROID_NDK=$(install_ndk ./ r18b)

  ./build-deps.sh -j 4 -n $ANDROID_NDK build $PRJ
  popd

  if echo $PRJ | grep -q "Opaque\|libs\|remoteClientLib"
  then
    pushd ../remoteClientLib/
    ${ANDROID_NDK}/ndk-build

    # Add your custom certificate authority files to certificate bundle from gstreamer.
    if [ -n "$(ls certificate_authorities/)" ]
    then
      for ca in certificate_authorities/*
      do
        echo Adding ${ca} to gstreamer provided ca-bundle.crt
        cat ${ca} >> src/main/assets/ca-bundle.crt
      done
    fi
    popd
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
