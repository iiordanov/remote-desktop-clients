#!/bin/bash -e

SKIP_BUILD=false

usage () {
  echo "$0 bVNC|freebVNC|aSPICE|freeaSPICE|aRDP|freeaRDP|CustomVncAnyPackageName|libs|remoteClientLib /path/to/your/android/ndk /path/to/your/android/sdk"
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

if [[ "$PRJ" != "bVNC" && "$PRJ" != "freebVNC" \
  && "$PRJ" != "aSPICE" && "$PRJ" != "freeaSPICE" \
  && "$PRJ" != "aRDP" && "$PRJ" != "freeaRDP" \
  && "$PRJ" =~ "^Custom.*" \
  && "$PRJ" != "libs" && "$PRJ" != "remoteClientLib" \
  || "$ANDROID_NDK" == "" || "$ANDROID_SDK" == "" ]]
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
  ../copy_prebuilt_files.sh $PRJ
fi

if [ -n "${CUSTOM_CLIENT}" ]
then
  rm AndroidManifest.xml
  cp AndroidManifest.xml.${CUSTOM_MANIFEST_EXTENSION} AndroidManifest.xml

  sed -i "s/__CUSTOM_APP_PACKAGE__/$PRJ/" AndroidManifest.xml

  rm -rf src2/main/java/com/iiordanov/$PRJ
  cp -a src2/main/java/com/iiordanov/CustomClientPackage src2/main/java/com/iiordanov/$PRJ
  sed -i "s/package com.iiordanov.CustomClientPackage/package com.iiordanov.$PRJ/" src2/main/java/com/iiordanov/$PRJ/*

  find src2/main/java/com/iiordanov -name \*.java -exec sed -i "s/com\.iiordanov\.CustomClientPackage\.\(.*\)/com\.iiordanov\.$PRJ\.\1 \/\/CUSTOM_CLIENT_IMPORTS/" {} \;
else
  ln -sf AndroidManifest.xml.$PRJ AndroidManifest.xml
  find src2/main/java/com/iiordanov -name \*.java -exec sed -i "s/import.*CUSTOM_CLIENT_IMPORTS/import com\.iiordanov\.CustomClientPackage\.\*;/" {} \;
  find src2/main/java/com/iiordanov -name \*.java -exec sed -i "s/package.*CUSTOM_CLIENT_IMPORTS/package com\.iiordanov\.CustomClientPackage;/" {} \;
fi

../copy_prebuilt_files.sh $PRJ

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
