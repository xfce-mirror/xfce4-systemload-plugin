/*
 * This file is part of Xfce (https://gitlab.xfce.org).
 *
 * Copyright (C) 2021 Simon Steinbeiß <simon@xfce.org>
 * Copyright (c) 2022 Jan Ziak <0xe2.0x9a.0x9b@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */



/*
 *  This file implements a configuration store. The class extends GObject.
 *
 */



#include <string.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#include "settings.h"




#define DEFAULT_TIMEOUT 500
#define DEFAULT_TIMEOUT_SECONDS 1
#define DEFAULT_SYSTEM_MONITOR_COMMAND "xfce4-taskmanager"
#define DEFAULT_UPTIME_LABEL "%hh %mm"

static const gchar *const DEFAULT_LABEL[] = {
    "cpu",
    "mem",
    "net",
    "swap",
};

static const gchar *const DEFAULT_COLOR[] = {
    "#1c71d8", /* CPU */
    "#2ec27e", /* MEM */
    "#e66100", /* NET */
    "#f5c211", /* SWAP */
};



static void                 systemload_config_finalize       (GObject          *object);
static void                 systemload_config_get_property   (GObject          *object,
                                                              guint             prop_id,
                                                              GValue           *value,
                                                              GParamSpec       *pspec);
static void                 systemload_config_set_property   (GObject          *object,
                                                              guint             prop_id,
                                                              const GValue     *value,
                                                              GParamSpec       *pspec);



struct _SystemloadConfigClass {
  GObjectClass     __parent__;
};

struct _SystemloadConfig {
  GObject          __parent__;

  XfconfChannel   *channel;
  gchar           *property_base;

  guint            timeout;
  guint            timeout_seconds;
  gchar           *system_monitor_command;
  bool             uptime;
  gchar           *uptime_label;

  struct {
    bool           enabled;
    bool           use_label;
    gchar         *label;
    GdkRGBA        color;
  } monitor[4];
};

enum SystemloadProperty {
    PROP_0,
    PROP_TIMEOUT,
    PROP_TIMEOUT_SECONDS,
    PROP_SYSTEM_MONITOR_COMMAND,
    PROP_UPTIME,
    PROP_UPTIME_LABEL,
    PROP_CPU_ENABLED,
    PROP_CPU_USE_LABEL,
    PROP_CPU_LABEL,
    PROP_CPU_COLOR,
    PROP_MEMORY_ENABLED,
    PROP_MEMORY_USE_LABEL,
    PROP_MEMORY_LABEL,
    PROP_MEMORY_COLOR,
    PROP_NETWORK_ENABLED,
    PROP_NETWORK_USE_LABEL,
    PROP_NETWORK_LABEL,
    PROP_NETWORK_COLOR,
    PROP_SWAP_ENABLED,
    PROP_SWAP_USE_LABEL,
    PROP_SWAP_LABEL,
    PROP_SWAP_COLOR,
    N_PROPERTIES,
};

enum {
    CONFIGURATION_CHANGED,
    LAST_SIGNAL
};

static guint systemload_config_signals [LAST_SIGNAL] = { 0, };

static SystemloadMonitor
prop2monitor (SystemloadProperty p)
{
  switch (p)
    {
    case PROP_CPU_ENABLED:
    case PROP_CPU_USE_LABEL:
    case PROP_CPU_LABEL:
    case PROP_CPU_COLOR:
      return CPU_MONITOR;
    case PROP_MEMORY_ENABLED:
    case PROP_MEMORY_USE_LABEL:
    case PROP_MEMORY_LABEL:
    case PROP_MEMORY_COLOR:
      return MEM_MONITOR;
    case PROP_NETWORK_ENABLED:
    case PROP_NETWORK_USE_LABEL:
    case PROP_NETWORK_LABEL:
    case PROP_NETWORK_COLOR:
      return NET_MONITOR;
    case PROP_SWAP_ENABLED:
    case PROP_SWAP_USE_LABEL:
    case PROP_SWAP_LABEL:
    case PROP_SWAP_COLOR:
      return SWAP_MONITOR;
    default:
      /* Ideally, this codepath is never reached */
      return CPU_MONITOR;
    }
}

static GdkRGBA
rgba_float (GdkRGBA color)
{
  return (GdkRGBA) {
    (float) color.red,
    (float) color.green,
    (float) color.blue,
    (float) color.alpha
  };
}

