#!/bin/bash

brew install coreutils

if git clone https://github.com/FreeRDP/FreeRDP.git FreeRDP_iphoneos
then
  pushd FreeRDP_iphoneos
  git checkout stable-2.0

# iOS Build
  cmake -DCMAKE_TOOLCHAIN_FILE=cmake/iOSToolchain.cmake \
        -DFREERDP_IOS_EXTERNAL_SSL_PATH=$(realpath ../../iSSH2/openssl_iphoneos) \
        -DCMAKE_CXX_FLAGS:STRING="-DTARGET_OS_IPHONE" \
        -DCMAKE_C_FLAGS:STRING="-DTARGET_OS_IPHONE" \
        -DCMAKE_OSX_ARCHITECTURES="arm64" \
        -GXcode

  echo "The FreeRDP project has been cloned to $(realpath .)"
  echo "Open the project in xcode, and set a valid Development Team in the iFreeRDP Target under Signing & Capabilities."
  echo "Press enter when you are done."
  read continue

  cmake --build .
  popd
fi

if git clone https://github.com/FreeRDP/FreeRDP.git FreeRDP_maccatalyst
then
# Mac Catalyst build
  pushd FreeRDP_maccatalyst
  git checkout stable-2.0

  patch -p1 < ../maccatalyst.patch

  MACOSX_SDK_DIR=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
  cmake -DCMAKE_TOOLCHAIN_FILE=cmake/iOSToolchain.cmake \
        -DFREERDP_IOS_EXTERNAL_SSL_PATH=$(realpath ../../iSSH2/openssl_iphoneos) \
        -DUIKIT_FRAMEWORK="${MACOSX_SDK_DIR}/System/iOSSupport/System/Library/Frameworks/UIKit.framework" \
        -DCMAKE_OSX_ARCHITECTURES="x86_64" \
        -DCMAKE_CXX_FLAGS:STRING="-target x86_64-apple-ios13.4-macabi -DTARGET_OS_IPHONE" \
        -DCMAKE_C_FLAGS:STRING="-target x86_64-apple-ios13.4-macabi -DTARGET_OS_IPHONE" \
        -DCMAKE_IOS_SDK_ROOT=${MACOSX_SDK_DIR} \
        -DWITH_NEON=OFF \
        -G"Unix Makefiles"
  cmake --build .
  popd
fi
