#
# Copyright (C) 2020- Morpheusly Inc.
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
# USA.
#
#!/bin/bash -e

. build-libs.conf

function realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}

function usage() {
  echo "$0 [Debug|Release] [clean]"
  exit 1
}

function check_dependencies() {
  if ! which cmake
  then
    echo "You must have cmake installed."
    exit 1
  fi
}

if [ "${1}" == "-h" ]
then
  usage
fi

check_dependencies

TYPE=$1
if [ -z "$TYPE" ]
then
  TYPE=Debug
fi

if [ "${TYPE}" != "Debug" -a "${TYPE}" != "Release" ]
then
  usage
fi

CLEAN=$2


if git clone https://github.com/leetal/ios-cmake.git
then
  pushd ios-cmake
  git checkout ${IOS_CMAKE_VERSION}
  popd
else
  echo "Found ios-cmake directory, run rm -rf ios-cmake to clone again."
  sleep 2
fi


# Clone and build libjpeg-turbo
if git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git
then
  pushd libjpeg-turbo
  git checkout ${LIBJPEG_TURBO_VERSION}

  mkdir -p build_iphoneos
  pushd build_iphoneos

  IOS_PLATFORMDIR=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform
  IOS_SYSROOT=($IOS_PLATFORMDIR/Developer/SDKs/iPhoneOS*.sdk)
  export CFLAGS="-Wall -arch arm64 -miphoneos-version-min=8.0 -funwind-tables"
  cat <<EOF >toolchain.cmake
    set(CMAKE_SYSTEM_NAME Darwin)
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    set(CMAKE_C_COMPILER /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang)
EOF
  cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake \
        -DCMAKE_OSX_SYSROOT=${IOS_SYSROOT[0]} \
        -DPLATFORM=OS64 -DDEPLOYMENT_TARGET=12.0 \
        -DCMAKE_INSTALL_PREFIX=./libs \
        -DENABLE_BITCODE=OFF \
        -DENABLE_VISIBILITY=ON \
        -DENABLE_ARC=OFF ..
  make -j 12
  make install
  popd

  mkdir -p build_maccatalyst
  pushd build_maccatalyst

  IOS_PLATFORMDIR=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform
  IOS_SYSROOT=($IOS_PLATFORMDIR/Developer/SDKs/MacOSX*.sdk)
  export CFLAGS="-Wall -arch x86_64 -mmacosx-version-min=10.15 -funwind-tables"
  cat <<EOF >toolchain.cmake
    set(CMAKE_SYSTEM_NAME Darwin)
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
    set(CMAKE_C_COMPILER /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang)
EOF
  cmake -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake \
        -DCMAKE_OSX_SYSROOT=${IOS_SYSROOT[0]} \
        -DPLATFORM=MAC_CATALYST -DDEPLOYMENT_TARGET=10.15 \
        -DCMAKE_INSTALL_PREFIX=./libs \
        -DCMAKE_CXX_FLAGS_MAC_CATALYST:STRING="-target x86_64-apple-ios13.2-macabi" \
        -DCMAKE_C_FLAGS_MAC_CATALYST:STRING="-target x86_64-apple-ios13.2-macabi" \
        -DCMAKE_BUILD_TYPE=MAC_CATALYST \
        -DENABLE_BITCODE=OFF \
        -DENABLE_VISIBILITY=ON \
        -DENABLE_ARC=OFF ..
  make -j 12
  make install
  popd

  popd
else
  echo "Found libjpeg-turbo directory, run rm -rf libjpeg-turbo to build again."
  sleep 2
fi


# Lipo together the architectures for libjpeg-turbo and copy them to the common directory.
mkdir -p libjpeg-turbo/libs_combined/lib/
rsync -avP libjpeg-turbo/build_iphoneos/libs/include libjpeg-turbo/libs_combined/
for lib in libjpeg.a libturbojpeg.a
do
  echo "Running lipo to create ${lib}"
  lipo libjpeg-turbo/build_maccatalyst/libs/lib/libturbojpeg.a libjpeg-turbo/build_iphoneos/libs/lib/libturbojpeg.a \
        -output libjpeg-turbo/libs_combined/lib/${lib} -create
done
rsync -avP libjpeg-turbo/libs_combined/ ./bVNC.xcodeproj/libs_combined/


# Clone and build libssh2
if git clone https://github.com/Jan-E/iSSH2.git
then
  pushd iSSH2
  git checkout ${ISSH2_VERSION}
  echo "Patching Jan-E/iSSH2"
  patch -p1 < ../iSSH2.patch
  ./catalyst.sh
  popd
else
  echo "Found libssh2 directory, assuming it is built, please remove with 'rm -rf iSSH2' to rebuild"
  sleep 2
fi


# Copy SSH libs and header files to project
rsync -avP iSSH2/libssh2_iphoneos/ ./bVNC.xcodeproj/libs_combined/
rsync -avP iSSH2/openssl_iphoneos/ ./bVNC.xcodeproj/libs_combined/

git clone https://github.com/iiordanov/libvncserver.git || true
pushd libvncserver/
git pull
git checkout ${LIBVNCSERVER_VERSION}

if [ -n "${CLEAN}" ]
then
  rm -rf build_simulator64 build_iphone build_maccatalyst
fi

