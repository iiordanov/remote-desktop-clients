FROM ubuntu:noble

ARG USER_UID
ARG USER_GID
ARG CURRENT_WORKING_DIR
ARG DEBIAN_FRONTEND=noninteractive

ENV BASEDIR=/home/remote-clients/
ENV ANDROID_SDK=/home/remote-clients/android-sdk

# This layer is needed by cerbero to avoid an interactive sudo apt-get failing, needs to be updated with subsequent cerbero versions
RUN apt update && \
apt install -yy gnome-common gobject-introspection nasm openjdk-17-jdk build-essential git python3-setuptools python3-setuptools wget curl unzip sudo rsync \
 autotools-dev automake autoconf libtool g++ autopoint make cmake bison flex yasm pkg-config gtk-doc-tools python-is-python3 libx11-xcb-dev \
 libxv-dev libx11-dev libpulse-dev python3-dev texinfo gettext build-essential pkg-config doxygen curl libxext-dev libxi-dev python3-distro \
 x11proto-record-dev libxrender-dev libgl1-mesa-dev libxfixes-dev libxdamage-dev libxcomposite-dev libasound2-dev libxml-simple-perl help2man \
 dpkg-dev debhelper build-essential devscripts fakeroot transfig gperf libdbus-glib-1-dev wget glib-networking libxtst-dev libxrandr-dev \
 libglu1-mesa-dev libegl1-mesa-dev git subversion xutils-dev intltool ccache python3-setuptools python3-pip libssl-dev chrpath libfuse-dev \
 libncurses6 libtinfo6 ninja-build python3*-venv python3-six python3-pyparsing \
  && apt clean && apt autoclean

RUN ln -sf /usr/lib/x86_64-linux-gnu/libncurses.so.6 /usr/lib/x86_64-linux-gnu/libncurses.so.5
RUN ln -sf /usr/lib/x86_64-linux-gnu/libtinfo.so.6 /usr/lib/x86_64-linux-gnu/libtinfo.so.5

RUN mkdir -p ${BASEDIR} ${ANDROID_SDK}

RUN groupadd -g ${USER_GID} remote-clients || true
RUN useradd -u ${USER_UID} -g ${USER_GID} -m remote-clients || true
RUN git config --global protocol.file.allow always

RUN chown ${USER_UID} -R ${BASEDIR} ${ANDROID_SDK}
RUN echo "$(id -nu ${USER_UID}) ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

WORKDIR ${CURRENT_WORKING_DIR}

RUN su $(id -nu 1000) -c 'git config --global protocol.file.allow always'

RUN su $(id -nu 1000) -c "cd ${BASEDIR} && curl -o ${BASEDIR}/commandlinetools-linux.zip https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip && \
 unzip ${BASEDIR}/commandlinetools-linux.zip && cmdline-tools/bin/sdkmanager --sdk_root=${ANDROID_SDK} 'platforms;android-34'"

CMD su $(id -nu 1000) -c "export PROJECT=libs && \
 export ANDROID_SDK=${ANDROID_SDK} && \
 export PATH=$PATH:${ANDROID_SDK}/platform-tools/ && \
 export PATH=$PATH:${ANDROID_SDK}/tools && \
 export PATH=$PATH:${ANDROID_SDK}/cmdline-tools/bin && \
 yes | ${BASEDIR}/cmdline-tools/bin/sdkmanager --sdk_root=${ANDROID_SDK} --licenses && \
 ./bVNC/prepare_project.sh libs ${ANDROID_SDK}"
