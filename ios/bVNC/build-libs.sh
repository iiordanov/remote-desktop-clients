#!/bin/bash

DEVELOPMENT_TEAM=$1
shift

if [ -z "${DEVELOPMENT_TEAM}" ]
then
  echo "Pass your development team from https://developer.apple.com/account/#/membership/ as the first argument."
  echo "$0 123456789A [Debug|Release]"
  exit 1
fi

TYPE=$1
if [ -z "$TYPE" ]
then
  TYPE=Debug
fi

git clone https://github.com/LibVNC/libvncserver.git
git clone https://github.com/leetal/ios-cmake.git
pushd libvncserver/

rm -rf build_simulator64
mkdir build_simulator64
pushd build_simulator64

cmake .. -G Xcode -DCMAKE_INSTALL_PREFIX=./libs -DCMAKE_TOOLCHAIN_FILE=../../ios-cmake/ios.toolchain.cmake -DPLATFORM=SIMULATOR64 -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="${DEVELOPMENT_TEAM}"
cmake --build . --config ${TYPE} --target install

popd

rm -rf build_iphone
mkdir build_iphone
pushd build_iphone

echo 'PRODUCT_BUNDLE_IDENTIFIER = com.iiordanov.bVNC' > ${TYPE}.xcconfig
cmake .. -G Xcode -DCMAKE_INSTALL_PREFIX=./libs -DCMAKE_TOOLCHAIN_FILE=../../ios-cmake/ios.toolchain.cmake -DPLATFORM=OS -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="${DEVELOPMENT_TEAM}"

# Workaround for missing PRODUCT_BUNDLE_IDENTIFIER in generated LibVNCServer.xcodeproj/project.pbxproj file
sed -i.bak '/ *ARCHS =.*/a\
PRODUCT_BUNDLE_IDENTIFIER="com.iiordanov.bVNC";
' LibVNCServer.xcodeproj/project.pbxproj

cmake --build . --config ${TYPE} --target install

# Workaround for a file that blocks the creation of a directory
if [ -f ${TYPE}-iphoneos ]
then
  rm ${TYPE}-iphoneos
  mkdir ${TYPE}-iphoneos
  cmake --build . --config ${TYPE} --target install
fi

popd

rsync -avP build_iphone/libs/ libs_combined/
pushd libs_combined

for lib in lib/lib*[0-9]*.[0-9]*.[0-9]*.*
do
 echo $lib
 lipo ../build_iphone/libs/${lib} ../build_simulator64/libs/${lib} -output ${lib} -create
done

popd

popd

rsync -avPL libvncserver/libs_combined/ bVNC.xcodeproj/libs_combined/
