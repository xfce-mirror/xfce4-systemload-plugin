/*
 * This file is part of Xfce (https://gitlab.xfce.org).
 *
 * Copyright (c) 2021-2022 Jan Ziak <0xe2.0x9a.0x9b@xfce.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "network.h"

#ifdef HAVE_LIBGTOP
/* Defined by obsoleted AC_HEADER_TIME macro, wanted by libgtop */
#define TIME_WITH_SYS_TIME 1
#include <glibtop/netlist.h>
#include <glibtop/netload.h>

static gint
read_netload_libgtop (gulong *bytes)
{
    glibtop_netlist netlist;
    char **interfaces = glibtop_get_netlist (&netlist);
    if (!interfaces)
        return -1;

    *bytes = 0;
    for (char **i = interfaces; *i != NULL; i++)
    {
        glibtop_netload netload;
        glibtop_get_netload (&netload, *i);
        *bytes += netload.bytes_total;
    }

    return 0;
}

#else

static gint
read_netload_libgtop (gulong *bytes)
{
    return -1;
}

#endif

static const char *const PROC_NET_DEV = "/proc/net/dev";
static const char *const REGEX_PATTERN = ".*:\\s*(\\d+)\\s*\\d+\\s*\\d+\\s*\\d+\\s*\\d+\\s*\\d+\\s*\\d+\\s*\\d+\\s*(\\d+)\\s*";

static gint
read_netload_proc (gulong *bytes)
{
    gchar *contents;
    GError *error;
    GRegex *regex;
    GMatchInfo *match_info;

    if (g_file_get_contents (PROC_NET_DEV, &contents, NULL, &error) == FALSE)
    {
        g_error ("Failed to read contents of %s: %s.", PROC_NET_DEV, error->message);
        g_error_free (error);
        return -1;
    }

    *bytes = 0;
    regex = g_regex_new (REGEX_PATTERN, (GRegexCompileFlags) 0, (GRegexMatchFlags) 0, NULL);
    g_regex_match (regex, contents, (GRegexMatchFlags) 0, &match_info);
    while (g_match_info_matches (match_info))
    {
        gchar *rx = g_match_info_fetch (match_info, 1);
        gchar *tx = g_match_info_fetch (match_info, 2);
        *bytes += g_ascii_strtoll (rx, NULL, 10) + g_ascii_strtoll (tx, NULL, 10);
        g_free (rx);
        g_free (tx);
        g_match_info_next (match_info, NULL);
    }

    g_match_info_free (match_info);
    g_regex_unref (regex);
    g_free (contents);

    return 0;
}

gint
read_netload (gulong *net, gulong *NTotal)
{
    static gulong bytes[2];
    static gint64 time[2];

    *net = 0;
    *NTotal = 0;

    time[1] = g_get_monotonic_time ();

    if (read_netload_proc (&bytes[1]) != 0)
        if (read_netload_libgtop (&bytes[1]) != 0)
            return -1;

    if (time[0] != 0 && G_LIKELY (time[1] > time[0]) && G_LIKELY (bytes[1] >= bytes[0]))
    {
        guint64 diff_bits = 8 * (bytes[1] - bytes[0]);
        gdouble diff_time = (time[1] - time[0]) / 1e6;
        *net = MIN (100 * diff_bits / diff_time / MAX_BANDWIDTH_BITS, 100);
        *NTotal = diff_bits / diff_time;
    }

    bytes[0] = bytes[1];
    time[0] = time[1];

    return 0;
}
