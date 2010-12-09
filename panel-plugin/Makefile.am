INCLUDES =								\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"xfce4-systemload-plugin\"			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

plugindir =								\
	$(libexecdir)/xfce4/panel-plugins
	
plugin_PROGRAMS =							\
	xfce4-systemload-plugin

xfce4_systemload_plugin_SOURCES =					\
	systemload.c							\
	uptime.c							\
	uptime.h							\
	memswap.c							\
	memswap.h							\
	cpu.c								\
	cpu.h

xfce4_systemload_plugin_CFLAGS =					\
	$(GTK_CFLAGS)							\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	$(PLATFORM_CFLAGS)

xfce4_systemload_plugin_LDADD =						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCEGUI4_LIBS)						\
	$(LIBXFCE4PANEL_LIBS)

#
# Desktop file
#
desktopdir =								\
	$(datadir)/xfce4/panel-plugins

desktop_in_in_files =							\
	systemload.desktop.in.in

desktop_in_files =							\
	$(desktop_in_in_files:.desktop.in.in=.desktop.in)

%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@

desktop_DATA =								\
	$(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(desktop_in_in_files)

CLEANFILES =								\
	$(desktop_in_files)						\
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
