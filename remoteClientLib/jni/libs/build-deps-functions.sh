install_ndk() {
    DIR=$1
    VER=$2

    pushd ${DIR} >&/dev/null
    if [ ! -e android-ndk-${VER} ]
    then
        if wget --quiet -c https://dl.google.com/android/repository/android-ndk-${VER}-linux-x86_64.zip >&/dev/null
        then
            unzip android-ndk-${VER}-linux-x86_64.zip >&/dev/null
        else
            wget --quiet -c https://dl.google.com/android/repository/android-ndk-${VER}-linux.zip >&/dev/null
            unzip android-ndk-${VER}-linux.zip >&/dev/null
        fi
    fi
    popd >&/dev/null
    echo $(realpath ${DIR}/android-ndk-${VER})
}

install_cmake() {
    DIR=$1
    VER=$2

    pushd ${DIR} >&/dev/null
    if [ ! -d cmake-${VER}-Linux-x86_64 ]
    then
        wget --quiet -c https://github.com/Kitware/CMake/releases/download/v${VER}/cmake-${VER}-Linux-x86_64.tar.gz >&/dev/null
        tar xzf cmake-${VER}-Linux-x86_64.tar.gz >&/dev/null
    fi
    popd >&/dev/null
    echo $(realpath ${DIR}/cmake-${VER}-linux-x86_64)
}

