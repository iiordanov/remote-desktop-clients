#!/bin/bash -e

# For spice-protocol and spice-gtk for macos
export LDFLAGS="-framework Foundation -L/usr/local/opt/openssl@1.1/lib"
export CPPFLAGS="-I/usr/local/opt/openssl@1.1/include"
export PKG_CONFIG_PATH=$(pwd)/root_macos/share/pkgconfig:\
$(pwd)/root_macos/lib/pkgconfig:\
/usr/local/lib/pkgconfig:\
/usr/local/opt/openssl@1.1/lib/pkgconfig:\
/usr/local/opt/zlib/lib/pkgconfig:\
/usr/local/opt/jpeg-turbo/lib/pkgconfig:\
/usr/local/opt/sqlite/lib/pkgconfig:\
/usr/local/opt/icu4c/lib/pkgconfig
export SDKROOT="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
# For brew installed bison to be available
export PATH=/usr/local/bin:$(pwd)/root_macos/bin:${PATH}

BREW_DEPS="xz bison meson libunistring nasm openssl python3 cairo zlib libpng jpeg-turbo gobject-introspection librest sqlite3 icu4c json-glib gobject-introspection"
brew install ${BREW_DEPS}
brew link --overwrite ${BREW_DEPS}
pip3 install six

brew unlink glib gobject-introspection
if [ ! -d gst-build ]
then

  git clone https://gitlab.freedesktop.org/gstreamer/gst-build.git
  pushd gst-build
  git checkout 1.18.0
  patch -p1 < ../gst-build-config.patch

  meson setup --native-file ../macos-native-file.txt --prefix $(pwd)/../root_macos/ build_macos

  pushd subprojects/glib
  git reset --hard
  patch -p1 < ../../../gst-build-glib-fix-cocoa.patch
  popd

  pushd subprojects/glib-networking
  git reset --hard
  patch -p1 < ../../../gst-build-glib-networking-disable-tls-tests.patch
  popd

  ninja -C build_macos/
  ninja -C build_macos/ install

fi

if [ ! -d spice-protocol ]
then

  git clone https://gitlab.freedesktop.org/spice/spice-protocol
  pushd spice-protocol
  rm -rf build_macos
  mkdir -p build_macos
  meson setup --prefix $(pwd)/../root_macos/ build_macos
  ninja -C build_macos
  ninja -C build_macos/ install
  popd

fi

if [ ! -d spice-gtk ]
then

  git clone https://gitlab.freedesktop.org/spice/spice-gtk
  pushd spice-gtk
  rm -rf build_macos
  mkdir -p build_macos
  meson setup -Dvapi=disabled -Dintrospection=disabled --buildtype=plain --prefix $(pwd)/../root_macos/ build_macos
  
  #meson setup --prefix $(pwd)/../root_macos/ build_macos
  ninja -C build_macos
  ninja -C build_macos/ install
  popd

fi

brew link gobject-introspection
if [ ! -d libgovirt ]
then

  git clone https://gitlab.gnome.org/GNOME/libgovirt.git
  pushd libgovirt
  rm -rf build_macos
  mkdir -p build_macos

  patch -p1 < ../libgovirt.patch

  meson setup --native-file ../macos-native-file.txt --prefix $(pwd)/../root_macos/ build_macos
  ninja -C build_macos
  ninja -C build_macos/ install
  popd

fi


CFLAGS="-I./root_macos/include/govirt-1.0 \
        -I./root_macos/include/glib-2.0 \
        -I./root_macos/lib/glib-2.0/include \
        -I/usr/local/include/rest-0.7 \
        -I/usr/local/include/libsoup-2.4 \
        -I/usr/local/include \
        -I./root_macos/include/spice-client-glib-2.0 \
        -I./root_macos/include/spice-1 \
        -I./spice-gtk/src \
        -I./spice-gtk/subprojects/spice-common \
        -I./spice-gtk/build_macos/subprojects/spice-common \
        -I./spice-gtk/tools \
        -I/Users/iiordanov/software/remote-desktop-clients/remoteClientLib/jni/virt-viewer \
        -I/usr/local/include/libusb-1.0 \
        -I./"

LDFLAGS="-L/usr/local/lib \
         -L./root_macos/lib \
         -lspice-client-glib-2.0 -lglib-2.0 -lgobject-2.0 -lusb-1.0 -lgthread-2.0"

REMOTE_CLIENT_LIB_PATH=../../../remoteClientLib/jni

for f in ${REMOTE_CLIENT_LIB_PATH}/android/android-io.c ${REMOTE_CLIENT_LIB_PATH}/android/android-service.c \
         ${REMOTE_CLIENT_LIB_PATH}/android/android-spice-widget.c ${REMOTE_CLIENT_LIB_PATH}/android/android-spicy.c \
         ${REMOTE_CLIENT_LIB_PATH}/virt-viewer/virt-viewer-file.c
do
  cc $CFLAGS $f -c
done

ar rsu libspice.a *.o
ranlib libspice.a
rm *.o

cc -o aspicec $CFLAGS $LDFLAGS -L./ -lspice
