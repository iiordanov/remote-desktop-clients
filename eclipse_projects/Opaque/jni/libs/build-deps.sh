#!/bin/bash
#
# A script for building VMNetX dependencies for Android
# Based on build.sh from openslide-winbuild
#
# Copyright (c) 2017 Iordan Iordanov
# Copyright (c) 2011-2015 Carnegie Mellon University
# All rights reserved.
#
# This script is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This script is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along
# with this script.  If not, see <http://www.gnu.org/licenses/>.
#

set -eE

. build-deps.conf

expand() {
    # Print the contents of the named variable
    # $1  = the name of the variable to expand
    echo "${!1}"
}

tarpath() {
    # Print the tarball path for the specified package
    # $1  = the name of the program
    case "$1" in
    configguess)
        echo "tar/config.guess"
        ;;
    configsub)
        echo "tar/config.sub"
        ;;
    *)
        echo "tar/$(basename $(expand ${1}_url))"
        ;;
    esac
}

fetch() {
    # Fetch the specified package
    # $1  = package shortname
    local url
    url="$(expand ${1}_url)"
    mkdir -p tar
    if [ ! -e "$(tarpath $1)" ] ; then
        echo "Fetching ${1}..."
        case "$1" in
        configguess|configsub)
            wget -q -O "$(tarpath $1)" "$url"
            ;;
        *)
            wget -P tar -q "$url"
            ;;
        esac
    fi
}

unpack() {
    # Remove the package build directory and re-unpack it
    # $1  = package shortname
    local path
    fetch "${1}"
    mkdir -p "${build}"
    path="${build}/$(expand ${1}_build)"
    echo "Unpacking ${1}..."
    rm -rf "${path}"
    tar xf "$(tarpath $1)" -C "${build}"
}

is_built() {
    # Return true if the specified package is already built
    # $1  = package shortname
    local artifact
    for artifact in $(expand ${1}_artifacts)
    do
        if [ ! -e "${root}/lib/${artifact}" ] ; then
            return 1
        fi
    done
    return 0
}

do_configure() {
    if [ "$1" == "install_in_gst" ]
    then
        shift
        prefix="${gst}"
    else
        prefix="${root}"
    fi
    # Run configure with the appropriate parameters.
    # Additional parameters can be specified as arguments.
    #
    # Fedora's ${build_host}-pkg-config clobbers search paths; avoid it
    #
    # Use only our pkg-config library directory, even on cross builds
    # https://bugzilla.redhat.com/show_bug.cgi?id=688171
    ./configure \
            --host=${build_host} \
            --build=${build_system} \
            --prefix="${prefix}" \
            --enable-static \
            --disable-shared \
            --disable-dependency-tracking \
            PKG_CONFIG="pkg-config --static" \
            PKG_CONFIG_LIBDIR="${root}/share/pkgconfig:${root}/lib/pkgconfig:${gst}/lib-fixed/pkgconfig" \
            PKG_CONFIG_PATH= \
            CPPFLAGS="${cppflags} -I${root}/include -I${gst}/include" \
            CFLAGS="${cflags}" \
            CXXFLAGS="${cxxflags}" \
            LDFLAGS="${ldflags} -L${root}/lib -L${gst}/lib-fixed" \
            "$@"
}

