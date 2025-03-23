plugindir = $(libdir)/xfce4/panel/plugins
plugin_LTLIBRARIES = libsystemload.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-systemload-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

libsystemload_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBGTOP_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(UPOWER_GLIB_CFLAGS) \
	$(PLATFORM_CFLAGS)

libsystemload_la_CXXFLAGS = $(libsystemload_la_CFLAGS)

libsystemload_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4PANEL_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBGTOP_LIBS) \
	$(UPOWER_GLIB_LIBS) \
	-lm

libsystemload_la_SOURCES = \
	cpu.cc \
	cpu.h \
	memswap.cc \
	memswap.h \
	network.cc \
	network.h \
	plugin.h \
	plugin.c \
	settings.cc \
	settings.h \
	systemload.cc \
	uptime.cc \
	uptime.h

libsystemload_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

#
# Desktop file
#
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_DATA = systemload.desktop
systemload.desktop: systemload.desktop.in Makefile
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = \
	meson.build \
	systemload.desktop.in \
	$(NULL)

CLEANFILES = $(desktop_DATA)

