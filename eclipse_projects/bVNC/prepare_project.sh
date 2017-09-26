#!/bin/bash

SKIP_BUILD=false

usage () {
  echo "$0 bVNC|freebVNC|aSPICE|freeaSPICE|aRDP|freeaRDP|libs /path/to/your/android/ndk"
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

if [ "$PRJ" != "bVNC" -a "$PRJ" != "freebVNC" \
  -a "$PRJ" != "aSPICE" -a "$PRJ" != "freeaSPICE" \
  -a "$PRJ" != "aRDP" -a "$PRJ" != "freeaRDP" \
  -a "$PRJ" != "libs" -o "$ANDROID_NDK" == "" ]
then
  usage
fi

if [ "$PRJ" == "libs" ]
then
  PRJ=bVNC
  BUILDING_DEPENDENCIES=true
fi

ln -sf AndroidManifest.xml.$PRJ AndroidManifest.xml

./copy_prebuilt_files.sh $PRJ

#echo
#echo "Now please switch to your IDE, select the bVNC project, refresh with F5,"
#echo "clean and rebuild it to auto-generate the DAO objects with sqlitegen."
#echo
#echo "You must have sqlitegen installed as per the BUILDING file."
#echo
#echo "When the build in your IDE completes, switch back to this terminal and"
#echo "press ENTER key for this script to continue executing."
#echo
#read CONTINUE

#for f in $generated_files
#do
#  file=gen/com/iiordanov/$PRJ/$f
#  if [ ! -f $file ]
#  then
#    echo "Could not find auto-generated file $file. Please try cleaning / rebuilding the project, and make sure sqlitegen is installed properly."
#    echo
#    exit 2
#  else
#    echo "Moving $f to src/com/iiordanov/$PRJ"
#    mv $file src/com/iiordanov/$PRJ/
#  fi
#done

if [ "$SKIP_BUILD" == "false" ]
then
  pushd jni/libs
  ./build-deps.sh -j 4 -n $ANDROID_NDK build
  popd

  ndk-build
fi

if [ -n "$BUILDING_DEPENDENCIES" ]
then
  echo "Done building libraries"
  exit 0
fi

freerdp_libs_dir=jni/libs/deps/FreeRDP/client/Android/Studio/freeRDPCore/src/main/jniLibs
freerdp_libs_link=jni/libs/deps/FreeRDP/client/Android/Studio/freeRDPCore/src/main/libs
if [ "$PRJ" == "bVNC" -o "$PRJ" == "freebVNC" ]
then
  clean_libs "sqlcipher" libs/
  [ -d ${freerdp_libs_dir} ] && mv ${freerdp_libs_dir} ${freerdp_libs_dir}.DISABLED
  rm -rf ${freerdp_libs_link}
elif [ "$PRJ" == "aRDP" -o "$PRJ" == "freeaRDP" ]
then
  clean_libs "sqlcipher" libs/
  [ -d ${freerdp_libs_dir}.DISABLED ] && mv ${freerdp_libs_dir}.DISABLED ${freerdp_libs_dir}
  rm -rf ${freerdp_libs_link}
  ln -s jniLibs ${freerdp_libs_link}
elif [ "$PRJ" == "aSPICE" -o "$PRJ" == "freeaSPICE" ]
then
  [ -d ${freerdp_libs_dir} ] && mv ${freerdp_libs_dir} ${freerdp_libs_dir}.DISABLED
  rm -rf ${freerdp_libs_link}
fi

popd
echo
echo "Now please switch to your IDE, select the bVNC project, refresh with F5,"
echo "and then clean and rebuild the project."
