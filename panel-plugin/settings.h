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

/*
gboolean           pulseaudio_config_get_enable_keyboard_shortcuts  (SystemloadConfig     *config);
gboolean           pulseaudio_config_get_enable_multimedia_keys     (SystemloadConfig     *config);
gboolean           pulseaudio_config_get_show_notifications         (SystemloadConfig     *config);
guint              pulseaudio_config_get_volume_step                (SystemloadConfig     *config);
guint              pulseaudio_config_get_volume_max                 (SystemloadConfig     *config);
const gchar       *pulseaudio_config_get_mixer_command              (SystemloadConfig     *config);
gchar            **pulseaudio_config_get_mpris_players              (SystemloadConfig     *config);

gboolean           pulseaudio_config_get_enable_mpris               (SystemloadConfig     *config);
void               pulseaudio_config_set_mpris_players              (SystemloadConfig     *config,
                                                                     gchar               **players);
void               pulseaudio_config_add_mpris_player               (SystemloadConfig     *config,
                                                                     gchar                *player);

void               pulseaudio_config_player_blacklist_add           (SystemloadConfig     *config,
                                                                     const gchar          *player);
void               pulseaudio_config_player_blacklist_remove        (SystemloadConfig     *config,
                                                                     const gchar          *player);
gboolean           pulseaudio_config_player_blacklist_lookup        (SystemloadConfig     *config,
                                                                     gchar                *player);

void               pulseaudio_config_clear_known_players            (SystemloadConfig     *config);

void               pulseaudio_config_set_can_raise_wnck             (SystemloadConfig     *config,
                                                                     gboolean              can_raise);
gboolean           pulseaudio_config_get_can_raise_wnck             (SystemloadConfig     *config);
*/
G_END_DECLS

#endif /* !__SYSTEMLOAD_CONFIG_H__ */
