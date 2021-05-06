FROM ubuntu:bionic

ARG USER_UID
ARG USER_GID
ARG CURRENT_WORKING_DIR
ARG ANDROID_SDK

RUN apt update && \
apt install -yy gnome-common gobject-introspection nasm openjdk-8-jdk build-essential git python-setuptools python3-setuptools wget curl unzip sudo rsync && \
apt clean && apt autoclean

# This layer is needed by cerbero to avoid an interactive sudo apt-get failing, needs to be update with subsequent cerbero versions
RUN sudo apt -yy install autotools-dev automake autoconf libtool g++ autopoint make cmake bison flex yasm pkg-config gtk-doc-tools \
libxv-dev libx11-dev libpulse-dev python3-dev texinfo gettext build-essential pkg-config doxygen curl libxext-dev libxi-dev \
x11proto-record-dev libxrender-dev libgl1-mesa-dev libxfixes-dev libxdamage-dev libxcomposite-dev libasound2-dev libxml-simple-perl \
dpkg-dev debhelper build-essential devscripts fakeroot transfig gperf libdbus-glib-1-dev wget glib-networking libxtst-dev libxrandr-dev \
libglu1-mesa-dev libegl1-mesa-dev git subversion xutils-dev intltool ccache python3-setuptools python3-pip libssl-dev chrpath libfuse-dev

# This layer is needed to install spice-gtk dependencies, requires pyparsing
RUN python3 -m pip install pyparsing

RUN mkdir -p $ANDROID_SDK $CURRENT_WORKING_DIR

RUN groupadd -g $USER_GID remote-clients
RUN useradd -u $USER_UID -g $USER_GID -m remote-clients

RUN echo "remote-clients ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

USER remote-clients
WORKDIR ${CURRENT_WORKING_DIR}

CMD export PROJECT=libs && export ANDROID_SDK=$ANDROID_SDK && export PATH=$PATH:$ANDROID_SDK/platform-tools/ && export PATH=$PATH:$ANDROID_SDK/tools && \
yes | $ANDROID_SDK/tools/bin/sdkmanager --licenses && \
./bVNC/prepare_project.sh libs $ANDROID_SDK
