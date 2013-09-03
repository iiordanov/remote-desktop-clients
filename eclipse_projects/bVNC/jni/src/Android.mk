LOCAL_PATH 	:= $(call my-dir)

include $(CLEAR_VARS)


SPICE_CLIENT_ANDROID_DEPS   := $(LOCAL_PATH)/../libs/deps.mini
spice_objs := \
    $(SPICE_CLIENT_ANDROID_DEPS)/lib/libssl.a \
    $(SPICE_CLIENT_ANDROID_DEPS)/lib/libcrypto.a \
    $(SPICE_CLIENT_ANDROID_DEPS)/lib/libcelt051.a 

LOCAL_MODULE    := spice

LOCAL_SRC_FILES := channel-record.c channel-playback.c channel-cursor.c \
                   spice-cmdline.c coroutine_gthread.c spice-util.c \
                   spice-session.c spice-channel.c spice-marshal.c spice-glib-enums.c \
                   common/generated_client_demarshallers.c common/generated_client_demarshallers1.c \
                   common/generated_client_marshallers.c common/generated_client_marshallers1.c \
                   gio-coroutine.c channel-base.c channel-main.c spice-proxy.c bio-gsocket.c glib-compat.c \
                   channel-display.c channel-display-mjpeg.c channel-inputs.c decode-glz.c \
                   decode-jpeg.c decode-zlib.c wocky-http-proxy.c channel-port.c spice-client.c spice-audio.c \
                   common/mem.c common/marshaller.c common/canvas_utils.c common/backtrace.c \
                   common/sw_canvas.c common/pixman_utils.c common/lines.c common/rop3.c common/quic.c \
                   common/lz.c common/region.c common/ssl_verify.c common/log.c

LOCAL_SRC_FILES += spice-gstaudio.c
LOCAL_LDLIBS 	+= $(spice_objs) \
		   -ljnigraphics \
		   -llog -ldl -lstdc++ -lz -lc \
		   -malign-double -malign-loops

LOCAL_CPPFLAGS 	+= -DG_LOG_DOMAIN=\"GSpice\" \
       -DSW_CANVAS_CACHE \
       -DSPICE_GTK_LOCALEDIR=\"/usr/local/share/locale\" \
       -DHAVE_CONFIG_H -UHAVE_SYS_SHM_H -DSW_CANVAS_CACHE  \
       -D_REENTRANT -DWITH_GSTAUDIO

LOCAL_C_INCLUDES += $(LOCAL_PATH)/common \
        $(SPICE_CLIENT_ANDROID_DEPS)/include

LOCAL_CFLAGS 	:= $(LOCAL_CPPFLAGS) \
    -std=gnu99 -Wall -Wno-sign-compare -Wno-deprecated-declarations -Wl,--no-undefined \
    -fPIC -DPIC -O3 -funroll-loops -ffast-math

LOCAL_EXPORT_CFLAGS += $(LOCAL_CFLAGS)
LOCAL_EXPORT_LDLIBS += $(LOCAL_LDLIBS)
LOCAL_SHARED_LIBRARIES := gstreamer_android
LOCAL_ARM_MODE := arm

include $(BUILD_STATIC_LIBRARY)
include $(CLEAR_VARS)
LOCAL_MODULE    := spice-android
LOCAL_SRC_FILES := android/android-service.c android/android-spicy.c android/android-spice-widget.c \
                   android/android-io.c
LOCAL_STATIC_LIBRARIES := spice
include $(BUILD_SHARED_LIBRARY)
include $(CLEAR_VARS)
ifndef GSTREAMER_SDK_ROOT
ifndef GSTREAMER_SDK_ROOT_ANDROID
$(error GSTREAMER_SDK_ROOT_ANDROID is not defined!)
endif
GSTREAMER_SDK_ROOT        := $(GSTREAMER_SDK_ROOT_ANDROID)
endif
GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_SDK_ROOT)/share/gst-android/ndk-build/
include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk
GSTREAMER_PLUGINS         := $(GSTREAMER_PLUGINS_CORE) $(GSTREAMER_PLUGINS_SYS) jpeg
GSTREAMER_EXTRA_DEPS      := glib-2.0 gthread-2.0 pixman-1 gstreamer-app-0.10
 
include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer.mk
