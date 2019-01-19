# Intro

This is the source code for bVNC, aRDP, aSPICE and Opaque, four remote desktop
clients for Android.

Please see the LICENSE file for information on how the source is licensed.


# Building

There are two ways to build the applications. With pre-built libraries, or from
scratch.

The build automation of requires Linux at the moment, due to the use of symlinks.

Building from scratch (I-b) is known to build with Android NDK r14b.

Please note that while for now there is a reference to eclipse_projects
in the directory structure of the project, you no longer have to work
in Eclipse to develop bVNC.

Pick one of I-a or I-b below, then move onto II.

## I-a With Prebuilt Libraries

Building bVNC with pre-built dependencies.

  - Set the environment variable PROJECT to one of bVNC, aSPICE, or aRDP.

    export PROJECT=bVNC
    ./eclipse_projects/download-prebuilt-dependencies.sh
    ./eclipse_projects/bVNC/copy_prebuilt_files.sh $PROJECT  
    ./eclipse_projects/bVNC/prepare_project.sh --skip-build $PROJECT nopath nopath


## I-b From Scratch

Building from scratch and working in Android Studio.

  - On Linux, install Android Studio, Android SDK, and Android NDK

  - Install at least the following dependencies on your Linux machine:
    cmake automake libtool intltool gtk-doc-tools gnome-common gobject-introspection nasm

  - To build bVNC, aSPICE, or aRDP

    - Set the environment variable PROJECT to one of bVNC, aSPICE, or aRDP.
      Set the environment variables ANDROID_SDK and ANDROID_NDK to your SDK and NDK respectively.

      export PROJECT=bVNC
      export ANDROID_NDK=/path/to/your/android/NDK/
      export ANDROID_SDK=/path/to/your/android/SDK/

      export PATH=$PATH:${ANDROID_NDK}
      export PATH=$PATH:${ANDROID_SDK}/platform-tools/
      export PATH=$PATH:${ANDROID_SDK}/tools

    - Accept all licenses (repeat if you see an error during build)
      ${ANDROID_SDK}/tools/bin/sdkmanager --licenses

    - Then, run the build script. E.g.:

      ./eclipse_projects/bVNC/prepare_project.sh $PROJECT $ANDROID_NDK $ANDROID_SDK

    - Follow the instructions that the script outputs.


## II Importing projects into Android Studio

This should be as simple as selecting "Open an existing Android Studio project" on the
Welcome screen, browsing to the remote-desktop-clients directory and selecting it.

  - One final tweak is necessary to the (external) freeRDPCore project before
    the project can build. Double-click "Gradle Scripts" on the left, and
    open build.gradle (Module freeRDPCore). Change minSdkVersion to 11.

  - Build -> Make Project should now work.
