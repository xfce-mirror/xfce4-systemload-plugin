/*
 *  This file is part of Xfce (https://gitlab.xfce.org).
 *
 *  Copyright (c) 2021-2022 Jan Ziak <0xe2.0x9a.0x9b@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _XFCE4PP_UTIL_FIXES_H_
#define _XFCE4PP_UTIL_FIXES_H_

#if defined G_BEGIN_DECLS || defined LIBXFCE4UTIL_MAJOR_VERSION
#error "Please include xfce4++/util/fixes.h before any other include directives"
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Before extern "C" */
#include <glib.h>

/*
 * Adjust for missing G_BEGIN_DECLS/G_END_DECLS directives in C files
 */
#define LIBXFCE4UTIL_INSIDE_LIBXFCE4UTIL_H
#define _LIBXFCE4UTIL_INSIDE_LIBXFCE4UTIL_H
extern "C" {
    #include <libxfce4util/libxfce4util-config.h>
    #if LIBXFCE4UTIL_CHECK_VERSION(4, 17, 0)
        #include <libxfce4util/xfce-gio-extensions.h>
    #endif

    /*
     * Define the macro GETTEXT_PACKAGE in order to avoid getting
     * an invalid ngettext() definition from <libxfce4util/xfce-i18n.h>.
     *
     * See also: https://gitlab.xfce.org/xfce/libxfce4util/-/issues/7
     */
    #ifdef GETTEXT_PACKAGE
        #include <libxfce4util/xfce-i18n.h>
    #else
        /* Note: The symbol __UNDEFINED__GETTEXT_PACKAGE__... is meant not to be defined anywhere.
         *       The numeric suffix is a random 64-bit number. The random number makes it improbable
         *       for any 3rd-party source code to define such a symbol. */
        #define GETTEXT_PACKAGE __UNDEFINED__GETTEXT_PACKAGE__RND_11148334482592236430__
        #include <libxfce4util/xfce-i18n.h>
        #undef GETTEXT_PACKAGE
    #endif
}
#undef LIBXFCE4UTIL_INSIDE_LIBXFCE4UTIL_H
#undef _LIBXFCE4UTIL_INSIDE_LIBXFCE4UTIL_H

#endif /* _XFCE4PP_UTIL_FIXES_H_ */
