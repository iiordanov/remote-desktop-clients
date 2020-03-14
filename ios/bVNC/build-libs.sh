#!/bin/bash -e

DEVELOPMENT_TEAM=$1
shift

if [ -z "${DEVELOPMENT_TEAM}" ]
then
  echo "Pass your development team from https://developer.apple.com/account/#/membership/ as the first argument."
  echo "$0 123456789A [Debug|Release] [clean]"
  exit 1
fi

TYPE=$1
if [ -z "$TYPE" ]
then
  TYPE=Debug
fi

CLEAN=$2

# Ensure libjpeg-turbo is installed in the default path
if [ ! -d /opt/libjpeg-turbo/ ]
then
  echo "You need to download and install libjpeg-turbo in the default location."
  echo "Pre-built libraries for iOS are available at https://libjpeg-turbo.org/Documentation/OfficialBinaries"
  exit 1
fi

# Clone and build OpenSSL
if git clone https://github.com/x2on/OpenSSL-for-iPhone.git
then
  pushd OpenSSL-for-iPhone
  patch -p1 < ../01_openssl.patch
  ./build-libssl.sh --deprecated --version=1.1.1d
  patch -p1 < ../02_openssl.patch
  patch -p1 < ../03_openssl.patch
  popd
else
  echo "Found OpenSSL-for-iPhone directory, assuming it is built, please remove with 'rm -rf OpenSSL-for-iPhone' to rebuild"
fi

# Clone and build libssh2
if git clone https://github.com/Frugghi/iSSH2.git
then
  pushd iSSH2
  ./iSSH2.sh --platform=iphoneos --min-version=12.0
  popd
else
  echo "Found libssh2 directory, assuming it is built, please remove with 'rm -rf iSSH2' to rebuild"
fi

# Copy SSH libs and header files to project
rsync -avP iSSH2/libssh2_iphoneos/ ./bVNC.xcodeproj/libs_combined/

if git clone https://github.com/LibVNC/libvncserver.git
then
  patch -p1 < ../01_libvncserver.patch
fi

git clone https://github.com/leetal/ios-cmake.git || true
pushd libvncserver/

if [ -n "${CLEAN}" ]
then
  rm -rf build_simulator64
fi

if [ -n "${CLEAN}" -o ! -d build_simulator64 ]
then
  mkdir -p build_simulator64
  pushd build_simulator64
  cmake .. -G Xcode -DENABLE_BITCODE=OFF -DARCHS='arm64 x86_64' \
    -DCMAKE_C_FLAGS='-D OPENSSL_MIN_API=0x00908000L -D OPENSSL_API_COMPAT=0x00908000L' \
    -DOPENSSL_SSL_LIBRARY=../../OpenSSL-for-iPhone/lib/libssl.a \
    -DOPENSSL_CRYPTO_LIBRARY=../../OpenSSL-for-iPhone/lib/libcrypto.a \
    -DOPENSSL_INCLUDE_DIR=../../OpenSSL-for-iPhone/include \
    -DCMAKE_INSTALL_PREFIX=./libs -DCMAKE_INSTALL_PREFIX=./libs \
    -DCMAKE_TOOLCHAIN_FILE=../../ios-cmake/ios.toolchain.cmake \
    -DPLATFORM=SIMULATOR64 -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="${DEVELOPMENT_TEAM}" \
    -DJPEG_LIBRARY=/opt/libjpeg-turbo/lib/libjpeg.a -DJPEG_INCLUDE_DIR=/opt/libjpeg-turbo/include
else
  pushd build_simulator64
fi
cmake --build . --config ${TYPE} --target install

popd

if [ -n "${CLEAN}" ]
then
  rm -rf build_iphone
fi

echo 'PRODUCT_BUNDLE_IDENTIFIER = com.iiordanov.bVNC' > ${TYPE}.xcconfig
if [ -n "${CLEAN}" -o ! -d build_iphone ]
then
  mkdir -p build_iphone
  pushd build_iphone
  cmake .. -G Xcode -DENABLE_BITCODE=OFF -DARCHS='arm64 x86_64' \
    -DCMAKE_C_FLAGS='-D OPENSSL_MIN_API=0x00908000L -D OPENSSL_API_COMPAT=0x00908000L' \
    -DOPENSSL_SSL_LIBRARY=../../OpenSSL-for-iPhone/lib/libssl.a \
    -DOPENSSL_CRYPTO_LIBRARY=../../OpenSSL-for-iPhone/lib/libcrypto.a \
    -DOPENSSL_INCLUDE_DIR=../../OpenSSL-for-iPhone/include \
    -DCMAKE_INSTALL_PREFIX=./libs -DCMAKE_INSTALL_PREFIX=./libs \
    -DCMAKE_TOOLCHAIN_FILE=../../ios-cmake/ios.toolchain.cmake \
    -DPLATFORM=OS -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="${DEVELOPMENT_TEAM}" \
    -DJPEG_LIBRARY=/opt/libjpeg-turbo/lib/libjpeg.a -DJPEG_INCLUDE_DIR=/opt/libjpeg-turbo/include
else
  pushd build_iphone
fi

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
