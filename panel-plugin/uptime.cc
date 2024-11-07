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

#include "uptime.h"

#if defined(__linux__) || defined(__FreeBSD_kernel__)

#include <fcntl.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROC_UPTIME "/proc/uptime"

gulong read_uptime()
{
    FILE *fd = fopen(PROC_UPTIME, "r");
    if (!fd) {
        g_warning("%s", _("File /proc/uptime not found!"));
        return 0;
    }

    gulong uptime;
    if (!fscanf(fd, "%lu", &uptime))
       uptime = 0;

    fclose(fd);
    return uptime;
}

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__APPLE__)

#if defined(__NetBSD__) || defined(__APPLE__)
/*
 * NetBSD and Darwin defines MAX and MIN in sys/param.h, so undef the glib macros first
 */
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif
#endif /* !__NetBSD__ && !__APPLE__ */

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/vmmeter.h>
#include <unistd.h>

#ifdef __FreeBSD__
#include <sys/time.h>
#endif /* !__FreeBSD__ */

gulong read_uptime()
{
   int mib[2] = {CTL_KERN, KERN_BOOTTIME};
   struct timeval boottime;
   size_t size = sizeof(boottime);
   gulong uptime;
 
   if((sysctl(mib, 2, &boottime, &size, NULL, 0) != -1) && (boottime.tv_sec != 0)) {
      time_t now;
      time(&now);
      uptime = now - boottime.tv_sec;
   }
   else
   {
       g_warning("Cannot get kern.boottime");
       uptime = 0;
   }

   return uptime;
}

#elif defined(__sun__)

#include <kstat.h>

gulong read_uptime()
{
   kstat_ctl_t *kc;
   kstat_t *ks;
   kstat_named_t *boottime;
   gulong uptime;

   if (((kc = kstat_open()) != 0) && ((ks = kstat_lookup(kc, "unix", 0, "system_misc")) != NULL) && (kstat_read(kc, ks, NULL) != -1) && ((boottime = kstat_data_lookup(ks, "boot_time")) != NULL)) {
      time_t now;
      time(&now);
      uptime = now - boottime->value.ul;
      kstat_close(kc);
   }
   else
   {
       g_warning("Cannot get boot_time");
       uptime = 0;
   }

   return uptime;
}

#else
#error "Your platform is not yet supported"
#endif
