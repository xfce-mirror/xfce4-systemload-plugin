dnl configure.ac
dnl
dnl xfce4-systemload-plugin - Systemload monitor plugin for xfce4-panel
dnl
dnl 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
dnl

AC_INIT([panel-plugin/systemload.c])

AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([xfce4-systemload-plugin], [0.3.3])

AM_MAINTAINER_MODE

dnl Check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl Check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl Check for kvm, needed by bsd
AC_CHECK_LIB([kvm], [kvm_open])

dnl Check for standard header files
AC_HEADER_STDC

dnl Check for i18n support 
BM_I18N([xfce4-systemload], [de ca hu lt uk])

dnl configure the panel plugin
XFCE_PANEL_PLUGIN([XFCE4_PANEL], [3.99.2])

dnl Check for debugging support
BM_DEBUG_SUPPORT

AC_OUTPUT([
Makefile
panel-plugin/Makefile
po/Makefile.in
])
