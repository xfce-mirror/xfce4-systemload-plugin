/*
 * Copyright (c) 2003 Riccardo Persichetti <ricpersi@libero.it>
 * Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 *
 * Some code taken from ascpu (credits to Albert Dorofeev <Albert@tigr.net>)
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

#ifdef __hpux__
#include <sys/pstat.h>
#include <sys/dk.h>
#endif

#ifdef _AIX32  /* AIX > 3.1 */
#include <nlist.h>
#include <sys/param.h>
#include <sys/sysinfo.h>
#endif /* _AIX32 */

/* file to read for stat info */
#define PROC_STAT "/proc/stat"

/* The maximum number of CPUs in the SMP system */
#ifdef __hpux__
#include <sys/pstat.h>
#define MAX_CPU PST_MAX_PROCS
#else
#define MAX_CPU 16
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#endif

#if defined(__FreeBSD__)
#include <osreldate.h>
#include <sys/types.h>
#if __FreeBSD_version < 500101
#include <sys/dkstat.h>
#else
#include <sys/resource.h>
#endif
#include <sys/sysctl.h>
#include <devstat.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <fcntl.h>
#include <nlist.h>
#endif

#ifdef _AIX32   /* AIX > 3.1 */
/* Kernel memory file */
#define KMEM_FILE "/dev/kmem"
/* Descriptor of kernel memory file */
static int  kmem;
/* Offset of sysinfo structure in kernel memory file */
static long sysinfo_offset;
/* Structure to access kernel symbol table */
static struct nlist namelist[] = {
  { {"sysinfo"}, 0, 0, {0}, 0, 0 },
  { {0},         0, 0, {0}, 0, 0 }
};
#endif /* _AIX32 */

/*
 * The information over the CPU load is always kept in 4 variables
 * The order is:
 *         user
 *         nice
 *         system
 *        interrupt(BSD specific)
 *         idle
 */
struct cpu_load_struct {
    gulong load[5];
};

struct cpu_load_struct fresh = {{0, 0, 0, 0, 0}};

gulong cpu_used, oldtotal, oldused;

gulong read_cpuload()
{
    FILE *fd;
    gulong used, total;

#ifdef __hpux__
    struct pst_dynamic store_pst_dynamic;
    long int *p;
    int i;
#endif

#ifdef _AIX32   /* AIX > 3.1 */
    time_t *p;
    struct sysinfo sysinfo;
#endif          

#if defined(__NetBSD__) || defined(__OpenBSD__)
    static int mib[] = { CTL_KERN, KERN_CP_TIME };
    u_int64_t cp_time[CPUSTATES];
    int len = sizeof(cp_time);

    if (sysctl(mib, 2, &cp_time, &len, NULL, 0) < 0) {
            g_warning("Cannot get kern.cp_time");
            return 0;
    }

    /* compatible with Linux(overwrite 'interrupt' with 'idle' field) */
    fresh.load[0] = cp_time[CP_USER];
    fresh.load[1] = cp_time[CP_NICE];
    fresh.load[2] = cp_time[CP_SYS];
    fresh.load[3] = cp_time[CP_IDLE];
    fresh.load[4] = cp_time[CP_IDLE];
#endif

#if defined(__FreeBSD__)
    long cp_time[CPUSTATES];
    int len = sizeof(cp_time);

    if (sysctlbyname("kern.cp_time", &cp_time, &len, NULL, 0) < 0) {
        g_warning("Cannot get kern.cp_time");
        return 0;
    }

    /* compatible with Linux(overwrite 'interrupt' with 'idle' field) */
    fresh.load[0] = cp_time[CP_USER];
    fresh.load[1] = cp_time[CP_NICE];
    fresh.load[2] = cp_time[CP_SYS];
    fresh.load[3] = cp_time[CP_IDLE];
    fresh.load[4] = cp_time[CP_IDLE];
#endif

#ifdef __hpux__
    /*
    params: structure buf pointer, structure size, num of structs
    some index. HP say always 1, 0 for the last two. the order in
    fresh[] matches HP's psd_cpu_time[] and psd_mp_cpu_time[N][],
    so I can use the indices in dk.h for everything
    */

    if (pstat_getdynamic(&store_pst_dynamic, sizeof store_pst_dynamic, 1, 0 )) {
        /* give the average over all processors */
        p = store_pst_dynamic.psd_cpu_time;

        fresh.load[CP_USER] = p[CP_USER];
        fresh.load[CP_NICE] = p[CP_NICE];
        fresh.load[CP_SYS]  = p[CP_SYS] + p[CP_BLOCK] +
                              p[CP_INTR] + p[CP_SSYS] + p[CP_SWAIT];
        fresh.load[CP_IDLE] = p[CP_IDLE] + p[CP_WAIT];
    }
    else {
        g_warning(_("Error while calling pstat_getdynamic(2)"));
    }

#endif

#ifdef _AIX32   /* AIX > 3.1 */
    nlist ("/unix", namelist);
    if (namelist[0].n_value == 0) {
        g_warning(_("Cannot get nlist."));
        return 0;
    }
    sysinfo_offset = namelist[0].n_value;
    kmem = open(KMEM_FILE, O_RDONLY);
    if (kmem < 0) {
        g_warning(_("Problem with kmem."));
        return 0;
    }
    if (lseek (kmem, sysinfo_offset, SEEK_SET) != sysinfo_offset) {
        g_warning(_(Problem with kmem.));
    }
    if (read(kmem, (char *)&sysinfo, sizeof(struct sysinfo)) != 
        sizeof(struct sysinfo))
    {
        g_warning(_("Problem with kmem."));
    }
    p = sysinfo.cpu;
    fresh.load[0] = p[CPU_USER];
    fresh.load[1] = p[CPU_WAIT];
    fresh.load[2] = p[CPU_KERNEL];
    fresh.load[3] = fresh.load[4] = p[CPU_IDLE];
#endif /* _AIX32 */

#ifdef __linux__
    fd = fopen(PROC_STAT, "r");
    if (!fd) {
        g_warning(_("File /proc/stat not found!"));
        return 0;
    }
    fscanf(fd, "%*s %ld %ld %ld %ld", &fresh.load[0], &fresh.load[1],
                                      &fresh.load[2], &fresh.load[3]);
    fclose(fd);
#endif

    used = fresh.load[0] + fresh.load[1] + fresh.load[2];
    total = fresh.load[0] + fresh.load[1] + fresh.load[2] + fresh.load[3];
    if ((total - oldtotal) != 0)
        cpu_used = (100 * (double)(used - oldused)) / (double)(total -oldtotal);
    else
        cpu_used = 0;
    oldused = used;
    oldtotal = total;

    return cpu_used;
}