if [ -n "${SIMULATOR_BUILD}" -a ! -d build_simulator64 ]
then
  echo "Simulator build"

  if [ ! -d build_simulator64 ]
  then
    mkdir -p build_simulator64
    pushd build_simulator64
    cmake .. -G"Unix Makefiles" -DENABLE_BITCODE=OFF -DARCHS='x86_64' \
        -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../ios-cmake/ios.toolchain.cmake) \
        -DCMAKE_C_FLAGS='-D OPENSSL_MIN_API=0x00908000L -D OPENSSL_API_COMPAT=0x00908000L' \
        -DOPENSSL_SSL_LIBRARY=$(realpath ../../iSSH2/openssl_iphoneos/lib/libssl.a) \
        -DOPENSSL_CRYPTO_LIBRARY=$(realpath ../../iSSH2/openssl_iphoneos/lib/libcrypto.a) \
        -DOPENSSL_INCLUDE_DIR=$(realpath ../../iSSH2/openssl_iphoneos/include) \
        -DCMAKE_INSTALL_PREFIX=./libs \
        -DPLATFORM=SIMULATOR64 \
        -DBUILD_SHARED_LIBS=OFF -DENABLE_VISIBILITY=ON -DENABLE_ARC=OFF \
        -DDEPLOYMENT_TARGET=12.0 \
        -DLIBVNCSERVER_HAVE_ENDIAN_H=OFF \
        -DWITH_GCRYPT=OFF \
        -DCMAKE_PREFIX_PATH=$(realpath ../../libjpeg-turbo/libs_combined/)
     popd
  fi
  pushd build_simulator64
  cmake --build . --config ${TYPE} --target install || true
  popd
fi

echo 'PRODUCT_BUNDLE_IDENTIFIER = com.iiordanov.bVNC' > ${TYPE}.xcconfig
if [ -z "${SIMULATOR_BUILD}" ]
then
  echo "Non-simulator build"

  if [ ! -d build_iphone ]
  then
    mkdir -p build_iphone
    pushd build_iphone
    cmake .. -G"Unix Makefiles" -DARCHS='arm64' \
        -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../ios-cmake/ios.toolchain.cmake) \
        -DPLATFORM=OS64 \
        -DDEPLOYMENT_TARGET=12.0 \
        -DENABLE_BITCODE=OFF \
        -DOPENSSL_SSL_LIBRARY=$(realpath ../../iSSH2/openssl_iphoneos/lib/libssl.a) \
        -DOPENSSL_CRYPTO_LIBRARY=$(realpath ../../iSSH2/openssl_iphoneos/lib/libcrypto.a) \
        -DOPENSSL_INCLUDE_DIR=$(realpath ../../iSSH2/openssl_iphoneos/include) \
        -DCMAKE_INSTALL_PREFIX=./libs \
        -DBUILD_SHARED_LIBS=OFF \
        -DENABLE_VISIBILITY=ON \
        -DENABLE_ARC=OFF \
        -DWITH_SASL=OFF \
        -DLIBVNCSERVER_HAVE_ENDIAN_H=OFF \
        -DWITH_GCRYPT=OFF \
        -DCMAKE_PREFIX_PATH=$(realpath ../../libjpeg-turbo/libs_combined/)
    popd
  fi
  pushd build_iphone
  make -j 12
  make install
  popd

  if [ ! -d build_maccatalyst ]
  then
    mkdir -p build_maccatalyst
    pushd build_maccatalyst
    cmake .. -G"Unix Makefiles" -DARCHS='x86_64' \
        -DCMAKE_TOOLCHAIN_FILE=$(realpath ../../ios-cmake/ios.toolchain.cmake) \
        -DPLATFORM=MAC_CATALYST \
        -DDEPLOYMENT_TARGET=10.15 \
        -DCMAKE_CXX_FLAGS_MAC_CATALYST:STRING="-target x86_64-apple-ios13.2-macabi" \
        -DCMAKE_C_FLAGS_MAC_CATALYST:STRING="-target x86_64-apple-ios13.2-macabi" \
        -DCMAKE_BUILD_TYPE=MAC_CATALYST \
        -DENABLE_BITCODE=OFF \
        -DOPENSSL_SSL_LIBRARY=$(realpath ../../iSSH2/openssl_iphoneos/lib/libssl.a) \
        -DOPENSSL_CRYPTO_LIBRARY=$(realpath ../../iSSH2/openssl_iphoneos/lib/libcrypto.a) \
        -DOPENSSL_INCLUDE_DIR=$(realpath ../../iSSH2/openssl_iphoneos/include) \
        -DCMAKE_INSTALL_PREFIX=./libs \
        -DBUILD_SHARED_LIBS=OFF \
        -DENABLE_VISIBILITY=ON \
        -DENABLE_ARC=OFF \
        -DWITH_SASL=OFF \
        -DLIBVNCSERVER_HAVE_ENDIAN_H=OFF \
        -DWITH_GCRYPT=OFF \
        -DCMAKE_PREFIX_PATH=$(realpath ../../libjpeg-turbo/libs_combined/)
    popd
  fi
  pushd build_maccatalyst
  make -j 12
  make install
  popd
fi

# Lipo together the architectures for libvncserver and copy them to the common directory.
rsync -avP build_iphone/libs/ libs_combined/
pushd libs_combined
for lib in lib/lib*.a
do
  echo "Running lipo for ${lib}"
  if [ -z "${SIMULATOR_BUILD}" ]
  then
    lipo ../build_maccatalyst/libs/${lib} ../build_iphone/libs/${lib} -output ${lib} -create
  else
    lipo ../build_simulator64/libs/${lib} -output ${lib} -create
  fi
done
popd
popd

rsync -avPL libvncserver/libs_combined/ bVNC.xcodeproj/libs_combined/

# Make a super duper static lib out of all the other libs
pushd bVNC.xcodeproj/libs_combined/lib
/Library/Developer/CommandLineTools/usr/bin//libtool -static -o superlib.a libcrypto.a libssh2.a libssl.a libturbojpeg.a libvncclient.a
popd
