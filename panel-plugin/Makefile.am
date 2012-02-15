INCLUDES =								\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"xfce4-systemload-plugin\"			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

plugindir =								\
	$(libdir)/xfce4/panel/plugins
	
plugin_LTLIBRARIES =							\
	libsystemload.la

libsystemload_la_SOURCES =					\
	systemload.c							\
	uptime.c							\
	uptime.h							\
	memswap.c							\
	memswap.h							\
	cpu.c								\
	cpu.h

libsystemload_la_CFLAGS =					\
	$(GTK_CFLAGS)							\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	$(UPOWER_GLIB_CFLAGS)						\
	$(PLATFORM_CFLAGS)

libsystemload_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libsystemload_la_LIBADD =						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCE4UI_LIBS)						\
	$(LIBXFCE4PANEL_LIBS) 						\
	$(UPOWER_GLIB_LIBS)

#
# Desktop file
#
desktopdir =								\
	$(datadir)/xfce4/panel/plugins

desktop_DATA =								\
	systemload.desktop

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	systemload.desktop.in

CLEANFILES =								\
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
