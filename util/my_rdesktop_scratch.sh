#!/bin/sh
export PROJECT=aRDP
export ANDROID_NDK=/opt/android-ndk-r14b/ 
export ANDROID_SDK=/home/hgode/Android/Sdk/

export PATH=$PATH:${ANDROID_NDK}
export PATH=$PATH:${ANDROID_SDK}/platform-tools/ 
export PATH=$PATH:${ANDROID_SDK}/tools
export PATH=/home/hgode/cmake-3.5.1-Linux-x86_64/bin:${PATH}

./eclipse_projects/bVNC/prepare_project.sh $PROJECT $ANDROID_NDK $ANDROID_SDK