build_one() {
    # Build the specified package if not already built
    # $1  = package shortname
    local basedir builddir

    if is_built "$1" ; then
        return
    fi

    unpack "$1"

    echo "Building ${1}..."
    basedir="$(pwd)"
    builddir="${build}/$(expand ${1}_build)"
    pushd "$builddir" >/dev/null
    case "$1" in
    celt)
        cp "${basedir}/$(tarpath configsub)" .
        do_configure \
                --without-ogg
        cd libcelt
        make $parallel
        make install
        cd ..
        mkdir -p "${root}/lib/pkgconfig"
        cp -a celt051.pc "${root}/lib/pkgconfig"
        ;;
    openssl)
        local os
        case "$abi" in
        armeabi)
            os=android
            ;;
        armeabi-v7a)
            os=android-armv7
            ;;
        x86)
            os=android-x86
            ;;
        *)
            echo "Unsupported ABI: $abi"
            exit 1
            ;;
        esac
        ./Configure \
                "${os}" \
                --prefix="$root" \
                --cross-compile-prefix="${build_host}-" \
                no-zlib \
                no-hw \
                no-ssl2 \
                no-ssl3 \
                ${cppflags} \
                ${cflags} \
                ${ldflags}
        make depend
        make
        make install_sw
        ;;
    spiceprotocol)
        autoreconf -fi
        do_configure
        make $parallel
        make install
        ;;
    spicegtk)
        autoreconf -fi
        do_configure \
                --with-gtk=no \
                --enable-dbus=no \
                --enable-controller=no \
                --with-audio=gstreamer \
                LIBS="-lm"
        patch -p1 < "${basedir}/spice-gtk-exit.patch"
        make $parallel

        # Patch to avoid SIGBUS due to unaligned accesses on ARM7
        patch -p1 < "${basedir}/spice-marshaller-sigbus.patch"
        make $parallel

        make install

        # Put some header files in a version-independent location.
        for f in config.h src/*.h spice-common/common
        do
            rsync -a $f ${root}/include/spice-1/
        done

        ;;
    soup)
        #gtkdocize
        echo "EXTRA_DIST =" > gtk-doc.make
        echo "CLEANFILES =" >> gtk-doc.make
        intltoolize --automake --copy
        autoreconf -fi
        do_configure \
                --enable-introspection=no \
                --without-gnome
        make $parallel
        make install
        ;;
    rest)
        echo "EXTRA_DIST =" > gtk-doc.make
        echo "CLEANFILES =" >> gtk-doc.make
        autoreconf -fi
        do_configure \
                --enable-introspection=no \
                --without-gnome \
                --disable-gtk-doc
        make $parallel
        make install
        ;;
    govirt)
        ./autogen.sh || /bin/true
        patch -p0 < "${basedir}/libgovirt-status.patch"
        patch -p0 < "${basedir}/libgovirt-tests.patch"
        do_configure \
                --enable-introspection=no \
                --without-gnome \
                --enable-tests=no
        make $parallel
        make install
        ;;
    usb)
        do_configure \
                --disable-udev
        make $parallel
        make install
        ;;
    usbredir)
        autoreconf -fi
        do_configure
        make $parallel
        make install
        ;;
    gnutls)
        # Build in normal root once to create artifact
        do_configure \
                --disable-crywrap \
                --without-p11-kit \
                --disable-doc \
                --disable-tests
        make $parallel
        make install

        # Build again over top of gstreamer's gnutls to upgrade it
        do_configure install_in_gst \
                --disable-crywrap \
                --without-p11-kit \
                --disable-doc \
                --disable-tests
        make $parallel
        make install
        ;;
    esac

    popd >/dev/null
}

sdist() {
    # Build source distribution
    local package tardir
    tardir="vmnetx-android-dependencies"
    rm -rf "${tardir}"
    mkdir -p "${tardir}"
    for package in $packages
    do
        fetch "$package"
        cp "$(tarpath ${package})" "${tardir}"
    done
    rm -f "${tardir}.tar.gz"
    tar czf "${tardir}.tar.gz" "${tardir}"
    rm -r "${tardir}"
}

setup() {
    # Configure the build environment and set up variables
    # $1 = ABI
    local system_arg
    if [ -z "${origpath}" ] ; then
        origpath="$PATH"
    fi

    cppflags=""
    cflags="-O2"
    cxxflags="${cflags}"
    ldflags=""

    abi="${1}"
    case "$abi" in
    armeabi)
        gstarch=arm
        arch=arm
        build_host="arm-linux-androideabi"
        ;;
    armeabi-v7a)
        gstarch=armv7
        arch=arm
        build_host="arm-linux-androideabi"
        cflags="${cflags} -march=armv7-a -mfloat-abi=softfp -mfpu=neon"
        ldflags="${ldflags} -march=armv7-a -Wl,--fix-cortex-a8"
        ;;
    arm64-v8a)
        gstarch=arm64
        arch=arm64
        build_host="aarch64-linux-android"
        ;;
    x86)
        gstarch=x86
        arch=x86
        build_host="i686-linux-android"
        ;;
    x86-64)
        gstarch=x86_64
        arch=x86_64
        build_host="x86_64-linux-android"
        ;;
    *)
        echo "Unknown ABI: $abi"
        exit 1
    esac

    build="deps/${abi}/build"
    root="$(pwd)/deps/${abi}/root"
    toolchain="$(pwd)/deps/${abi}/toolchain"
    gst="$(pwd)/deps/${abi}/gstreamer"
    mkdir -p "${root}"

    fetch configguess
    build_system=$(sh tar/config.guess)

    # Set up build environment
    if ! [ -e "${toolchain}/bin/${build_host}-gcc" ] ; then
        if [ -z "${ndkdir}" ] ; then
            echo "No toolchain configured and NDK directory not set."
            exit 1
        fi
        ${ndkdir}/build/tools/make_standalone_toolchain.py \
                --api "${android_api}" \
                --arch "${arch}" \
                --install-dir "${toolchain}"
    fi
    if ! [ -e "${toolchain}/bin/${build_host}-gcc" ] ; then
        echo "Couldn't configure compiler."
        exit 1
    fi
    PATH="${toolchain}/bin:${origpath}"
}

build() {
    # Build binaries
    # $1 = ABI
    local package pkgstr origroot

    # Set up build environment
    setup "$1"
    fetch configsub

    # Unpack GStreamer SDK
    if [ ! -e "${gst}/lib/libglib-2.0.a" ] ; then
        pkgstr="gstreamer_$(echo ${abi} | tr -d -)"
        fetch "${pkgstr}"
        echo "Unpacking ${pkgstr}..."
        rm -rf "${gst}-${abi}"
        mkdir -p "${gst}-${abi}"
        tar xf "$(tarpath ${pkgstr})" -C "${gst}-${abi}"
        ln -s "${gst}-${abi}/${gstarch}" "${gst}"
        # The .la files point to shared libraries that don't exist, so
        # linking fails.  We can't delete the .la files outright because
        # the GStreamer ndk-build glue depends on them.  Create a separate
        # lib directory with no .la files.
        cp -a "${gst}/lib" "${gst}/lib-fixed"
        rm -f ${gst}/lib-fixed/*.la
        # Fix paths in .pc files
        origroot=$(grep '^prefix' "${gst}/lib/pkgconfig/gstreamer-1.0.pc" | \
                sed -e 's/prefix=//')
        sed -i -e "s|${origroot}/lib|${gst}/lib-fixed|g" \
               -e "s|${origroot}|${gst}|g" \
                ${gst}/lib-fixed/pkgconfig/*.pc
        # Add pkg-config file for libjpeg so Android.mk can ask for its
        # symbols to be exposed in the gstreamer .so
        cat > ${gst}/lib/pkgconfig/jpeg.pc <<EOF
prefix=${origroot}
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: jpeg
Description: JPEG library
Version: 8
Libs: -L\${libdir} -ljpeg
Cflags: -I\${includedir}
EOF
        # Drop pkg-config file for opus, since static libopus and static
        # libcelt051 can't be linked into the same binary due to symbol
        # conflicts, and RHEL's libspice-server doesn't link with opus
        rm -f ${gst}/lib-fixed/pkgconfig/opus.pc
    fi

    # Build
    for package in $packages
    do
        build_one "$package"
    done
}

clean() {
    # Clean built files
    local package artifact curabi
    if [ $# -gt 0 ] ; then
        for package in "$@"
        do
            echo "Cleaning ${package}..."
            for curabi in $abis; do
                setup "${curabi}"
                for artifact in $(expand ${package}_artifacts)
                do
                    rm -f "${root}/lib/${artifact}"
                done
            done
        done
    else
        echo "Cleaning..."
        rm -rf deps
    fi
}

updates() {
    # Report new releases of software packages
    local package url curver newver
    for package in $packages gstreamer
    do
        url="$(expand ${package}_upurl)"
        if [ -z "$url" ] ; then
            continue
        fi
        curver="$(expand ${package}_ver)"
        newver=$(wget -q -O- "$url" | \
                sed -nr "s%.*$(expand ${package}_upregex).*%\\1%p" | \
                sort -uV | \
                tail -n 1)
        if [ "${curver}" != "${newver}" ] ; then
            printf "%-15s %10s  => %10s\n" "${package}" "${curver}" "${newver}"
        fi
    done
}

fail_handler() {
    # Report failed command
    echo "Failed: $BASH_COMMAND (line $BASH_LINENO)"
    exit 1
}

build_freerdp() {
    pushd deps
    basedir="$(pwd)"

    missing_artifact="false"
    for abi in $abis
    do
      for f in ${freerdp_artifacts}
      do
        if [ ! -f ${freerdp_build}/client/Android/Studio/freeRDPCore/src/main/jniLibs/${abi}/${f} -a \
             ! -f ${freerdp_build}/client/Android/Studio/freeRDPCore/src/main/jniLibs.DISABLED/${abi}/${f} ]
        then
          missing_artifact="true"
        fi
      done
    done

    if [ $missing_artifact == "true" ]
    then
        if [ ! -d ${freerdp_build} ]
        then
            git clone ${freerdp_url}
        fi

        pushd ${freerdp_build}
        git fetch
        git checkout ${freerdp_ver}
        git reset --hard

        # Patch the config
        sed -i -e 's/CMAKE_BUILD_TYPE=.*/CMAKE_BUILD_TYPE=Release/'\
               -e 's/WITH_OPENH264=.*/WITH_OPENH264=1/'\
               -e "s/BUILD_ARCH=.*/BUILD_ARCH=\"${abis}\"/" ./scripts/android-build.conf

        echo 'set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWINPR_EXPORTS --sysroot=${ANDROID_SYSROOT}")' >>  winpr/CMakeLists.txt
        for f in winpr/CMakeLists.txt winpr/libwinpr/CMakeLists.txt libfreerdp/CMakeLists.txt client/common/CMakeLists.txt client/Android/CMakeLists.txt client/common/CMakeLists.txt
        do
            echo 'set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --sysroot=${ANDROID_SYSROOT}")' >> $f
            echo 'set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} --sysroot=${ANDROID_SYSROOT}")' >> $f
        done

        # Something wrong with NDK?
        sed -i 's/static int pthread_mutex_timedlock/int pthread_mutex_timedlock/' winpr/libwinpr/synch/wait.c

        patch -p0 < "${basedir}/../freerdp_drive-file.patch"
        patch -p1 < "${basedir}/../freerdp_cmakelists.patch"

        export ANDROID_NDK="${ndkdir}"
        ./scripts/android-build-freerdp.sh

        # Prepare the FreeRDPCore project for importing into Eclipse
        rm -f ../../../../../FreeRDP
        ln -s Opaque/jni/libs/deps/${freerdp_build} ../../../../../FreeRDP
        ln -s jniLibs client/Android/Studio/freeRDPCore/src/main/libs
        ln -s java client/Android/Studio/freeRDPCore/src/main/src
    fi
    popd
}

