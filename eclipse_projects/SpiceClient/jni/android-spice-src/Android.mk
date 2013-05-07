LOCAL_PATH 	:= $(call my-dir)

include $(CLEAR_VARS)

CROSS_DIR 	:= /opt/android

libspicec_link_objs 	:= $(CROSS_DIR)/lib/libintl.a \
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

LOCAL_MODULE    := spicec

LOCAL_SRC_FILES := bio-gsocket.c channel-record.c channel-playback.c channel-cursor.c jpeg_encoder.c spicy.c spice-cmdline.c android-worker.c android-spice.c coroutine_gthread.c spice-util.c spice-session.c spice-channel.c spice-marshal.c spice-glib-enums.c generated_demarshallers.c generated_demarshallers1.c generated_marshallers.c generated_marshallers1.c gio-coroutine.c channel-base.c channel-main.c channel-display.c channel-display-mjpeg.c channel-inputs.c decode-glz.c decode-jpeg.c decode-zlib.c mem.c marshaller.c canvas_utils.c sw_canvas.c pixman_utils.c lines.c rop3.c quic.c lz.c region.c ssl_verify.c

LOCAL_LDLIBS 	+= $(libspicec_link_objs) \
		   -L$(CROSS_DIR)/lib \
		   -llog -ldl -lstdc++ -lz -lc 

LOCAL_CPPFLAGS 	+= -DG_LOG_DOMAIN=\"GSpice\" \
		   -DSW_CANVAS_CACHE \
		   -DSPICE_GTK_LOCALEDIR=\"/usr/local/share/locale\" \
		   -DHAVE_CONFIG_H -UHAVE_SYS_SHM_H -DSW_CANVAS_CACHE  \
		   -D_REENTRANT \

LOCAL_C_INCLUDES += $(CROSS_DIR)/include/spice-1 \
		    $(CROSS_DIR)/include/pixman-1 \
		    $(CROSS_DIR)/include \
		    $(CROSS_DIR)/include/glib-2.0 \
		    $(CROSS_DIR)/lib/glib-2.0/include

LOCAL_CFLAGS 	:= $(LOCAL_CPPFLAGS) \
    -std=gnu99 -Wall -Wno-sign-compare -Wno-deprecated-declarations -Wl,--no-undefined \
    -fPIC -DPIC 

include $(BUILD_SHARED_LIBRARY)
