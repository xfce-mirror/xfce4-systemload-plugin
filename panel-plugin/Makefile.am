plugindir = @XFCE4_PANEL_PLUGINSDIR@

plugin_LTLIBRARIES =							\
	libsystemload.la

libsystemload_la_LDFLAGS =						\
	-avoid-version							\
	-export-dynamic							\
	-module

libsystemload_la_SOURCES =						\
	systemload.c							\
	uptime.c							\
	uptime.h							\
	memswap.c							\
	memswap.h							\
	cpu.c								\
	cpu.h

libsystemload_la_CFLAGS =						\
        -DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	@XFCE4_PANEL_CFLAGS@

libsystemload_la_LIBADD = 						\
	@XFCE4_PANEL_LIBS@
