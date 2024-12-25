dnl
dnl Copyright (c) 2003 - 2022 The Xfce development team
dnl
dnl 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
dnl 2010 Florian Rivoal <frivoal@xfce.org>
dnl 2012, 2014, 2016 Landry Breuil <landry@xfce.org>
dnl

dnl Version information
XDT_VERSION_INIT([1.3.2], [git])

dnl Initialize autoconf
AC_COPYRIGHT([Copyright (c) 2003-2024 The Xfce development team. All rights reserved.])
AC_INIT([xfce4-systemload-plugin], [xdt_version], [https://gitlab.xfce.org/panel-plugins/xfce4-systemload-plugin/], [xfce4-systemload-plugin])
AC_REVISION([xdt_version_build])
AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIRS([m4])

dnl Initialize automake
AM_INIT_AUTOMAKE([1.8 dist-bzip2 tar-ustar no-dist-gzip foreign])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()
AM_SILENT_RULES([yes])

dnl Check for basic programs
AC_PROG_CC()
AC_PROG_CXX()
LT_PATH_LD([])
AC_PROG_INSTALL
AX_CXX_COMPILE_STDCXX_11()

dnl Disable static libs
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl Check for standard headers
dnl libgtop wants HAVE_SYS_TIME_H defined
AC_CHECK_HEADERS([sys/time.h])

dnl Check for kvm, needed for BSD
AC_CHECK_LIB([kvm], [kvm_open])

dnl Check for i18n support
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl Check for required packages
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [2.50.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [3.22.0])
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-2.0], [4.16.0])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [4.16.0])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.17.2])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [4.16.0])
XDT_CHECK_OPTIONAL_PACKAGE([LIBGTOP], [libgtop-2.0], [2.32.0], [libgtop],
                           [libgtop for network utilization monitoring])
XDT_CHECK_OPTIONAL_PACKAGE([UPOWER_GLIB], [upower-glib], [0.99.0], [upower],
                           [upower for adapting update interval to power state])

dnl Check for debugging support
XDT_FEATURE_DEBUG([xdt_debug_default])

AC_CONFIG_FILES([
Makefile
icons/Makefile
icons/16x16/Makefile
icons/24x24/Makefile
icons/32x32/Makefile
icons/128x128/Makefile
icons/scalable/Makefile
panel-plugin/Makefile
po/Makefile.in
xfce4++/Makefile
xfce4++/util/Makefile
])
AC_OUTPUT
