/*
 *  This file is part of Xfce (https://gitlab.xfce.org).
 *
 *  Copyright (C) 2021 Simon Steinbeiß <simon@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _XFCE_SYSTEMLOAD_SETTINGS_H_
#define _XFCE_SYSTEMLOAD_SETTINGS_H_

#include <glib.h>

#define MIN_TIMEOUT 500
#define MAX_TIMEOUT 10000

#define MIN_BAR_WIDTH 1
#define MAX_BAR_WIDTH 128

enum SystemloadMonitor {
    CPU_MONITOR,
    MEM_MONITOR,
    NET_MONITOR,
    SWAP_MONITOR,
};

typedef struct _SystemloadConfigClass SystemloadConfigClass;
typedef struct _SystemloadConfig      SystemloadConfig;

#define TYPE_SYSTEMLOAD_CONFIG             (systemload_config_get_type ())
#define SYSTEMLOAD_CONFIG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SYSTEMLOAD_CONFIG, SystemloadConfig))
#define SYSTEMLOAD_CONFIG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_SYSTEMLOAD_CONFIG, SystemloadConfigClass))
#define IS_SYSTEMLOAD_CONFIG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SYSTEMLOAD_CONFIG))
#define IS_SYSTEMLOAD_CONFIG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_SYSTEMLOAD_CONFIG))
#define SYSTEMLOAD_CONFIG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_SYSTEMLOAD_CONFIG, SystemloadConfigClass))

GType              systemload_config_get_type                       (void) G_GNUC_CONST;

SystemloadConfig  *systemload_config_new                            (const gchar          *property_base);

void               systemload_config_on_change                      (SystemloadConfig     *config,
                                                                     gboolean             (*callback)(gpointer user_data),
                                                                     gpointer             user_data);

guint              systemload_config_get_timeout                    (const SystemloadConfig *config);
guint              systemload_config_get_timeout_seconds            (const SystemloadConfig *config);
const gchar       *systemload_config_get_system_monitor_command     (const SystemloadConfig *config);
guint              systemload_config_get_bar_width                  (const SystemloadConfig *config);
bool               systemload_config_get_uptime_enabled             (const SystemloadConfig *config);

bool               systemload_config_get_enabled   (const SystemloadConfig *config, SystemloadMonitor monitor);
bool               systemload_config_get_use_label (const SystemloadConfig *config, SystemloadMonitor monitor);
const gchar       *systemload_config_get_label     (const SystemloadConfig *config, SystemloadMonitor monitor);
const GdkRGBA     *systemload_config_get_color     (const SystemloadConfig *config, SystemloadMonitor monitor);

#endif /* _XFCE_SYSTEMLOAD_SETTINGS_H_ */