/* Compare two GdkRGBA values with the precision of float32 instead of the default precision of float64.
 * This is needed because the value of gdk_rgba_parse("#RRGGBB") can differ (by a delta of about 1e-17)
 * from the value returned by GtkColorButton if exactly the same "#RRGGBB" string is input by the user,
 * which indicates that either the GDK library or the GTK library is using mathematical operations
 * with a lower precision than the other library when processing the same "#RRGGBB" string. */
static bool
rgba_equal (GdkRGBA a, GdkRGBA b)
{
  a = rgba_float (a);
  b = rgba_float (b);
  return gdk_rgba_equal (&a, &b);
}

static bool
is_default_color (SystemloadMonitor m, const GdkRGBA *color)
{
  GdkRGBA default_color;
  if (G_LIKELY (gdk_rgba_parse (&default_color, DEFAULT_COLOR[m])))
    return rgba_equal (*color, default_color);
  else
    return FALSE;
}



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
                                                      GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_TIMEOUT_SECONDS,
                                   g_param_spec_uint ("timeout-seconds", NULL, NULL,
                                                      0, 10, DEFAULT_TIMEOUT_SECONDS,
                                                      GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_SYSTEM_MONITOR_COMMAND,
                                   g_param_spec_string ("system-monitor-command", NULL, NULL,
                                                        DEFAULT_SYSTEM_MONITOR_COMMAND,
                                                        GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_UPTIME,
                                   g_param_spec_boolean ("uptime-enabled", NULL, NULL,
                                                         TRUE,
                                                         GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_UPTIME_LABEL,
                                   g_param_spec_string ("uptime-label", NULL, NULL,
                                                        DEFAULT_UPTIME_LABEL,
                                                        GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_CPU_ENABLED,
                                   g_param_spec_boolean ("cpu-enabled", NULL, NULL,
                                                         TRUE,
                                                         GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_CPU_USE_LABEL,
                                   g_param_spec_boolean ("cpu-use-label", NULL, NULL,
                                                         TRUE,
                                                         GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_CPU_LABEL,
                                   g_param_spec_string ("cpu-label", NULL, NULL,
                                                        DEFAULT_LABEL[CPU_MONITOR],
                                                        GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_CPU_COLOR,
                                   g_param_spec_boxed ("cpu-color",
                                                       NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_MEMORY_ENABLED,
                                   g_param_spec_boolean ("memory-enabled", NULL, NULL,
                                                         TRUE,
                                                         GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_MEMORY_USE_LABEL,
                                   g_param_spec_boolean ("memory-use-label", NULL, NULL,
                                                         TRUE,
                                                         GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_MEMORY_LABEL,
                                   g_param_spec_string ("memory-label", NULL, NULL,
                                                        DEFAULT_LABEL[MEM_MONITOR],
                                                        GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_MEMORY_COLOR,
                                   g_param_spec_boxed ("memory-color",
                                                       NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_NETWORK_ENABLED,
                                   g_param_spec_boolean ("network-enabled", NULL, NULL,
                                                         TRUE,
                                                         GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_NETWORK_USE_LABEL,
                                   g_param_spec_boolean ("network-use-label", NULL, NULL,
                                                         TRUE,
                                                         GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_NETWORK_LABEL,
                                   g_param_spec_string ("network-label", NULL, NULL,
                                                        DEFAULT_LABEL[NET_MONITOR],
                                                        GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_NETWORK_COLOR,
                                   g_param_spec_boxed ("network-color",
                                                       NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_SWAP_ENABLED,
                                   g_param_spec_boolean ("swap-enabled", NULL, NULL,
                                                         TRUE,
                                                         GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_SWAP_USE_LABEL,
                                   g_param_spec_boolean ("swap-use-label", NULL, NULL,
                                                         TRUE,
                                                         GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_SWAP_LABEL,
                                   g_param_spec_string ("swap-label", NULL, NULL,
                                                        DEFAULT_LABEL[SWAP_MONITOR],
                                                        GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class,
                                   PROP_SWAP_COLOR,
                                   g_param_spec_boxed ("swap-color",
                                                       NULL, NULL,
                                                       GDK_TYPE_RGBA,
                                                       GParamFlags (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

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
  config->system_monitor_command = g_strdup (DEFAULT_SYSTEM_MONITOR_COMMAND);
  config->uptime = true;
  config->uptime_label = g_strdup (DEFAULT_UPTIME_LABEL);
  for (gsize i = 0; i < G_N_ELEMENTS (config->monitor); i++)
    {
      config->monitor[i].enabled = true;
      config->monitor[i].use_label = true;
      config->monitor[i].label = g_strdup (DEFAULT_LABEL[i]);
      gdk_rgba_parse (&config->monitor[i].color, DEFAULT_COLOR[i]);
    }
}



static void
systemload_config_finalize (GObject *object)
{
  SystemloadConfig *config = SYSTEMLOAD_CONFIG (object);

  xfconf_shutdown();
  g_free (config->property_base);
  g_free (config->system_monitor_command);
  g_free (config->uptime_label);
  for (gsize i = 0; i < G_N_ELEMENTS (config->monitor); i++)
    g_free (config->monitor[i].label);

  G_OBJECT_CLASS (systemload_config_parent_class)->finalize (object);
}



static void
systemload_config_get_property (GObject    *object,
                                guint       _prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  const SystemloadConfig *config = SYSTEMLOAD_CONFIG (object);

  auto prop_id = SystemloadProperty (_prop_id);
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

    case PROP_UPTIME_LABEL:
      g_value_set_string (value, config->uptime_label);
      break;

    case PROP_CPU_ENABLED:
    case PROP_MEMORY_ENABLED:
    case PROP_NETWORK_ENABLED:
    case PROP_SWAP_ENABLED:
      g_value_set_boolean (value, config->monitor[prop2monitor(prop_id)].enabled);
      break;

    case PROP_CPU_USE_LABEL:
    case PROP_MEMORY_USE_LABEL:
    case PROP_NETWORK_USE_LABEL:
    case PROP_SWAP_USE_LABEL:
      g_value_set_boolean (value, config->monitor[prop2monitor(prop_id)].use_label);
      break;

    case PROP_CPU_LABEL:
    case PROP_MEMORY_LABEL:
    case PROP_NETWORK_LABEL:
    case PROP_SWAP_LABEL:
      g_value_set_string (value, config->monitor[prop2monitor(prop_id)].label);
      break;

    case PROP_CPU_COLOR:
    case PROP_MEMORY_COLOR:
    case PROP_NETWORK_COLOR:
    case PROP_SWAP_COLOR:
      g_value_set_boxed (value, &config->monitor[prop2monitor(prop_id)].color);
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
  SystemloadConfig *config = SYSTEMLOAD_CONFIG (object);
  gboolean          val_bool;
  GdkRGBA          *val_rgba;
  const char       *val_string;
  guint             val_uint;

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
      val_string = g_value_get_string (value);
      if (g_strcmp0 (config->system_monitor_command, val_string) != 0)
        {
          g_free (config->system_monitor_command);
          config->system_monitor_command = g_value_dup_string (value);
          g_object_notify (G_OBJECT (config), "system-monitor-command");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
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

    case PROP_UPTIME_LABEL:
      val_string = g_value_get_string (value);
      if (g_strcmp0 (config->uptime_label, val_string) != 0)
        {
          g_free (config->uptime_label);
          config->uptime_label = g_value_dup_string (value);
          g_object_notify (G_OBJECT (config), "uptime-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_CPU_ENABLED:
      val_bool = g_value_get_boolean (value);
      if (config->monitor[CPU_MONITOR].enabled != val_bool)
        {
          config->monitor[CPU_MONITOR].enabled = val_bool;
          g_object_notify (G_OBJECT (config), "cpu-enabled");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_CPU_USE_LABEL:
      val_bool = g_value_get_boolean (value);
      if (config->monitor[CPU_MONITOR].use_label != val_bool)
        {
          config->monitor[CPU_MONITOR].use_label = val_bool;
          g_object_notify (G_OBJECT (config), "cpu-use-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_CPU_LABEL:
      val_string = g_value_get_string (value);
      if (g_strcmp0 (config->monitor[CPU_MONITOR].label, val_string) != 0)
        {
          g_free (config->monitor[CPU_MONITOR].label);
          config->monitor[CPU_MONITOR].label = g_value_dup_string (value);
          g_object_notify (G_OBJECT (config), "cpu-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_CPU_COLOR:
      val_rgba = (GdkRGBA*) g_value_dup_boxed (value);
      if (!rgba_equal (config->monitor[CPU_MONITOR].color, *val_rgba))
        {
          config->monitor[CPU_MONITOR].color = *val_rgba;
          g_object_notify (G_OBJECT (config), "cpu-color");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      if (is_default_color (CPU_MONITOR, val_rgba))
        {
          char *property = g_strconcat (config->property_base, "/cpu/color", NULL);
          xfconf_channel_reset_property (config->channel, property, TRUE);
          g_free (property);
        }
      g_boxed_free (GDK_TYPE_RGBA, val_rgba);
      break;

    case PROP_MEMORY_ENABLED:
      val_bool = g_value_get_boolean (value);
      if (config->monitor[MEM_MONITOR].enabled != val_bool)
        {
          config->monitor[MEM_MONITOR].enabled = val_bool;
          g_object_notify (G_OBJECT (config), "memory-enabled");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_MEMORY_USE_LABEL:
      val_bool = g_value_get_boolean (value);
      if (config->monitor[MEM_MONITOR].use_label != val_bool)
        {
          config->monitor[MEM_MONITOR].use_label = val_bool;
          g_object_notify (G_OBJECT (config), "memory-use-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_MEMORY_LABEL:
      val_string = g_value_get_string (value);
      if (g_strcmp0 (config->monitor[MEM_MONITOR].label, val_string) != 0)
        {
          g_free (config->monitor[MEM_MONITOR].label);
          config->monitor[MEM_MONITOR].label = g_value_dup_string (value);
          g_object_notify (G_OBJECT (config), "memory-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_MEMORY_COLOR:
      val_rgba = (GdkRGBA*) g_value_dup_boxed (value);
      if (!rgba_equal (config->monitor[MEM_MONITOR].color, *val_rgba))
        {
          config->monitor[MEM_MONITOR].color = *val_rgba;
          g_object_notify (G_OBJECT (config), "memory-color");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      if (is_default_color (MEM_MONITOR, val_rgba))
        {
          char *property = g_strconcat (config->property_base, "/memory/color", NULL);
          xfconf_channel_reset_property (config->channel, property, TRUE);
          g_free (property);
        }
      g_boxed_free (GDK_TYPE_RGBA, val_rgba);
      break;

    case PROP_NETWORK_ENABLED:
      val_bool = g_value_get_boolean (value);
      if (config->monitor[NET_MONITOR].enabled != val_bool)
        {
          config->monitor[NET_MONITOR].enabled = val_bool;
          g_object_notify (G_OBJECT (config), "network-enabled");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_NETWORK_USE_LABEL:
      val_bool = g_value_get_boolean (value);
      if (config->monitor[NET_MONITOR].use_label != val_bool)
        {
          config->monitor[NET_MONITOR].use_label = val_bool;
          g_object_notify (G_OBJECT (config), "network-use-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_NETWORK_LABEL:
      val_string = g_value_get_string (value);
      if (g_strcmp0 (config->monitor[NET_MONITOR].label, val_string) != 0)
        {
          g_free (config->monitor[NET_MONITOR].label);
          config->monitor[NET_MONITOR].label = g_value_dup_string (value);
          g_object_notify (G_OBJECT (config), "network-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_NETWORK_COLOR:
      val_rgba = (GdkRGBA*) g_value_dup_boxed (value);
      if (!rgba_equal (config->monitor[NET_MONITOR].color, *val_rgba))
        {
          config->monitor[NET_MONITOR].color = *val_rgba;
          g_object_notify (G_OBJECT (config), "network-color");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      if (is_default_color (NET_MONITOR, val_rgba))
        {
          char *property = g_strconcat (config->property_base, "/network/color", NULL);
          xfconf_channel_reset_property (config->channel, property, TRUE);
          g_free (property);
        }
      g_boxed_free (GDK_TYPE_RGBA, val_rgba);
      break;

    case PROP_SWAP_ENABLED:
      val_bool = g_value_get_boolean (value);
      if (config->monitor[SWAP_MONITOR].enabled != val_bool)
        {
          config->monitor[SWAP_MONITOR].enabled = val_bool;
          g_object_notify (G_OBJECT (config), "swap-enabled");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_SWAP_USE_LABEL:
      val_bool = g_value_get_boolean (value);
      if (config->monitor[SWAP_MONITOR].use_label != val_bool)
        {
          config->monitor[SWAP_MONITOR].use_label = val_bool;
          g_object_notify (G_OBJECT (config), "swap-use-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_SWAP_LABEL:
      val_string = g_value_get_string (value);
      if (g_strcmp0 (config->monitor[SWAP_MONITOR].label, val_string) != 0)
        {
          g_free (config->monitor[SWAP_MONITOR].label);
          config->monitor[SWAP_MONITOR].label = g_value_dup_string (value);
          g_object_notify (G_OBJECT (config), "swap-label");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      break;

    case PROP_SWAP_COLOR:
      val_rgba = (GdkRGBA*) g_value_dup_boxed (value);
      if (!rgba_equal (config->monitor[SWAP_MONITOR].color, *val_rgba))
        {
          config->monitor[SWAP_MONITOR].color = *val_rgba;
          g_object_notify (G_OBJECT (config), "swap-color");
          g_signal_emit (G_OBJECT (config), systemload_config_signals [CONFIGURATION_CHANGED], 0);
        }
      if (is_default_color (SWAP_MONITOR, val_rgba))
        {
          char *property = g_strconcat (config->property_base, "/swap/color", NULL);
          xfconf_channel_reset_property (config->channel, property, TRUE);
          g_free (property);
        }
      g_boxed_free (GDK_TYPE_RGBA, val_rgba);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



guint
systemload_config_get_timeout (const SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), DEFAULT_TIMEOUT);

  return config->timeout;
}

guint
systemload_config_get_timeout_seconds (const SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), DEFAULT_TIMEOUT_SECONDS);

  return config->timeout_seconds;
}

const gchar*
systemload_config_get_system_monitor_command (const SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), DEFAULT_SYSTEM_MONITOR_COMMAND);

  return config->system_monitor_command;
}

bool
systemload_config_get_uptime_enabled (const SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), true);

  return config->uptime;
}

gchar*
systemload_config_get_uptime_label (const SystemloadConfig *config)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), NULL);

  return config->uptime_label;
}

bool
systemload_config_get_enabled (const SystemloadConfig *config, SystemloadMonitor monitor)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), true);

  if (monitor >= 0 && (gsize) monitor < G_N_ELEMENTS (config->monitor))
      return config->monitor[monitor].enabled;
  else
      return true;
}

bool
systemload_config_get_use_label (const SystemloadConfig *config, SystemloadMonitor monitor)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), true);

  if (monitor >= 0 && (gsize) monitor < G_N_ELEMENTS (config->monitor))
      return config->monitor[monitor].use_label;
  else
      return true;
}

const gchar *
systemload_config_get_label (const SystemloadConfig *config, SystemloadMonitor monitor)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), "");

  if (monitor >= 0 && (gsize) monitor < G_N_ELEMENTS (config->monitor))
      return config->monitor[monitor].label;
  else
      return "";
}

const GdkRGBA *
systemload_config_get_color (const SystemloadConfig *config, SystemloadMonitor monitor)
{
  g_return_val_if_fail (IS_SYSTEMLOAD_CONFIG (config), NULL);

  if (monitor >= 0 && (gsize) monitor < G_N_ELEMENTS (config->monitor))
      return &config->monitor[monitor].color;
  else
      return NULL;
}



SystemloadConfig *
systemload_config_new (const gchar *property_base)
{
  auto config = (SystemloadConfig*) g_object_new (TYPE_SYSTEMLOAD_CONFIG, NULL);

  if (xfconf_init (NULL))
    {
      XfconfChannel *channel = xfconf_channel_get ("xfce4-panel");
      gchar *property;

      config->channel = channel;
      config->property_base = g_strdup (property_base);

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

      property = g_strconcat (property_base, "/uptime/label", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_STRING, config, "uptime-label");
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

      property = g_strconcat (property_base, "/network/enabled", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, "network-enabled");
      g_free (property);

      property = g_strconcat (property_base, "/network/use-label", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_BOOLEAN, config, "network-use-label");
      g_free (property);

      property = g_strconcat (property_base, "/network/label", NULL);
      xfconf_g_property_bind (channel, property, G_TYPE_STRING, config, "network-label");
      g_free (property);

      property = g_strconcat (property_base, "/network/color", NULL);
      xfconf_g_property_bind_gdkrgba (channel, property, config, "network-color");
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



void systemload_config_on_change (SystemloadConfig     *config,
                                  gboolean             (*callback)(gpointer user_data),
                                  gpointer             user_data)
{
    g_signal_connect_swapped (G_OBJECT (config), "configuration-changed", G_CALLBACK (callback), user_data);
}