build_sqlcipher() {
    pushd deps
    if [ ! -f ${sqlcipher_build}/${sqlcipher_artifacts} ]
    then

        if [ ! -d ${sqlcipher_build} ]
        then
            git clone ${sqlcipher_url}
        fi

        pushd ${sqlcipher_build}
        git fetch
        git checkout v${sqlcipher_ver}
	git reset --hard

        export ANDROID_NDK_ROOT="${ndkdir}"
        make init

        rm -f libs/sqlcipher.jar
        make
        popd
    fi
    pushd ${sqlcipher_build}
    rsync -a ./libs/ ../../../../../bVNC/libs/
    popd
    popd
}


# Set up error handling
trap fail_handler ERR

# Parse command-line options
parallel=""
ndkdir=""
origdir="$(pwd)"

while getopts "j:n:" opt
do
    case "$opt" in
    j)
        parallel="-j${OPTARG}"
        ;;
    n)
        ndkdir="${OPTARG}"
        ;;
    esac
done
shift $(( $OPTIND - 1 ))

# Process command-line arguments
case "$1" in
sdist)
    sdist
    ;;
build)
    for curabi in $abis
    do
        build "$curabi"
    done
    build_sqlcipher
    build_freerdp

    echo
    echo "Now you can run ndk-build if building aSPICE."
    echo
    echo "Run the following command:"
    echo "cd ../../ ; ndk-build"
    echo

    ;;
clean)
    shift
    clean "$@"
    ;;
updates)
    updates
    ;;
*)
    cat <<EOF
Usage: $0 sdist
       $0 [-j<parallelism>] [-n<ndk-dir>] build
       $0 clean [package...]
       $0 updates

Packages:
$packages
EOF
    exit 1
    ;;
esac

exit 0
