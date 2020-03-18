#!/bin/bash -e

SKIP_BUILD=false

function usage () {
  echo "$0 bVNC|freebVNC|aSPICE|freeaSPICE|aRDP|freeaRDP|CustomVncAnyPackageName|\
CustomRdpAnyPackageName|CustomSpiceAnyPackageName|libs|remoteClientLib /path/to/your/android/ndk /path/to/your/android/sdk"
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
fi

DIR=$(dirname $0)
pushd $DIR

PRJ="$1"
export ANDROID_SDK="$2"

if [[ "$PRJ" != "bVNC" && "$PRJ" != "freebVNC" \
  && "$PRJ" != "aSPICE" && "$PRJ" != "freeaSPICE" \
  && "$PRJ" != "aRDP" && "$PRJ" != "freeaRDP" \
  && "$PRJ" =~ "^Custom.*" \
  && "$PRJ" != "libs" && "$PRJ" != "remoteClientLib" \
  || "$ANDROID_SDK" == "" ]]
then
  usage
fi

if echo ${PRJ} | grep -qi "VNC"
then
  CLIENT_TYPE_VNC=true
  CUSTOM_MANIFEST_EXTENSION=CustomVnc
elif echo ${PRJ} | grep -qi "RDP"
then
  CLIENT_TYPE_RDP=true
  CUSTOM_MANIFEST_EXTENSION=CustomRdp
elif echo ${PRJ} | grep -qi "SPICE"
then
  CLIENT_TYPE_SPICE=true
  CUSTOM_MANIFEST_EXTENSION=CustomSpice
fi

if [ "$PRJ" == "libs" ]
then
  BUILDING_DEPENDENCIES=true
elif echo ${PRJ} | grep -q Custom
then
  CUSTOM_CLIENT=true
  ORIG_PRJ=${PRJ}
  PRJ=$(echo ${PRJ} | sed 's/^Custom//')
else
  ln -sf AndroidManifest.xml.$PRJ AndroidManifest.xml
fi

if [ -n "${CUSTOM_CLIENT}" ]
then
  # Check that there are no changes to source code and exit if there are.
  if [ -n "$(git diff src src2)" ]
  then
    echo "Exiting because there are changes in src or src2. Please commit them before building custom clients. Changes:"
    git diff src src2
    exit 1
  fi

  rm AndroidManifest.xml
  cp AndroidManifest.xml.${CUSTOM_MANIFEST_EXTENSION} AndroidManifest.xml

  sed -i.bakautocustom "s/__CUSTOM_APP_PACKAGE__/$PRJ/" AndroidManifest.xml

  rm -rf src2/main/java/com/iiordanov/$PRJ
  cp -a src2/main/java/com/iiordanov/CustomClientPackage src2/main/java/com/iiordanov/$PRJ
  sed -i.bakautocustom "s/package com.iiordanov.CustomClientPackage/package com.iiordanov.$PRJ/" src2/main/java/com/iiordanov/$PRJ/*

  find src2/main/java/com/iiordanov -name \*.java -exec sed -i.bakautocustom "s/com\.iiordanov\.CustomClientPackage\.\(.*\)/com\.iiordanov\.$PRJ\.\1 \/\/CUSTOM_CLIENT_IMPORTS/" {} \;
else
  ln -sf AndroidManifest.xml.$PRJ AndroidManifest.xml
  find src2/main/java/com/iiordanov -name \*.java -exec sed -i.bakautocustom "s/import.*CUSTOM_CLIENT_IMPORTS/import com\.iiordanov\.CustomClientPackage\.\*;/" {} \;
  find src2/main/java/com/iiordanov -name \*.java -exec sed -i.bakautocustom "s/package.*CUSTOM_CLIENT_IMPORTS/package com\.iiordanov\.CustomClientPackage;/" {} \;
fi
find src2/main/java/com/iiordanov -name \*.bakautocustom -exec rm {} \;

if [ "$SKIP_BUILD" == "false" ]
then
  pushd ../remoteClientLib/jni/libs
  echo "Automatically installing Android NDK"
  export ANDROID_NDK=$(install_ndk ./ r18b)
  ./build-deps.sh -j 8 -n $ANDROID_NDK build $PRJ
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
if echo $PRJ | grep -iq "VNC"
then
  clean_libs "sqlcipher" libs/
  clean_libs "sqlcipher" ../remoteClientLib/libs/
  [ -d ${freerdp_libs_dir} ] && rm -rf ${freerdp_libs_dir}.DISABLED && mv ${freerdp_libs_dir} ${freerdp_libs_dir}.DISABLED
  rm -rf ${freerdp_libs_link}
elif echo $PRJ | grep -iq "RDP"
then
  clean_libs "sqlcipher" libs/
  clean_libs "sqlcipher" ../remoteClientLib/libs/
  [ -d ${freerdp_libs_dir}.DISABLED ] && rm -rf ${freerdp_libs_dir} && mv ${freerdp_libs_dir}.DISABLED ${freerdp_libs_dir}
  rm -rf ${freerdp_libs_link}
  ln -s jniLibs ${freerdp_libs_link}
elif echo $PRJ | grep -iq "SPICE"
then
  [ -d ${freerdp_libs_dir} ] && rm -rf ${freerdp_libs_dir}.DISABLED && mv ${freerdp_libs_dir} ${freerdp_libs_dir}.DISABLED
  rm -rf ${freerdp_libs_link}
fi

popd
echo
echo "Now please switch to your IDE, select the bVNC project, refresh with F5,"
echo "and then clean and rebuild the project."
