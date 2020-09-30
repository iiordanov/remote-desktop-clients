#!/bin/bash -e

if [ -z "${1}" ]
then
  echo "Pass your development team from https://developer.apple.com/account/#/membership/ as the first argument."
  echo "$0 123456789A [Debug|Release] [clean]"
  exit 1
fi

DEVELOPMENT_TEAM=$1
shift

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

rsync -avP /opt/libjpeg-turbo/lib /opt/libjpeg-turbo/include ./bVNC.xcodeproj/libs_combined/

echo "Ensure there are links for your version of Mac OS X present here in the form MacOSX10.15.7.sdk -> MacOSX.sdk"
echo "If you do not, OpenSSL build for Mac OS X will fail."
ls -l /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/

# Clone and build libssh2
if git clone https://github.com/Jan-E/iSSH2.git
then
  pushd iSSH2
  git checkout catalyst
  patch -p1 < ../iSSH2.patch
  ./catalyst.sh
  popd
else
  echo "Found libssh2 directory, assuming it is built, please remove with 'rm -rf iSSH2' to rebuild"
fi

# Copy SSH libs and header files to project
rsync -avP iSSH2/libssh2_iphoneos/ ./bVNC.xcodeproj/libs_combined/
rsync -avP iSSH2/openssl_iphoneos/ ./bVNC.xcodeproj/libs_combined/

if git clone https://github.com/leetal/ios-cmake.git
then
  pushd ios-cmake
  patch -p1 < ../ios-cmake.patch
  popd
fi

git clone https://github.com/iiordanov/libvncserver.git || true

pushd libvncserver/

if [ -n "${CLEAN}" ]
then
  rm -rf build_simulator64
fi

if [ -n "${SIMULATOR_BUILD}" -a ! -d build_simulator64 ]
then
  echo "Simulator build"

  if [ ! -d build_simulator64 ]
  then
    mkdir -p build_simulator64
    pushd build_simulator64
    cmake .. -G Xcode -DENABLE_BITCODE=OFF -DARCHS='x86_64' \
      -DCMAKE_C_FLAGS='-D OPENSSL_MIN_API=0x00908000L -D OPENSSL_API_COMPAT=0x00908000L' \
      -DOPENSSL_SSL_LIBRARY=../../iSSH2/openssl_iphoneos/lib/libssl.a \
      -DOPENSSL_CRYPTO_LIBRARY=../../iSSH2/openssl_iphoneos/lib/libcrypto.a \
      -DOPENSSL_INCLUDE_DIR=../../iSSH2/openssl_iphoneos/include \
      -DCMAKE_INSTALL_PREFIX=./libs -DCMAKE_INSTALL_PREFIX=./libs \
      -DCMAKE_TOOLCHAIN_FILE=../../ios-cmake/ios.toolchain.cmake \
      -DPLATFORM=SIMULATOR64 -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="${DEVELOPMENT_TEAM}" \
      -DBUILD_SHARED_LIBS=OFF -DENABLE_VISIBILITY=ON -DENABLE_ARC=OFF \
      -DDEPLOYMENT_TARGET=12.0 \
      -DLIBVNCSERVER_HAVE_ENDIAN_H=OFF \
      -DWITH_GCRYPT=OFF
# TODO: Prebuilt Jpeg lib not build with Mac Catalyst support
#      -DJPEG_LIBRARY=/opt/libjpeg-turbo/lib/libjpeg.a -DJPEG_INCLUDE_DIR=/opt/libjpeg-turbo/include \
     popd
  fi
  pushd build_simulator64
  cmake --build . --config ${TYPE} --target install
  popd
fi

if [ -n "${CLEAN}" ]
then
  rm -rf build_iphone
fi

echo 'PRODUCT_BUNDLE_IDENTIFIER = com.iiordanov.bVNC' > ${TYPE}.xcconfig
if [ -z "${SIMULATOR_BUILD}" ]
then
  echo "Non-simulator build"

  if [ ! -d build_iphone ]
  then
    mkdir -p build_iphone
    pushd build_iphone
