LOCAL_PATH 	:= $(call my-dir)

include $(CLEAR_VARS)

CROSS_DIR 	:= /opt/android

spice_objs := $(CROSS_DIR)/lib/libintl.a \
    $(CROSS_DIR)/lib/libgio-2.0.a \
    $(CROSS_DIR)/lib/libgobject-2.0.a  \
    $(CROSS_DIR)/lib/libgthread-2.0.a  \
    $(CROSS_DIR)/lib/libgmodule-2.0.a \
    $(CROSS_DIR)/lib/libglib-2.0.a  \
    $(CROSS_DIR)/lib/libiconv.a  \
    $(CROSS_DIR)/lib/libjpeg-6.a   \
    $(CROSS_DIR)/lib/libpixman.a \
    $(CROSS_DIR)/lib/libssl.a \
    $(CROSS_DIR)/lib/libcrypto.a \
    $(CROSS_DIR)/lib/libcelt.a 

LOCAL_MODULE    := spice

LOCAL_SRC_FILES := channel-record.c channel-playback.c channel-cursor.c \
                   spice-cmdline.c coroutine_gthread.c spice-util.c \
                   spice-session.c spice-channel.c spice-marshal.c spice-glib-enums.c \
                   common/generated_client_demarshallers.c common/generated_client_demarshallers1.c \
                   common/generated_client_marshallers.c common/generated_client_marshallers1.c \
                   gio-coroutine.c channel-base.c channel-main.c spice-proxy.c bio-gsocket.c \
                   channel-display.c channel-display-mjpeg.c channel-inputs.c decode-glz.c \
                   decode-jpeg.c decode-zlib.c wocky-http-proxy.c channel-port.c spice-client.c \
                   common/mem.c common/marshaller.c common/canvas_utils.c common/backtrace.c \
                   common/sw_canvas.c common/pixman_utils.c common/lines.c common/rop3.c common/quic.c \
                   common/lz.c common/region.c common/ssl_verify.c common/log.c \
                   android/android-service.c android/android-spicy.c android/android-spice-widget.c \
                   android/android-io.c

LOCAL_LDLIBS 	+= $(spice_objs) \
		   -L$(CROSS_DIR)/lib \
		   -ljnigraphics \
		   -llog -ldl -lstdc++ -lz -lc \
		   -malign-double -malign-loops

LOCAL_CPPFLAGS 	+= -DG_LOG_DOMAIN=\"GSpice\" \
		   -DSW_CANVAS_CACHE \
		   -DSPICE_GTK_LOCALEDIR=\"/usr/local/share/locale\" \
		   -DHAVE_CONFIG_H -UHAVE_SYS_SHM_H -DSW_CANVAS_CACHE  \
		   -D_REENTRANT 

LOCAL_C_INCLUDES += $(LOCAL_PATH)/common \
                    $(CROSS_DIR)/include/spice-1 \
		    $(CROSS_DIR)/include/pixman-1 \
		    $(CROSS_DIR)/include \
 		    $(CROSS_DIR)/include/glib-2.0 \
		    $(CROSS_DIR)/lib/glib-2.0/include

LOCAL_CFLAGS 	:= $(LOCAL_CPPFLAGS) \
    -std=gnu99 -Wall -Wno-sign-compare -Wno-deprecated-declarations -Wl,--no-undefined \
    -fPIC -DPIC -O3 -funroll-loops -ffast-math

LOCAL_ARM_MODE := arm

include $(BUILD_SHARED_LIBRARY)
