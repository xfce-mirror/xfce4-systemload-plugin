/*
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

#ifndef __SYSTEMLOAD_CONFIG_H__
#define __SYSTEMLOAD_CONFIG_H__

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

enum { CPU_MONITOR, MEM_MONITOR, SWAP_MONITOR };

typedef struct _SystemloadConfigClass SystemloadConfigClass;
typedef struct _SystemloadConfig      SystemloadConfig;

#define TYPE_SYSTEMLOAD_CONFIG             (systemload_config_get_type ())
#define SYSTEMLOAD_CONFIG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SYSTEMLOAD_CONFIG, SystemloadConfig))
#define SYSTEMLOAD_CONFIG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_SYSTEMLOAD_CONFIG, SystemloadConfigClass))
#define IS_SYSTEMLOAD_CONFIG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SYSTEMLOAD_CONFIG))
#define IS_SYSTEMLOAD_CONFIG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_SYSTEMLOAD_CONFIG))
#define SYSTEMLOAD_CONFIG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_SYSTEMLOAD_CONFIG, SystemloadConfigClass))

GType              systemload_config_get_type                       (void)                                       G_GNUC_CONST;

SystemloadConfig  *systemload_config_new                            (const gchar          *property_base);

guint              systemload_config_get_timeout                    (SystemloadConfig     *config);
guint              systemload_config_get_timeout_seconds            (SystemloadConfig     *config);
const gchar       *systemload_config_get_system_monitor_command     (SystemloadConfig     *config);
gboolean           systemload_config_get_uptime_enabled             (SystemloadConfig     *config);

gboolean           systemload_config_get_cpu_enabled                (SystemloadConfig     *config);
gboolean           systemload_config_get_cpu_use_label              (SystemloadConfig     *config);
const gchar       *systemload_config_get_cpu_label                  (SystemloadConfig     *config);
GdkRGBA           *systemload_config_get_cpu_color                  (SystemloadConfig     *config);

gboolean           systemload_config_get_memory_enabled             (SystemloadConfig     *config);
gboolean           systemload_config_get_memory_use_label           (SystemloadConfig     *config);
const gchar       *systemload_config_get_memory_label               (SystemloadConfig     *config);
GdkRGBA           *systemload_config_get_memory_color               (SystemloadConfig     *config);

gboolean           systemload_config_get_swap_enabled               (SystemloadConfig     *config);
gboolean           systemload_config_get_swap_use_label             (SystemloadConfig     *config);
const gchar       *systemload_config_get_swap_label                 (SystemloadConfig     *config);
GdkRGBA           *systemload_config_get_swap_color                 (SystemloadConfig     *config);



G_END_DECLS

#endif /* !__SYSTEMLOAD_CONFIG_H__ */
