# This file is part of Xfce (https://gitlab.xfce.org).
#
# Copyright (c) 2021-2022 Jan Ziak <0xe2.0x9a.0x9b@xfce.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

AM_CPPFLAGS = -I${top_srcdir}

noinst_LTLIBRARIES = libxfce4util_pp.la

libxfce4util_pp_la_CXXFLAGS = \
	@GTK_CFLAGS@ \
	@LIBXFCE4PANEL_CFLAGS@ \
	@LIBXFCE4UTIL_CFLAGS@

libxfce4util_pp_la_LIBADD = \
	@GTK_LIBS@ \
	@LIBXFCE4PANEL_LIBS@ \
	@LIBXFCE4UTIL_LIBS@

libxfce4util_pp_la_SOURCES = \
	fixes.h
