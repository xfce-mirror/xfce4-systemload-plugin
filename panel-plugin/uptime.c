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

#include <gtk/gtk.h>

#include <libxfce4util/i18n.h>
#include <libxfcegui4/dialogs.h>
#include <panel/plugins.h>
#include <panel/xfce.h>

#include "uptime.h"

#if defined(__linux__)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define PROC_UPTIME "/proc/uptime"

gulong read_uptime()
{
    FILE *fd;
    gulong uptime;

    fd = fopen(PROC_UPTIME, "r");
    if (!fd) {
        g_warning(_("File /proc/uptime not found!"));
        return 0;
    }
    fscanf(fd, "%lu", &uptime);
    fclose(fd);

    return uptime;
}

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)

#ifdef __NetBSD__
/*
 * NetBSD defines MAX and MIN in sys/param.h, so undef the glib macros first
 */
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif
#endif /* !__NetBSD__ */

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

gulong read_uptime()
{
   int mib[2] = {CTL_KERN, KERN_BOOTTIME};
   struct timeval boottime;
   time_t now;
   int size = sizeof(boottime);
   gulong uptime;
 
   if((sysctl(mib, 2, &boottime, &size, NULL, 0) != -1)
	   && (boottime.tv_sec != 0)) {
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

#endif
