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



/*
 *  This file implements a configuration store. The class extends GObject.
 *
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#include "settings.h"




#define DEFAULT_TIMEOUT 500
#define DEFAULT_TIMEOUT_SECONDS 1
#define DEFAULT_SYSTEM_MONITOR_COMMAND "xfce4-taskmanager"
#define DEFAULT_CPU_LABEL "cpu"
#define DEFAULT_MEMORY_LABEL "mem"
#define DEFAULT_SWAP_LABEL "swap"



static void                 systemload_config_finalize       (GObject          *object);
static void                 systemload_config_get_property   (GObject          *object,
                                                              guint             prop_id,
                                                              GValue           *value,
                                                              GParamSpec       *pspec);
static void                 systemload_config_set_property   (GObject          *object,
                                                              guint             prop_id,
                                                              const GValue     *value,
                                                              GParamSpec       *pspec);



struct _SystemloadConfigClass
{
  GObjectClass     __parent__;
};

struct _SystemloadConfig
{
  GObject          __parent__;

  guint            timeout;
  guint            timeout_seconds;
  gchar           *system_monitor_command;
  gboolean         uptime;

  gboolean         cpu_enabled;
  gboolean         cpu_use_label;
  gchar           *cpu_label;
  GdkRGBA         *cpu_color;

  gboolean         memory_enabled;
  gboolean         memory_use_label;
  gchar           *memory_label;
  GdkRGBA         *memory_color;

  gboolean         swap_enabled;
  gboolean         swap_use_label;
  gchar           *swap_label;
  GdkRGBA         *swap_color;

};

enum
  {
    PROP_0,
    PROP_TIMEOUT,
    PROP_TIMEOUT_SECONDS,
    PROP_SYSTEM_MONITOR_COMMAND,
    PROP_UPTIME,
    PROP_CPU_ENABLED,
    PROP_CPU_USE_LABEL,
    PROP_CPU_LABEL,
    PROP_CPU_COLOR,
    PROP_MEMORY_ENABLED,
    PROP_MEMORY_USE_LABEL,
    PROP_MEMORY_LABEL,
    PROP_MEMORY_COLOR,
    PROP_SWAP_ENABLED,
    PROP_SWAP_USE_LABEL,
    PROP_SWAP_LABEL,
    PROP_SWAP_COLOR,
    N_PROPERTIES,
  };

enum
  {
    CONFIGURATION_CHANGED,
    LAST_SIGNAL
  };

static guint systemload_config_signals [LAST_SIGNAL] = { 0, };



G_DEFINE_TYPE (SystemloadConfig, systemload_config, G_TYPE_OBJECT)



static void
systemload_config_class_init (SystemloadConfigClass *klass)
{
  GObjectClass                 *gobject_class;

  gobject_class               = G_OBJECT_CLASS (klass);
  gobject_class->finalize     = systemload_config_finalize;
  gobject_class->get_property = systemload_config_get_property;
  gobject_class->set_property = systemload_config_set_property;


  g_object_class_install_property (gobject_class,
                                   PROP_TIMEOUT,
                                   g_param_spec_uint ("timeout", NULL, NULL,
                                                      500, 10000, DEFAULT_TIMEOUT,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_TIMEOUT_SECONDS,
                                   g_param_spec_uint ("timeout-seconds", NULL, NULL,
                                                      1, 10000, DEFAULT_TIMEOUT_SECONDS,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SYSTEM_MONITOR_COMMAND,
                                   g_param_spec_string ("system-monitor-command", NULL, NULL,
                                                        DEFAULT_SYSTEM_MONITOR_COMMAND,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_UPTIME,
                                   g_param_spec_boolean ("uptime-enabled", NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_CPU_ENABLED,
                                   g_param_spec_boolean ("cpu-enabled", NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_CPU_USE_LABEL,
                                   g_param_spec_boolean ("cpu-use-label", NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_CPU_LABEL,
                                   g_param_spec_string ("cpu-label", NULL, NULL,
                                                        DEFAULT_CPU_LABEL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_CPU_COLOR,
                                   g_param_spec_boxed ("cpu-color",
                                                       NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_MEMORY_ENABLED,
                                   g_param_spec_boolean ("memory-enabled", NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_MEMORY_USE_LABEL,
                                   g_param_spec_boolean ("memory-use-label", NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_MEMORY_LABEL,
                                   g_param_spec_string ("memory-label", NULL, NULL,
                                                        DEFAULT_MEMORY_LABEL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_MEMORY_COLOR,
                                   g_param_spec_boxed ("memory-color",
                                                       NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SWAP_ENABLED,
                                   g_param_spec_boolean ("swap-enabled", NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SWAP_USE_LABEL,
                                   g_param_spec_boolean ("swap-use-label", NULL, NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SWAP_LABEL,
                                   g_param_spec_string ("swap-label", NULL, NULL,
                                                        DEFAULT_SWAP_LABEL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SWAP_COLOR,
                                   g_param_spec_boxed ("swap-color",
                                                       NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  systemload_config_signals[CONFIGURATION_CHANGED] =
    g_signal_new (g_intern_static_string ("configuration-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
systemload_config_init (SystemloadConfig *config)
{
  config->timeout = DEFAULT_TIMEOUT;
  config->timeout_seconds = DEFAULT_TIMEOUT_SECONDS;
  config->system_monitor_command = DEFAULT_SYSTEM_MONITOR_COMMAND;
  config->uptime = TRUE;
  config->cpu_enabled = TRUE;
  config->cpu_use_label = TRUE;
  config->cpu_label = g_strdup (DEFAULT_CPU_LABEL);
  g_warning ("cpu color: %s", DEFAULT_COLOR[CPU_MONITOR]);
  if (gdk_rgba_parse (config->cpu_color,
                  DEFAULT_COLOR[0]) == FALSE)
    g_warning ("cpu color value not set");
  config->memory_enabled = TRUE;
  config->memory_use_label = TRUE;
  config->memory_label = g_strdup (DEFAULT_MEMORY_LABEL);
  g_warning ("mem color: %s", DEFAULT_COLOR[MEM_MONITOR]);
  gdk_rgba_parse (config->memory_color,
                  DEFAULT_COLOR[1]);
  config->swap_enabled = TRUE;
  config->swap_use_label = TRUE;
  config->swap_label = g_strdup (DEFAULT_SWAP_LABEL);
  gdk_rgba_parse (config->swap_color,
                  DEFAULT_COLOR[2]);
}



static void
systemload_config_finalize (GObject *object)
{
  SystemloadConfig *config = SYSTEMLOAD_CONFIG (object);

  xfconf_shutdown();
  g_free (config->system_monitor_command);
  g_free (config->cpu_label);
  gdk_rgba_free (config->cpu_color);
  g_free (config->memory_label);
  gdk_rgba_free (config->memory_color);
  g_free (config->swap_label);
  gdk_rgba_free (config->swap_color);

  G_OBJECT_CLASS (systemload_config_parent_class)->finalize (object);
}



static void
systemload_config_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  SystemloadConfig     *config = SYSTEMLOAD_CONFIG (object);

  switch (prop_id)
    {
    case PROP_TIMEOUT:
      g_value_set_uint (value, config->timeout);
      break;

    case PROP_TIMEOUT_SECONDS:
      g_value_set_uint (value, config->timeout_seconds);
      break;

    case PROP_SYSTEM_MONITOR_COMMAND:
      g_value_set_string (value, config->system_monitor_command);
      break;

    case PROP_UPTIME:
      g_value_set_boolean (value, config->uptime);
      break;

    case PROP_CPU_ENABLED:
      g_value_set_boolean (value, config->cpu_enabled);
      break;

    case PROP_CPU_USE_LABEL:
      g_value_set_boolean (value, config->cpu_use_label);
      break;

    case PROP_CPU_LABEL:
      g_value_set_string (value, config->cpu_label);
      break;

    case PROP_CPU_COLOR:
      g_warning ("get cpu color property");
      g_value_set_boxed (value, config->cpu_color);
      break;

    case PROP_MEMORY_ENABLED:
      g_value_set_boolean (value, config->memory_enabled);
      break;

    case PROP_MEMORY_USE_LABEL:
      g_value_set_boolean (value, config->memory_use_label);
      break;

    case PROP_MEMORY_LABEL:
      g_value_set_string (value, config->memory_label);
      break;

    case PROP_MEMORY_COLOR:
      g_value_set_boxed (value, config->memory_color);
      break;

    case PROP_SWAP_ENABLED:
      g_value_set_boolean (value, config->swap_enabled);
      break;

    case PROP_SWAP_USE_LABEL:
      g_value_set_boolean (value, config->swap_use_label);
      break;

    case PROP_SWAP_LABEL:
      g_value_set_string (value, config->swap_label);
      break;

    case PROP_SWAP_COLOR:
      g_value_set_boxed (value, config->swap_color);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
systemload_config_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  SystemloadConfig     *config = SYSTEMLOAD_CONFIG (object);
  guint                 val_uint;
  gboolean              val_bool;

  switch (prop_id)
    {
    case PROP_TIMEOUT:
      val_uint = g_value_get_uint (value);
      if (config->timeout != val_uint)
        {
          config->timeout = val_uint;
          g_object_notify (G_OBJECT (config), "timeout");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_TIMEOUT_SECONDS:
      val_uint = g_value_get_uint (value);
      if (config->timeout_seconds != val_uint)
        {
          config->timeout_seconds = val_uint;
          g_object_notify (G_OBJECT (config), "timeout-seconds");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_SYSTEM_MONITOR_COMMAND:
      g_free (config->system_monitor_command);
      config->system_monitor_command = g_value_dup_string (value);
      break;

    case PROP_UPTIME:
      val_bool = g_value_get_boolean (value);
      if (config->uptime != val_bool)
        {
          config->uptime = val_bool;
          g_object_notify (G_OBJECT (config), "uptime-enabled");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_CPU_ENABLED:
      val_bool = g_value_get_boolean (value);
      if (config->cpu_enabled != val_bool)
        {
          config->cpu_enabled = val_bool;
          g_object_notify (G_OBJECT (config), "cpu-enabled");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_CPU_USE_LABEL:
      val_bool = g_value_get_boolean (value);
      if (config->cpu_use_label != val_bool)
        {
          config->cpu_use_label = val_bool;
          g_object_notify (G_OBJECT (config), "cpu-use-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_CPU_LABEL:
      g_free (config->cpu_label);
      config->cpu_label = g_value_dup_string (value);
      break;

    case PROP_CPU_COLOR:
      g_warning ("set cpu color property");
      if (config->cpu_color != NULL)
        gdk_rgba_free (config->cpu_color);
      config->cpu_color = g_value_dup_boxed (value);
      g_object_notify (G_OBJECT (config), "cpu-color");
      g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
      break;

    case PROP_MEMORY_ENABLED:
      val_bool = g_value_get_boolean (value);
      if (config->memory_enabled != val_bool)
        {
          config->memory_enabled = val_bool;
          g_object_notify (G_OBJECT (config), "memory-enabled");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_MEMORY_USE_LABEL:
      val_bool = g_value_get_boolean (value);
      if (config->memory_use_label != val_bool)
        {
          config->memory_use_label = val_bool;
          g_object_notify (G_OBJECT (config), "memory-use-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_MEMORY_LABEL:
      g_free (config->memory_label);
      config->memory_label = g_value_dup_string (value);
      break;

    case PROP_MEMORY_COLOR:
      if (config->memory_color != NULL)
        gdk_rgba_free (config->memory_color);
      config->memory_color = g_value_dup_boxed (value);
      g_object_notify (G_OBJECT (config), "memory-color");
      g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
      break;

    case PROP_SWAP_ENABLED:
      val_bool = g_value_get_boolean (value);
      if (config->swap_enabled != val_bool)
        {
          config->swap_enabled = val_bool;
          g_object_notify (G_OBJECT (config), "swap-enabled");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_SWAP_USE_LABEL:
      val_bool = g_value_get_boolean (value);
      if (config->swap_use_label != val_bool)
        {
          config->swap_use_label = val_bool;
          g_object_notify (G_OBJECT (config), "swap-use-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_SWAP_LABEL:
      g_free (config->swap_label);
      config->swap_label = g_value_dup_string (value);
      break;

    case PROP_SWAP_COLOR:
      if (config->swap_color != NULL)
        gdk_rgba_free (config->swap_color);
      config->swap_color = g_value_dup_boxed (value);
      g_object_notify (G_OBJECT (config), "swap-color");
      g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



guint
systemload_config_get_timeout (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), DEFAULT_TIMEOUT);

  return config->timeout;
}

guint
systemload_config_get_timeout_seconds (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), DEFAULT_TIMEOUT_SECONDS);

  return config->timeout_seconds;
}

const gchar *
systemload_config_get_system_monitor_command (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), DEFAULT_SYSTEM_MONITOR_COMMAND);

  return config->system_monitor_command;
}

gboolean
systemload_config_get_uptime_enabled (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), TRUE);

  return config->uptime;
}

gboolean
systemload_config_get_cpu_enabled (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), TRUE);

  return config->cpu_enabled;
}

gboolean
systemload_config_get_cpu_use_label (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), TRUE);

  return config->cpu_use_label;
}

const gchar *
systemload_config_get_cpu_label (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), DEFAULT_CPU_LABEL);

  return config->cpu_label;
}

const GdkRGBA *
systemload_config_get_cpu_color (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), NULL);

  if (config->cpu_color == NULL)
    g_warning ("somehow no color ?");
  return config->cpu_color;
}

gboolean
systemload_config_get_memory_enabled (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), TRUE);

  return config->memory_enabled;
}

gboolean
systemload_config_get_memory_use_label (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), TRUE);

  return config->memory_use_label;
}

const gchar *
systemload_config_get_memory_label (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), DEFAULT_MEMORY_LABEL);

  return config->memory_label;
}

const GdkRGBA *
systemload_config_get_memory_color (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), NULL);

  return config->memory_color;
}

gboolean
systemload_config_get_swap_enabled (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), TRUE);

  return config->swap_enabled;
}

gboolean
systemload_config_get_swap_use_label (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), TRUE);

  return config->swap_use_label;
}

const gchar *
systemload_config_get_swap_label (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), DEFAULT_SWAP_LABEL);

  return config->swap_label;
}

const GdkRGBA *
systemload_config_get_swap_color (SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), NULL);

  return config->swap_color;
}



SystemloadConfig *
systemload_config_new (const gchar     *property_base)
{
  SystemloadConfig    *config;
  XfconfChannel       *channel;
  gchar               *property;

  config = g_object_new (TYPE_SYSTEMLOAD_CONFIG, NULL);

  if (xfconf_init (NULL))
    {
      channel = xfconf_channel_get ("xfce4-panel");

      property = g_strconcat (property_base, "/timeout", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_UINT, config, "timeout");
      g_free (property);

      property = g_strconcat (property_base, "/timeout-seconds", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_UINT, config, "timeout-seconds");
      g_free (property);

      property = g_strconcat (property_base, "/system-monitor-command", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_STRING, config, "system-monitor-command");
      g_free (property);

      property = g_strconcat (property_base, "/uptime/enabled", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, "uptime-enabled");
      g_free (property);

      property = g_strconcat (property_base, "/cpu/enabled", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, "cpu-enabled");
      g_free (property);

      property = g_strconcat (property_base, "/cpu/use-label", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, "cpu-use-label");
      g_free (property);

      property = g_strconcat (property_base, "/cpu/label", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_STRING, config, "cpu-label");
      g_free (property);

      property = g_strconcat (property_base, "/cpu/color", NULL);
      xfconf_g_property_bind_gdkrgba (channel, property, config, "cpu-color");
      g_free (property);

      property = g_strconcat (property_base, "/memory/enabled", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, "memory-enabled");
      g_free (property);

      property = g_strconcat (property_base, "/memory/use-label", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, "memory-use-label");
      g_free (property);

      property = g_strconcat (property_base, "/memory/label", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_STRING, config, "memory-label");
      g_free (property);

      property = g_strconcat (property_base, "/memory/color", NULL);
      xfconf_g_property_bind_gdkrgba (channel, property, config, "memory-color");
      g_free (property);

      property = g_strconcat (property_base, "/swap/enabled", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, "swap-enabled");
      g_free (property);

      property = g_strconcat (property_base, "/swap/use-label", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, "swap-use-label");
      g_free (property);

      property = g_strconcat (property_base, "/swap/label", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_STRING, config, "swap-label");
      g_free (property);

      property = g_strconcat (property_base, "/swap/color", NULL);
      xfconf_g_property_bind_gdkrgba (channel, property, config, "swap-color");
      g_free (property);
    }

  return config;
}
