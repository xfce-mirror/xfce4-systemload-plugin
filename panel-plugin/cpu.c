/*
 * Copyright (c) 2003 Riccardo Persichetti <riccardo.persichetti@tin.it>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <gtk/gtk.h>

#include <libxfce4util/i18n.h>
#include <libxfcegui4/dialogs.h>
#include <panel/plugins.h>
#include <panel/xfce.h>

#include "cpu.h" 

#if defined(__linux__)
#define PROC_STAT "/proc/stat"

/* user, nice, system, interrupt(BSD specific), idle */
struct cpu_load_struct {
    gulong load[5];
};

struct cpu_load_struct fresh = {{0, 0, 0, 0, 0}};
gulong cpu_used, oldtotal, oldused;

gulong read_cpuload()
{
    FILE *fd;
    gulong used, total;

    fd = fopen(PROC_STAT, "r");
    if (!fd) {
        g_warning(_("File /proc/stat not found!"));
        return 0;
    }
    fscanf(fd, "%*s %ld %ld %ld %ld", &fresh.load[0], &fresh.load[1],
           &fresh.load[2], &fresh.load[3]);
    fclose(fd);

    used = fresh.load[0] + fresh.load[1] + fresh.load[2];
    total = fresh.load[0] + fresh.load[1] + fresh.load[2] + fresh.load[3];
    if ((total - oldtotal) != 0)
    {
        cpu_used = (100 * (double)(used - oldused)) / (double)(total - oldtotal);
    }
    else
    {
        cpu_used = 0;
    }
    oldused = used;
    oldtotal = total;

    return cpu_used;
}

#elif defined(__FreeBSD__)

#include <osreldate.h>
#include <sys/types.h>
#if __FreeBSD_version < 500101
#include <sys/dkstat.h>
#else
#include <sys/resource.h>
#endif
#include <sys/sysctl.h>
#include <devstat.h>
#include <fcntl.h>
#include <nlist.h>

/* user, nice, system, interrupt(BSD specific), idle */
struct cpu_load_struct {
    gulong load[5];
};

struct cpu_load_struct fresh = {{0, 0, 0, 0, 0}};
gulong cpu_used, oldtotal, oldused;

gulong read_cpuload()
{
    gulong used, total;
    long cp_time[CPUSTATES];
    int len = sizeof(cp_time);

    if (sysctlbyname("kern.cp_time", &cp_time, &len, NULL, 0) < 0) {
        g_warning("Cannot get kern.cp_time");
        return 0;
    }

    fresh.load[0] = cp_time[CP_USER];
    fresh.load[1] = cp_time[CP_NICE];
    fresh.load[2] = cp_time[CP_SYS];
    fresh.load[3] = cp_time[CP_IDLE];
    fresh.load[4] = cp_time[CP_IDLE];

    used = fresh.load[0] + fresh.load[1] + fresh.load[2];
    total = fresh.load[0] + fresh.load[1] + fresh.load[2] + fresh.load[3];
    if ((total - oldtotal) != 0)
    {
        cpu_used = (100 * (double)(used - oldused)) / (double)(total - oldtotal);
    }
    else
    {
        cpu_used = 0;
    }
    oldused = used;
    oldtotal = total;

    return cpu_used;
}

#elif defined(__NetBSD__)
/*
 * NetBSD defines MAX and MIN in sys/param.h, so undef the glib macros first
 */
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif

#include <sys/param.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <nlist.h>

/* user, nice, system, interrupt(BSD specific), idle */
struct cpu_load_struct {
    gulong load[5];
};

struct cpu_load_struct fresh = {{0, 0, 0, 0, 0}};
gulong cpu_used, oldtotal, oldused;

gulong read_cpuload()
{
    gulong used, total;
    static int mib[] = { CTL_KERN, KERN_CP_TIME };
    u_int64_t cp_time[CPUSTATES];
    int len = sizeof(cp_time);

    if (sysctl(mib, 2, &cp_time, &len, NULL, 0) < 0) {
            g_warning("Cannot get kern.cp_time");
            return 0;
    }

    fresh.load[0] = cp_time[CP_USER];
    fresh.load[1] = cp_time[CP_NICE];
    fresh.load[2] = cp_time[CP_SYS];
    fresh.load[3] = cp_time[CP_IDLE];
    fresh.load[4] = cp_time[CP_IDLE];

    used = fresh.load[0] + fresh.load[1] + fresh.load[2];
    total = fresh.load[0] + fresh.load[1] + fresh.load[2] + fresh.load[3];
    if ((total - oldtotal) != 0)
    {
        cpu_used = (100 * (double)(used - oldused)) / (double)(total - oldtotal);
    }
    else
    {
        cpu_used = 0;
    }
    oldused = used;
    oldtotal = total;

    return cpu_used;
}

#elif defined(__OpenBSD__)
/*
 * NetBSD defines MAX and MIN in sys/param.h, so undef the glib macros first
 */
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif

#include <sys/param.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/dkstat.h>
#include <fcntl.h>
#include <nlist.h>

/* user, nice, system, interrupt(BSD specific), idle */
struct cpu_load_struct {
    gulong load[5];
};

struct cpu_load_struct fresh = {{0, 0, 0, 0, 0}};
gulong cpu_used, oldtotal, oldused;

gulong read_cpuload()
{
    gulong used, total;
    static int mib[] = { CTL_KERN, KERN_CPTIME };
    long cp_time[CPUSTATES];
    int len = sizeof(cp_time);

    if (sysctl(mib, 2, &cp_time, &len, NULL, 0) < 0) {
            g_warning("Cannot get kern.cp_time");
            return 0;
    }

    fresh.load[0] = cp_time[CP_USER];
    fresh.load[1] = cp_time[CP_NICE];
    fresh.load[2] = cp_time[CP_SYS];
    fresh.load[3] = cp_time[CP_IDLE];
    fresh.load[4] = cp_time[CP_IDLE];

    used = fresh.load[0] + fresh.load[1] + fresh.load[2];
    total = fresh.load[0] + fresh.load[1] + fresh.load[2] + fresh.load[3];
    if ((total - oldtotal) != 0)
    {
        cpu_used = (100 * (double)(used - oldused)) / (double)(total - oldtotal);
    }
    else
    {
        cpu_used = 0;
    }
    oldused = used;
    oldtotal = total;

    return cpu_used;
}

#else
#error "Your plattform is not yet supported"
#endif