# TODO: Prebuilt jpeg turbo doesn't have arm64e
#    cmake .. -G Xcode -DENABLE_BITCODE=OFF -DARCHS='arm64 arm64e'
    cmake .. -G Xcode -DENABLE_BITCODE=OFF -DARCHS='arm64' \
      -DCMAKE_C_FLAGS='-D OPENSSL_MIN_API=0x00908000L -D OPENSSL_API_COMPAT=0x00908000L' \
      -DOPENSSL_SSL_LIBRARY=../../iSSH2/openssl_iphoneos/lib/libssl.a \
      -DOPENSSL_CRYPTO_LIBRARY=../../iSSH2/openssl_iphoneos/lib/libcrypto.a \
      -DOPENSSL_INCLUDE_DIR=../../iSSH2/openssl_iphoneos/include \
      -DCMAKE_INSTALL_PREFIX=./libs -DCMAKE_INSTALL_PREFIX=./libs \
      -DCMAKE_TOOLCHAIN_FILE=../../ios-cmake/ios.toolchain.cmake \
      -DPLATFORM=OS64 -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="${DEVELOPMENT_TEAM}" \
      -DBUILD_SHARED_LIBS=OFF -DENABLE_VISIBILITY=ON -DENABLE_ARC=OFF \
      -DDEPLOYMENT_TARGET=12.0 \
      -DLIBVNCSERVER_HAVE_ENDIAN_H=OFF \
      -DWITH_GCRYPT=OFF
# TODO: Prebuilt Jpeg lib not build with Mac Catalyst support
#      -DJPEG_LIBRARY=/opt/libjpeg-turbo/lib/libjpeg.a -DJPEG_INCLUDE_DIR=/opt/libjpeg-turbo/include \
    popd
  fi
  pushd build_iphone

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

  if [ ! -d build_maccatalyst ]
  then
    mkdir -p build_maccatalyst
    pushd build_maccatalyst
    for i in 1 2 ; do cmake .. -DARCHS='x86_64' \
      -DCMAKE_TOOLCHAIN_FILE=../../ios-cmake/ios.toolchain.cmake \
      -DPLATFORM=MAC_CATALYST \
      -DDEPLOYMENT_TARGET=10.15 \
      -DCMAKE_CXX_FLAGS_MAC_CATALYST:STRING="-target x86_64-apple-ios13.2-macabi" \
      -DCMAKE_C_FLAGS_MAC_CATALYST:STRING="-target x86_64-apple-ios13.2-macabi" \
      -DCMAKE_BUILD_TYPE=MAC_CATALYST \
      -DENABLE_BITCODE=OFF \
      -DOPENSSL_SSL_LIBRARY=../../iSSH2/openssl_iphoneos/lib/libssl.a \
      -DOPENSSL_CRYPTO_LIBRARY=../../iSSH2/openssl_iphoneos/lib/libcrypto.a \
      -DOPENSSL_INCLUDE_DIR=../../iSSH2/openssl_iphoneos/include \
      -DCMAKE_INSTALL_PREFIX=./libs \
      -DBUILD_SHARED_LIBS=OFF -DENABLE_VISIBILITY=ON -DENABLE_ARC=OFF \
      -DWITH_SASL=OFF \
      -DLIBVNCSERVER_HAVE_ENDIAN_H=OFF \
      -DWITH_GCRYPT=OFF
    done
# TODO: Prebuilt Jpeg lib not build with Mac Catalyst support
#      -DJPEG_LIBRARY=/opt/libjpeg-turbo/lib/libjpeg.a -DJPEG_INCLUDE_DIR=/opt/libjpeg-turbo/include \
    popd
  fi
  pushd build_maccatalyst
  cmake --build . --config ${TYPE} --target install
  popd
fi

rsync -avP build_iphone/libs/ libs_combined/
pushd libs_combined

for lib in lib/lib*.a
do
  echo $lib
  if [ -z "${SIMULATOR_BUILD}" ]
  then
    lipo ../build_maccatalyst/libs/${lib} ../build_iphone/libs/${lib} -output ${lib} -create
  else
    lipo ../build_simulator64/libs/${lib} -output ${lib} -create
  fi
done

popd

popd

rsync -avPL libvncserver/libs_combined/lib libvncserver/libs_combined/include bVNC.xcodeproj/libs_combined/

# Make a super duper static lib out of all the other libs
pushd bVNC.xcodeproj/libs_combined/lib
/Library/Developer/CommandLineTools/usr/bin//libtool -static -o superlib.a libcrypto.a libssh2.a libssl.a libturbojpeg.a libvncclient.a
popd
