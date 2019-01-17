LOCAL_PATH 	:= $(call my-dir)
COMMON_ROOT	:= ../libs/deps/$(TARGET_ARCH_ABI)
PREBUILT_ROOT   := $(COMMON_ROOT)/root
GSTREAMER_ROOT  := $(COMMON_ROOT)/gstreamer

include $(CLEAR_VARS)
LOCAL_MODULE            := opus
LOCAL_SRC_FILES         := $(GSTREAMER_ROOT)/lib/libopus.a
LOCAL_EXPORT_C_INCLUDES := $(GSTREAMER_ROOT)/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE            := iconv
LOCAL_SRC_FILES         := $(GSTREAMER_ROOT)/lib/libiconv.a
LOCAL_EXPORT_C_INCLUDES := $(GSTREAMER_ROOT)/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE            := intl
LOCAL_SRC_FILES         := $(GSTREAMER_ROOT)/lib/libintl.a
LOCAL_EXPORT_C_INCLUDES := $(GSTREAMER_ROOT)/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE            := usb
LOCAL_SRC_FILES         := $(PREBUILT_ROOT)/lib/libusb-1.0.a
LOCAL_EXPORT_C_INCLUDES := $(PREBUILT_ROOT)/include/libusb-1.0
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE            := usbredirhost
LOCAL_SRC_FILES         := $(PREBUILT_ROOT)/lib/libusbredirhost.a
LOCAL_EXPORT_C_INCLUDES := $(PREBUILT_ROOT)/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE            := usbredirparser
LOCAL_SRC_FILES         := $(PREBUILT_ROOT)/lib/libusbredirparser.a
LOCAL_EXPORT_C_INCLUDES := $(PREBUILT_ROOT)/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE            := rest
LOCAL_SRC_FILES         := $(PREBUILT_ROOT)/lib/librest-0.7.a
LOCAL_EXPORT_C_INCLUDES := $(PREBUILT_ROOT)/include/rest-0.7
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE            := govirt
LOCAL_SRC_FILES         := $(PREBUILT_ROOT)/lib/libgovirt.a
LOCAL_EXPORT_C_INCLUDES := $(PREBUILT_ROOT)/include/govirt-1.0
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE            := libcrypto
LOCAL_SRC_FILES         := $(PREBUILT_ROOT)/lib/libcrypto.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/$(PREBUILT_ROOT)/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE            := libssl
LOCAL_SRC_FILES         := $(PREBUILT_ROOT)/lib/libssl.a
LOCAL_STATIC_LIBRARIES  := libcrypto
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/$(PREBUILT_ROOT)/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE            := spice-client-glib
LOCAL_SRC_FILES         := $(PREBUILT_ROOT)/lib/libspice-client-glib-2.0.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/$(PREBUILT_ROOT)/include/spice-client-glib-2.0 \
                           $(LOCAL_PATH)/$(PREBUILT_ROOT)/include/spice-1
LOCAL_SHARED_LIBRARIES  := gstreamer_android
LOCAL_STATIC_LIBRARIES  := opus libssl
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
GSTREAMER_ROOT		  := $(LOCAL_PATH)/$(COMMON_ROOT)/gstreamer
GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/
GSTREAMER_JAVA_SRC_DIR	  := java
include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk
GSTREAMER_PLUGINS         := $(GSTREAMER_PLUGINS_CORE) $(GSTREAMER_PLUGINS_SYS)
G_IO_MODULES              := gnutls
GSTREAMER_EXTRA_DEPS      := pixman-1 gstreamer-app-1.0 libsoup-2.4 libxml-2.0 glib-2.0 gthread-2.0 gobject-2.0 jpeg
include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk


include $(CLEAR_VARS)
LOCAL_MODULE    := spice

LOCAL_CPPFLAGS  += -DSW_CANVAS_CACHE \
                   -DSPICE_GTK_LOCALEDIR=\"/usr/local/share/locale\" \
                   -DHAVE_CONFIG_H -UHAVE_SYS_SHM_H -DSW_CANVAS_CACHE  \
                   -D_REENTRANT -DWITH_GSTAUDIO

LOCAL_C_INCLUDES += \
                    $(LOCAL_PATH)/$(PREBUILT_ROOT)/include/spice-1 \
                    $(LOCAL_PATH)/$(PREBUILT_ROOT)/include/govirt-1.0 \
                    $(LOCAL_PATH)/$(PREBUILT_ROOT)/include/rest-0.7 \
                    $(LOCAL_PATH)/$(PREBUILT_ROOT)/include/libusb-1.0 \
                    $(LOCAL_PATH)/virt-viewer

LOCAL_SRC_FILES := virt-viewer/virt-viewer-file.c virt-viewer/virt-viewer-util.c \
                   android/android-service.c android/android-spicy.c android/android-spice-widget.c \
                   android/android-io.c

LOCAL_LDLIBS 	+= -ljnigraphics -llog

LOCAL_CPPFLAGS  += -DG_LOG_DOMAIN=\"android-spice\"

LOCAL_CFLAGS 	:=  $(LOCAL_CPPFLAGS) \
                   -std=gnu99 -Wall -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wl,--no-undefined \
                   -O3 -funroll-loops

LOCAL_EXPORT_CFLAGS += $(LOCAL_CFLAGS)
LOCAL_EXPORT_LDLIBS += $(LOCAL_LDLIBS)
LOCAL_ARM_MODE := arm
LOCAL_SHARED_LIBRARIES := gstreamer_android
LOCAL_STATIC_LIBRARIES := spice-client-glib govirt rest usb usbredirhost usbredirparser iconv intl
LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
include $(BUILD_SHARED_LIBRARY)
