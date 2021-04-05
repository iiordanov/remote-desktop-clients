# Intro

This is the source code for bVNC, aRDP, aSPICE and Opaque, four remote desktop
clients for Android.

Please see the LICENSE file for information on how the source is licensed.

# Building

There are different ways to build the applications depending on OS and whether you are using
pre-built libraries, or building them from scratch.

## On Linux and WSL2

These instructions should work on Ubuntu 18.04, 20.04, and Windows Subsystem for Linux 2.
Builds are likely to also work on MacOS, feedback is welcome.

Pick one of I-a, I-b, or I-c below, then move onto II.

### I-a With Prebuilt Libraries

Building the projects with pre-built dependencies.

        ./download-prebuilt-dependencies.sh
        ./bVNC/prepare_project.sh --skip-build libs nopath

### I-b From Scratch with Docker

Make sure you're running the commands below from the root of the project.
Ensure ANDROID_SDK is set to the path to your SDK.

```bash
export ANDROID_SDK=${HOME}/Android/Sdk
echo "USER_UID=$(id -u)" > docker/.env
echo "USER_GID=$(id -g)" >> docker/.env
echo "ANDROID_SDK=${ANDROID_SDK}" >> docker/.env
echo "CURRENT_WORKING_DIR=$(pwd)" >> docker/.env
docker-compose -f docker/docker-compose.yml up
```

### I-c From Scratch

Building from scratch and working in Android Studio.

  - Install some packages. On Ubuntu:
        apt install gnome-common gobject-introspection nasm gtk-doc-tools

  - On Linux, install Android Studio
  - Install Android SDK from Tools -> SDK Tools
  - Install Android SDK command-line tools and CMake from the SDK Tools tab
  - Ensure that the path to ANDROID_SDK is ${HOME}/Android/Sdk/ and correct below if necessary

  - To build the projects

    - If building a non-custom client, set PROJECT to libs. For a custom VNC client, set PROJECT to a string
      that stars with Custom and contains Vnc, i.e. `CustomYourVncClient` (see III below for details).

    - Set the environment variables ANDROID_SDK to your SDK installation. The scripts will install the NDK automatically.

    - Example:

              export PROJECT=libs # or CustomSomethingOrOther
              export ANDROID_SDK=${HOME}/Android/Sdk/
              export PATH=$PATH:${ANDROID_SDK}/platform-tools/
              export PATH=$PATH:${ANDROID_SDK}/tools

    - Accept all licenses (repeat if you see an error during build)

              ${ANDROID_SDK}/tools/bin/sdkmanager --licenses

    - Then, run the build script which takes hours to run. E.g.:

              ./bVNC/prepare_project.sh $PROJECT $ANDROID_SDK

    - Switch to Android Studio, select the launch configuration you want to run, and run it on an emulator or device.

    - If using an emulator, choose x86_64 as the architecture to avoid "has text relocations" errors loading gstreamer on Android.

## On Windows with Git Bash

These instructions are for Windows without WSL2 installed. For now, only pre-build dependencies are
supported in this configuration.

- First [download and install install git from here](https://git-scm.com/downloads)
- Then, start Git Bash, clone this project, cd into the project root directory and run:
```bash
./download-prebuilt-dependencies.sh
```
- Start Android Studio and open the project directory
- Click File->Sync Project with Gradle Files
- If Android Studio reports any missing android versions (such as `android-28`, for instance),
find and start SDK Manager and ensure any missing Android versions are installed. As of today
the required versions are `android-28`, `android-29`, and `android-30`, but in future other versions
will need to be installed if Android Studio shows an error.


## II Importing projects into Android Studio

This should be as simple as selecting "Open an existing Android Studio project" on the
Welcome screen, browsing to the remote-desktop-clients directory and selecting it.

  - One final tweak is necessary to the (external) freeRDPCore project before
    the project can build. Double-click "Gradle Scripts" on the left, and
    open build.gradle (Module freeRDPCore). Change minSdkVersion to 11.

  - Build -> Make Project should now work.

## Custom Certificate Authority

You can add custom CAs for aSPICE and Opaque in remoteClientLib/certificate_authorities/. They will be merged with the
ca-bundle.crt provided to the app to validate your self-signed server certs if you have any.

## Generating Keyboard Layouts for aSPICE and Opaque

The directory `bVNC/layouts` contains a utiliy `convert.py` that can be used to generate new layouts for the desktop clients.

```bash
sudo apt install qemu-keymaps
cd bVNC/layouts
python3 ./convert.py
```

If you would like to add a layout, the best way to do so would be to create a file with the correct format in
`/usr/share/qemu/keymaps/`. Ideally, you should submit this file to the qemu project so everybody would be able to benefit
from the new layout.


## III Building "Custom" VNC clients

It is possible to programmatically build additional customized clients based on the VNC client contained in this project
without altering any of the source code of the project.

- Pick a unique identifier for your app. It will become part of the
  (application ID)[https://developer.android.com/studio/build/application-id] of the app. For example,
  say you pick YourVncClient. The application ID will be com.iiordanov.YourVncClient.

- Place a configuration file in yaml format (note - with filename matching your resulting application ID) at
  `bVNC/src/main/assets/com.iiordanov.YourVncClient.yaml`

- Use the file `custom_vnc_client.yaml-EXAMPLE` as a starting point. The numbers after each field are one of
  View.INVISIBLE or View.GONE and it controls whether the field is invisible or gone in the customized interface.

- See [https://developer.android.com/reference/android/view/View.html#GONE] for the numeric value of View.GONE
  and [https://developer.android.com/reference/android/view/View.html#INVISIBLE] for the numberic value of View.INVISIBLE.

- Place an icon at `CustomVnc-app/src/main/res/drawable/icon_of_the_custom_app.png`.

- Edit `gradle.properties` and set CUSTOM_VNC_APP_NAME to `Name Of The Custom App` and CUSTOM_VNC_APP_ICON to `icon_of_the_custom_app`

- Follow the build procedure in I-a, I-b, or I-c above, but with the PROJECT environment variable set to anything that starts with
  `Custom` and has `Vnc` in its name. For instance, if you set PROJECT to `CustomYourVncClient`. The `bVNC/prepare_project.sh` script
  will strip "Custom" from that identifier and once the project is built will be com.iiordanov.YourVncClient.

## Bugs

Please post any bugs you find at the github issue tracker:

https://github.com/iiordanov/remote-desktop-clients/issues

## Support Forum

Questions and general discussion should be posted at the following forum:

https://groups.google.com/forum/#!forum/bvnc-ardp-aspice-opaque-remote-desktop-clients