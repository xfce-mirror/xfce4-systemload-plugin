/*
 * This file is part of Xfce (https://gitlab.xfce.org).
 *
 * Copyright (c) 2003 Riccardo Persichetti <riccardo.persichetti@tin.it>
 * Copyright (c) 2010 Florian Rivoal <frivoal@xfce.org>
 * Copyright (c) 2022 Jan Ziak <0xe2.0x9a.0x9b@xfce.org>
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

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

#if defined(__linux__) || defined(__FreeBSD_kernel__)

#include <glib/gi18n.h>
#include <stdint.h>

#define PROC_STAT "/proc/stat"

/* user, nice, system, interrupt(BSD specific), idle */
struct cpu_load_struct {
    gulong load[5];
};

static gulong oldtotal, oldused;

gulong read_cpuload(void)
{
    FILE *fd;
    unsigned long long int user, unice, usystem, idle, iowait, irq, softirq, guest;
    gulong cpu_used, used, total;
    int nb_read;

    fd = fopen(PROC_STAT, "r");
    if (!fd) {
        g_warning("%s", _("File /proc/stat not found!"));
        return 0;
    }

    /* Don't count steal time. It is neither busy nor free tiime. */
    nb_read = fscanf (fd, "%*s %llu %llu %llu %llu %llu %llu %llu %*u %llu",
                      &user, &unice, &usystem, &idle, &iowait, &irq, &softirq, &guest);
    fclose(fd);
    if (nb_read <= 4) iowait = 0;
    if (nb_read <= 5) irq = 0;
    if (nb_read <= 6) softirq = 0;
    if (nb_read <= 7) guest = 0;

    used = user + unice + usystem + irq + softirq + guest;
    total = used + idle + iowait;

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

#elif defined(__FreeBSD__) || defined(__DragonFly__)

#include <osreldate.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <devstat.h>
#include <fcntl.h>
#include <nlist.h>

/* user, nice, system, interrupt(BSD specific), idle */
struct cpu_load_struct {
    gulong load[5];
};

static gulong oldtotal, oldused;

gulong read_cpuload(void)
{
    gulong cpu_used, used, total;
    long cp_time[CPUSTATES];
    size_t len = sizeof(cp_time);

    if (sysctlbyname("kern.cp_time", &cp_time, &len, NULL, 0) < 0) {
        g_warning("Cannot get kern.cp_time");
        return 0;
    }

    used = cp_time[CP_USER] + cp_time[CP_NICE] + cp_time[CP_SYS] + cp_time[CP_INTR];
    total = used + cp_time[CP_IDLE];

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

static gulong oldtotal, oldused;

gulong read_cpuload(void)
{
    gulong cpu_used, used, total;
    static int mib[] = { CTL_KERN, KERN_CP_TIME };
    u_int64_t cp_time[CPUSTATES];
    size_t len = sizeof(cp_time);

    if (sysctl(mib, 2, &cp_time, &len, NULL, 0) < 0) {
            g_warning("Cannot get kern.cp_time");
            return 0;
    }

    used = cp_time[CP_USER] + cp_time[CP_NICE] + cp_time[CP_SYS] + cp_time[CP_INTR];
    total = used + cp_time[CP_IDLE];

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
#include <fcntl.h>
#include <nlist.h>

/* user, nice, system, interrupt(BSD specific), idle */
struct cpu_load_struct {
    gulong load[5];
};

static gulong oldtotal, oldused;

gulong read_cpuload(void)
{
    gulong cpu_used, used, total;
    static int mib[] = { CTL_KERN, KERN_CPTIME };
    long cp_time[CPUSTATES];
    size_t len = sizeof(cp_time);

    if (sysctl(mib, 2, &cp_time, &len, NULL, 0) < 0) {
            g_warning("Cannot get kern.cp_time");
            return 0;
    }

    used = cp_time[CP_USER] + cp_time[CP_NICE] + cp_time[CP_SYS] + cp_time[CP_INTR];
    total = used + cp_time[CP_IDLE];

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
#elif defined(__sun__)

#include <kstat.h>

static kstat_ctl_t *kc;
static gulong oldtotal, oldused;

void init_stats(void)
{
    kc = kstat_open();
}

gulong read_cpuload(void)
{
    gulong cpu_used, used, total;
    kstat_t *ksp;
    kstat_named_t *knp;

    if (!kc)
    {
       init_stats();
    }
    kstat_chain_update(kc);
    used = 0;
    total = 0;
    for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next)
    {
        if (!strcmp(ksp->ks_module, "cpu") && !strcmp(ksp->ks_name, "sys"))
       {
           kstat_read(kc, ksp, NULL);
           knp = kstat_data_lookup(ksp, "cpu_ticks_user");
           used += knp->value.ui64;
           total += knp->value.ui64;
           knp = kstat_data_lookup(ksp, "cpu_ticks_kernel");
           used += knp->value.ui64;
           total += knp->value.ui64;
           knp = kstat_data_lookup(ksp, "cpu_ticks_idle");
           total += knp->value.ui64;
       }
    }

    printf("CPU: %lu %lu %lu %lu\n", used, oldused, total, oldtotal);

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
#error "Your platform is not yet supported"
#endif
