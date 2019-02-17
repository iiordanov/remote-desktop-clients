#!/bin/bash -e

SKIP_BUILD=false

usage () {
  echo "$0 bVNC|freebVNC|aSPICE|freeaSPICE|aRDP|freeaRDP|CustomVnc|libs /path/to/your/android/ndk /path/to/your/android/sdk"
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
  -a "$PRJ" != "CustomVnc" \
  -a "$PRJ" != "libs" -o "$ANDROID_NDK" == "" \
  -o "$ANDROID_SDK" == "" ]
then
  usage
fi

if [ "$PRJ" == "libs" ]
then
  PRJ=bVNC
  BUILDING_DEPENDENCIES=true
fi

if echo $PRJ | grep -iq "Custom"
then
  # TODO: Instead of copying an exact android manifest, copy the one matching the TYPE of app instead (VNC, RDP, or SPICE)
  rm AndroidManifest.xml
  cp AndroidManifest.xml.$PRJ AndroidManifest.xml

  sed -i "s/__CUSTOM_APP_PACKAGE__/$PRJ/" AndroidManifest.xml

  # TODO: Replace with a placeholder package instead of using freeaSPICE
  rm -rf src2/main/java/com/iiordanov/$PRJ
  cp -a src2/main/java/com/iiordanov/freeaSPICE src2/main/java/com/iiordanov/$PRJ
  sed -i "s/package com.iiordanov.freeaSPICE/package com.iiordanov.$PRJ/" src2/main/java/com/iiordanov/$PRJ/*

  # TODO: Replace with a placeholder comment that gets text-replaced instead of using the freeaSPICE import
  find src2/main/java/com/iiordanov -name \*.java -exec sed -i "s/com\.iiordanov\.freeaSPICE\./com\.iiordanov\.$PRJ\./" {} \;
else
  ln -sf AndroidManifest.xml.$PRJ AndroidManifest.xml
  find src2/main/java/com/iiordanov -name \*.java -exec sed -i "s/com\.iiordanov\.Custom.*/com\.iiordanov\.freeaSPICE\.\*;/" {} \;
fi

./copy_prebuilt_files.sh $PRJ


if [ "$SKIP_BUILD" == "false" ]
then
  pushd jni/libs
  ./build-deps.sh -j 4 -n $ANDROID_NDK build $PRJ
  popd

  if echo $PRJ | grep -iq "SPICE\|Opaque"
  then
    ${ANDROID_NDK}/ndk-build
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
  [ -d ${freerdp_libs_dir} ] && rm -rf ${freerdp_libs_dir}.DISABLED && mv ${freerdp_libs_dir} ${freerdp_libs_dir}.DISABLED
  rm -rf ${freerdp_libs_link}
elif echo $PRJ | grep -iq "RDP"
then
  clean_libs "sqlcipher" libs/
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
