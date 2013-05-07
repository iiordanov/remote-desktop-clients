NULL =

AM_CPPFLAGS =					\
	-DG_LOG_DOMAIN=\"GSpiceController\"	\
	$(GIO_CFLAGS)				\
	$(PROTOCOL_CFLAGS)			\
	$(NULL)

# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
AM_LDFLAGS =					\
	-no-undefined				\
	$(GIO_LIBS)				\
	$(NULL)

AM_VALAFLAGS =							\
	--pkg gio-2.0						\
	--pkg spice-protocol --vapidir=$(top_srcdir)/data	\
	--pkg custom --vapidir=$(srcdir)			\
	-C -g							\
	$(NULL)

lib_LTLIBRARIES = libspice-controller.la
noinst_PROGRAMS = test-controller spice-controller-dump

BUILT_SOURCES = controller.vala.stamp

libspice_controller_la_VALASOURCES =		\
	menu.vala				\
	controller.vala				\
	foreign-menu.vala			\
	$(NULL)
libspice_controller_la_SOURCES =		\
	custom.h				\
	spice-controller-listener.c		\
	spice-controller-listener.h		\
	spice-foreign-menu-listener.c		\
	spice-foreign-menu-listener.h		\
	$(libspice_controller_la_VALASOURCES:.vala=.c) \
	$(NULL)

if OS_WIN32
libspice_controller_la_SOURCES +=		\
	namedpipe.c				\
	namedpipe.h				\
	namedpipeconnection.c			\
	namedpipeconnection.h			\
	namedpipelistener.c			\
	namedpipelistener.h			\
	$(NULL)
endif
libspice_controller_la_LDFLAGS =		\
	$(AM_LDFLAGS)				\
	-version-number 0:0:0			\
	$(NULL)

libspice_controllerincludedir = $(includedir)/spice-controller
libspice_controllerinclude_HEADERS =		\
	spice-controller.h

test_controller_SOURCES = test.c
test_controller_LDADD = libspice-controller.la

spice_controller_dump_SOURCES = dump.c
spice_controller_dump_LDADD = libspice-controller.la

controller.vala.stamp: $(libspice_controller_la_VALASOURCES) custom.vapi
	@if test -z "$(VALAC)"; then \
		echo "" ; \
		echo "  *** Error: missing valac!" ; \
		echo "  *** You must run autogen.sh or configure --enable-vala" ; \
		echo "" ; \
		exit 1 ; \
	fi
	$(VALA_V)$(VALAC) $(VALAFLAGS) $(AM_VALAFLAGS) $(libspice_controller_la_VALASOURCES) -H spice-controller.h
	@touch $@

spice-controller.h: controller.vala.stamp
menu.c: controller.vala.stamp
controller.c: controller.vala.stamp

EXTRA_DIST =					\
	$(libspice_controller_la_VALASOURCES)	\
	controller.vala.stamp			\
	custom.vapi				\
	gio-windows-2.0.vapi			\
	spice-controller.h			\
	$(NULL)

vala-clean:
	rm -f `find . -name "*.stamp"`

