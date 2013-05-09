NULL =

AM_CPPFLAGS = $(GIO_CFLAGS) $(PROTOCOL_CFLAGS)
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

if OS_WIN32
AM_VALAFLAGS += --pkg gio-windows-2.0 -D WIN32
else
AM_VALAFLAGS += --pkg gio-unix-2.0
endif

lib_LTLIBRARIES = libspice-controller.la
noinst_PROGRAMS = test-controller

BUILT_SOURCES = controller.vala.stamp

libspice_controller_la_VALASOURCES =		\
	menu.vala				\
	controller.vala				\
	$(NULL)
libspice_controller_la_SOURCES =		\
	custom.h				\
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

controller.vala.stamp: $(libspice_controller_la_VALASOURCES) custom.vapi
	$(VALA_V)$(VALAC) $(VALAFLAGS) $(AM_VALAFLAGS) $(libspice_controller_la_VALASOURCES) -H spice-controller.h
	@touch $@

EXTRA_DIST =					\
	$(libspice_controller_la_VALASOURCES)	\
	controller.vala.stamp			\
	custom.vapi				\
	gio-windows-2.0.vapi			\
	spice-controller.h			\
	$(NULL)

vala-clean:
	rm -f `find . -name "*.stamp"`

