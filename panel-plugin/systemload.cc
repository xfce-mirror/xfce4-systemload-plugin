/*
 * This file is part of Xfce (https://gitlab.xfce.org).
 *
 * Copyright (c) 2003 Riccardo Persichetti <riccardo.persichetti@tin.it>
 * Copyright (c) 2010 Florian Rivoal <frivoal@xfce.org>
 * Copyright (c) 2012 David Schneider <dnschneid@gmail.com>
 * Copyright (c) 2014-2017 Landry Breuil <landry@xfce.org>
 * Copyright (c) 2021 Simon Steinbeiss <simon@xfce.org>
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
#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include <string>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/libxfce4panel.h>

#ifdef HAVE_UPOWER_GLIB
#include <upower.h>
#endif

#ifdef HAVE_LIBGTOP
#include <glibtop.h>
#endif

#include "cpu.h"
#include "memswap.h"
#include "network.h"
#include "plugin.h"
#include "settings.h"
#include "uptime.h"



struct t_command {
    bool  enabled;
    gchar *command_text;
};

struct t_monitor {
    GtkWidget  *box;
    GtkWidget  *label;
    GtkWidget  *status;
    GtkWidget  *ebox;

    gulong     value_read; /* Range: 0% ... 100% */
};

struct t_uptime_monitor {
    GtkWidget  *label;
    GtkWidget  *ebox;

    gulong     value_read;
};

struct t_global_monitor {
    XfcePanelPlugin   *plugin;
    SystemloadConfig  *config;
    GtkWidget         *ebox;
    GtkWidget         *box;
    guint             timeout, timeout_seconds;
    bool              use_timeout_seconds;
    guint             timeout_id;
    t_command         command;
    t_monitor         *monitor[4];
    t_uptime_monitor  uptime;
#ifdef HAVE_UPOWER_GLIB
    UpClient          *upower;
#endif
};



static const SystemloadMonitor VISUAL_ORDER[] = {
    CPU_MONITOR,
    MEM_MONITOR,
    SWAP_MONITOR,
    NET_MONITOR,
};

static gboolean setup_monitor_cb(gpointer user_data);



static bool
spawn_system_monitor(GtkWidget *w, t_global_monitor *global)
{
    // Spawn defined command; In-terminal: false, Startup-notify: false
    return xfce_spawn_command_line(gdk_screen_get_default(),
                                   global->command.command_text,
                                   FALSE, FALSE, TRUE, NULL);
}

static gboolean
click_event(GtkWidget *w, GdkEventButton *event, t_global_monitor *global)
{
    if(event->button == 1 && global->command.enabled && *(global->command.command_text))
        return spawn_system_monitor(w, global);
    return FALSE;
}

static void
set_fraction(GtkProgressBar *bar, gdouble fraction)
{
    /*
     * Try to avoid a call to GTK's bar_set_fraction() if the new fraction
     * isn't changing the number of pixels (height or width of the bar) already
     * displayed on the screen.
     *
     * max_alloc is independent from horizontal/vertical orientation of the bar.
     */

    GtkAllocation alloc;
    gtk_widget_get_allocation(GTK_WIDGET(bar), &alloc);

    gint max_alloc = MAX(alloc.width, alloc.height);
    if (max_alloc > 1)
        fraction = round(fraction * max_alloc) / max_alloc;

    if (gtk_progress_bar_get_fraction(bar) != fraction)
        gtk_progress_bar_set_fraction(bar, fraction);
}

static void
set_label_text(GtkLabel *label, const gchar *text)
{
    const gchar *displayed_text = gtk_label_get_text(label);
    if (g_strcmp0(displayed_text, text) != 0)
        gtk_label_set_text(label, text);
}

static void
date_format(std::string& format_string, const gchar* replace_str, std::string value_str)
{
    size_t pos = 0;
    pos = format_string.find(replace_str, pos);
    while (pos != std::string::npos)
    {
        format_string.replace(pos, 2, value_str);
        pos = format_string.find(replace_str, pos);
    }
}

static void
set_tooltip(GtkWidget *w, const gchar *caption)
{
    gchar *displayed_caption = gtk_widget_get_tooltip_text(w);
    if (g_strcmp0(displayed_caption, caption) != 0)
        gtk_widget_set_tooltip_text(w, caption);
    g_free(displayed_caption);
}

static void
update_monitors(t_global_monitor *global)
{
    const SystemloadConfig *config = global->config;
    gulong MTotal = 0, MUsed = 0, NTotal = 0, STotal = 0, SUsed = 0;

    for(gsize i = 0; i < G_N_ELEMENTS (global->monitor); i++)
        global->monitor[i]->value_read = 0;

    if (systemload_config_get_enabled (config, CPU_MONITOR))
        global->monitor[CPU_MONITOR]->value_read = read_cpuload();
    if (systemload_config_get_enabled (config, MEM_MONITOR) ||
        systemload_config_get_enabled (config, SWAP_MONITOR))
    {
        gulong mem, swap;
        if (read_memswap(&mem, &swap, &MTotal, &MUsed, &STotal, &SUsed) == 0)
        {
            global->monitor[MEM_MONITOR]->value_read = mem;
            global->monitor[SWAP_MONITOR]->value_read = swap;
        }
    }
    if (systemload_config_get_enabled (config, NET_MONITOR))
    {
        gulong net;
        if (read_netload (&net, &NTotal) == 0)
            global->monitor[NET_MONITOR]->value_read = net;
    }
    if (systemload_config_get_uptime_enabled (config))
        global->uptime.value_read = read_uptime();

    for(gsize i = 0; i < G_N_ELEMENTS (global->monitor); i++)
    {
        const auto monitor = (SystemloadMonitor) i;
        t_monitor *m = global->monitor[monitor];

        if (systemload_config_get_enabled (config, monitor))
        {
            gulong value = MIN(m->value_read, 100);
            set_fraction(GTK_PROGRESS_BAR(global->monitor[i]->status), value / 100.0);
        }
    }

    if (systemload_config_get_enabled (config, CPU_MONITOR))
    {
        gchar tooltip[128];
        g_snprintf(tooltip, sizeof(tooltip), _("System Load: %ld%%"), global->monitor[CPU_MONITOR]->value_read);
        set_tooltip(global->monitor[CPU_MONITOR]->ebox, tooltip);
    }

    if (systemload_config_get_enabled (config, MEM_MONITOR))
    {
        gchar tooltip[128];
        g_snprintf(tooltip, sizeof(tooltip), _("Memory: %ldMB of %ldMB used"), MUsed >> 10 , MTotal >> 10);
        set_tooltip(global->monitor[MEM_MONITOR]->ebox, tooltip);
    }

    if (systemload_config_get_enabled (config, NET_MONITOR))
    {
        gchar tooltip[128];
        g_snprintf(tooltip, sizeof(tooltip), _("Network: %ld Mbit/s"), (glong) round (NTotal / 1e6));
        set_tooltip(global->monitor[NET_MONITOR]->ebox, tooltip);
    }

    if (systemload_config_get_enabled (config, SWAP_MONITOR))
    {
        gchar tooltip[128];

        if (STotal)
            g_snprintf(tooltip, sizeof(tooltip), _("Swap: %ldMB of %ldMB used"), SUsed >> 10, STotal >> 10);
        else
            g_snprintf(tooltip, sizeof(tooltip), _("No swap"));

        set_tooltip(global->monitor[SWAP_MONITOR]->ebox, tooltip);
    }

    if (systemload_config_get_uptime_enabled (config))
    {
        gchar days_str[32], hours_str[32], mins_str[32];
        gchar tooltip[128];

        const gchar* format = systemload_config_get_uptime_label(config);
        std::string formatted_date = format;

        gint days = global->uptime.value_read / 86400;
        gint hours = (global->uptime.value_read / 3600) % 24;
        gint mins = (global->uptime.value_read / 60) % 60;
        gint secs = (global->uptime.value_read) % 60;

        date_format(formatted_date, "%d", std::to_string(days));
        date_format(formatted_date, "%h", std::to_string(hours));
        date_format(formatted_date, "%m", std::to_string(mins));
        date_format(formatted_date, "%s", std::to_string(secs));

        set_label_text(GTK_LABEL(global->uptime.label), formatted_date.c_str());

        // Tooltip text
        g_snprintf(days_str, sizeof(days_str), ngettext("%d day", "%d days", days), days);
        g_snprintf(hours_str, sizeof(hours_str), ngettext("%d hour", "%d hours", hours), hours);
        g_snprintf(mins_str, sizeof(mins_str), ngettext("%d minute", "%d minutes", mins), mins);

        g_snprintf(tooltip, sizeof(tooltip), _("Uptime: %s, %s, %s"), days_str, hours_str, mins_str);
        set_tooltip(global->uptime.ebox, tooltip);
    }
}

static void
monitor_update_orientation (XfcePanelPlugin  *plugin,
                            GtkOrientation    panel_orientation,
                            GtkOrientation    orientation,
                            t_global_monitor *global)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(global->box), panel_orientation);
    for(gsize count = 0; count < G_N_ELEMENTS (global->monitor); count++)
    {
        gtk_orientable_set_orientation(GTK_ORIENTABLE(global->monitor[count]->box), panel_orientation);
        gtk_label_set_angle(GTK_LABEL(global->monitor[count]->label),
                            (orientation == GTK_ORIENTATION_HORIZONTAL) ? 0 : -90);
        gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR(global->monitor[count]->status), (panel_orientation == GTK_ORIENTATION_HORIZONTAL));
        gtk_orientable_set_orientation (GTK_ORIENTABLE(global->monitor[count]->status),
                                        (panel_orientation == GTK_ORIENTATION_HORIZONTAL) ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
    }
    gtk_label_set_angle(GTK_LABEL(global->uptime.label),
                        (orientation == GTK_ORIENTATION_HORIZONTAL) ? 0 : -90);
}

static void
create_monitor (t_global_monitor *global)
{
    const SystemloadConfig *config = global->config;

    global->box = gtk_box_new(xfce_panel_plugin_get_orientation(global->plugin), 0);
    gtk_widget_show(global->box);

    for(gsize i = 0; i < G_N_ELEMENTS (global->monitor); i++)
    {
        SystemloadMonitor monitor = VISUAL_ORDER[i];
        t_monitor *m = global->monitor[monitor];

        m->label = gtk_label_new (systemload_config_get_label (config, monitor));

        m->status = GTK_WIDGET(gtk_progress_bar_new());
        GtkCssProvider *css_provider = gtk_css_provider_new ();
        gtk_style_context_add_provider (
            GTK_STYLE_CONTEXT (gtk_widget_get_style_context (GTK_WIDGET (m->status))),
            GTK_STYLE_PROVIDER (css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_data (css_provider, "\
            progressbar.horizontal trough { min-height: 4px; }\
            progressbar.horizontal progress { min-height: 4px; }\
            progressbar.vertical trough { min-width: 4px; }\
            progressbar.vertical progress { min-width: 4px; }",
             -1, NULL);
        g_object_set_data(G_OBJECT(m->status), "css_provider", css_provider);

        m->box = gtk_box_new(xfce_panel_plugin_get_orientation(global->plugin), 0);

        gtk_box_pack_start(GTK_BOX(m->box), GTK_WIDGET(m->label), FALSE, FALSE, 0);

        m->ebox = gtk_event_box_new();
        gtk_widget_show(m->ebox);
        gtk_container_add(GTK_CONTAINER(m->ebox), GTK_WIDGET(m->box));

        gtk_event_box_set_visible_window(GTK_EVENT_BOX(m->ebox), FALSE);
        gtk_event_box_set_above_child(GTK_EVENT_BOX(m->ebox), TRUE);

        gtk_widget_show(GTK_WIDGET(m->status));

        gtk_box_pack_start(GTK_BOX(m->box), GTK_WIDGET(m->status), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(global->box), GTK_WIDGET(m->ebox), FALSE, FALSE, 0);

        gtk_widget_show_all(GTK_WIDGET(m->ebox));
    }

    global->uptime.ebox = gtk_event_box_new();
    if (systemload_config_get_uptime_enabled (config))
        gtk_widget_show(global->uptime.ebox);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(global->uptime.ebox), FALSE);

    global->uptime.label = gtk_label_new("");

    gtk_widget_show(global->uptime.label);
    gtk_container_add(GTK_CONTAINER(global->uptime.ebox),
                      GTK_WIDGET(global->uptime.label));

    gtk_box_pack_start(GTK_BOX(global->box),
                       GTK_WIDGET(global->uptime.ebox),
                       FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(global->ebox), GTK_WIDGET(global->box));
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(global->ebox), FALSE);
    gtk_widget_show(GTK_WIDGET(global->ebox));

    update_monitors (global);
}

static t_global_monitor *
monitor_control_new(XfcePanelPlugin *plugin)
{
    t_global_monitor *global = g_new0 (t_global_monitor, 1);
#ifdef HAVE_UPOWER_GLIB
    global->upower = up_client_new();
#endif
    global->plugin = plugin;

    /* initialize xfconf */
    global->config = systemload_config_new (xfce_panel_plugin_get_property_base (plugin));

    global->timeout = systemload_config_get_timeout (global->config);
    if (global->timeout < MIN_TIMEOUT)
        global->timeout = MIN_TIMEOUT;
    global->timeout_seconds = systemload_config_get_timeout_seconds (global->config);
    global->use_timeout_seconds = (global->timeout_seconds > 0);

    global->ebox = gtk_event_box_new();
    gtk_widget_show(global->ebox);

    global->command.command_text = g_strdup (systemload_config_get_system_monitor_command (global->config));
    if (strlen(global->command.command_text) > 0)
        global->command.enabled = true;

    xfce_panel_plugin_add_action_widget (plugin, global->ebox);

    for(gsize i = 0; i < G_N_ELEMENTS (global->monitor); i++)
        global->monitor[i] = g_new0 (t_monitor, 1);

    systemload_config_on_change (global->config, setup_monitor_cb, global);

    return global;
}

static void
monitor_free(XfcePanelPlugin *plugin, t_global_monitor *global)
{
#ifdef HAVE_UPOWER_GLIB
    if (global->upower) {
        g_object_unref(global->upower);
        global->upower = NULL;
    }
#endif

    if (global->timeout_id)
        g_source_remove(global->timeout_id);

    g_free(global->command.command_text);

    for(gsize i = 0; i < G_N_ELEMENTS (global->monitor); i++)
        g_free (global->monitor[i]);

    g_free(global);
}

static gboolean
update_monitors_cb(gpointer user_data)
{
    auto global = (t_global_monitor*) user_data;

    update_monitors (global);
    return TRUE;
}

static void
setup_timer(t_global_monitor *global)
{
    GtkSettings *settings;
    if (global->timeout_id)
        g_source_remove(global->timeout_id);
#ifdef HAVE_UPOWER_GLIB
    if (global->upower && global->use_timeout_seconds) {
        if (up_client_get_on_battery(global->upower)) {
            if (!up_client_get_lid_is_closed(global->upower)) {
                global->timeout_id = g_timeout_add_seconds(
                                        global->timeout_seconds,
                                        update_monitors_cb, global);
            } else {
                /* Don't do any timeout if the lid is closed on battery */
                global->timeout_id = 0;
            }
            return;
        }
    }
#endif
    global->timeout_id = g_timeout_add(global->timeout, update_monitors_cb, global);
    /* reduce the default tooltip timeout to be smaller than the update interval otherwise
     * we won't see tooltips on GTK 2.16 or newer */
    settings = gtk_settings_get_default();
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(settings), "gtk-tooltip-timeout"))
        g_object_set(settings, "gtk-tooltip-timeout",
                     global->timeout - 10, NULL);

}

static void
set_margin (const t_global_monitor *global, GtkWidget *w, gint margin)
{
    if (xfce_panel_plugin_get_orientation (global->plugin) == GTK_ORIENTATION_HORIZONTAL)
    {
        gtk_widget_set_margin_start (w, margin);
        gtk_widget_set_margin_top (w, 0);
    }
    else
    {
        gtk_widget_set_margin_start (w, 0);
        gtk_widget_set_margin_top (w, margin);
    }
}

static void
setup_monitors(t_global_monitor *global)
{
    const SystemloadConfig *config = global->config;

    gtk_widget_hide(GTK_WIDGET(global->uptime.ebox));

    /* determine the number of enabled monitors and the number of enabled labels */
    guint n_enabled = 0, n_enabled_labels = 0;
    for(gsize i = 0; i < G_N_ELEMENTS (global->monitor); i++)
    {
        auto monitor = (SystemloadMonitor) i;
        if (systemload_config_get_enabled (config, monitor))
        {
            bool label_visible = systemload_config_get_use_label (config, monitor) &&
                                 strlen (systemload_config_get_label (config, monitor)) != 0;
            n_enabled++;
            n_enabled_labels += (label_visible ? 1 : 0);
        }
    }

    for(gsize i = 0; i < G_N_ELEMENTS (global->monitor); i++)
    {
        const auto monitor = (SystemloadMonitor) i;
        const t_monitor *m = global->monitor[monitor];
        const GdkRGBA *color = NULL;

        gtk_widget_hide(m->ebox);
        gtk_widget_hide(m->label);
        gtk_label_set_text(GTK_LABEL(m->label), systemload_config_get_label (config, monitor));

        color = systemload_config_get_color (config, monitor);
        if (G_LIKELY (color != NULL))
        {
            gchar *color_str = gdk_rgba_to_string(color);
            gchar *css;
            css = g_strdup_printf("progressbar progress { background-color: %s; background-image: none; border-color: %s; }",
                                  color_str, color_str);
            gtk_css_provider_load_from_data (
                (GtkCssProvider*) g_object_get_data(G_OBJECT(m->status), "css_provider"),
                css, strlen(css), NULL);
            g_free(color_str);
            g_free(css);
        }

        if (systemload_config_get_enabled (config, monitor))
        {
            bool label_visible = systemload_config_get_use_label (config, monitor) &&
                                 strlen (systemload_config_get_label (config, monitor)) != 0;

            gtk_widget_show_all(GTK_WIDGET(m->ebox));
            gtk_widget_set_visible (m->label, label_visible);
            set_margin (global, m->ebox, (n_enabled_labels == 0) ? 0 : 6);
        }
    }

    if (systemload_config_get_uptime_enabled (config))
    {
        gtk_widget_show_all (global->uptime.ebox);
        set_margin (global, global->uptime.ebox, (n_enabled == 0) ? 0 : 6);
    }

    setup_timer (global);
}

static gboolean
setup_monitor_cb(gpointer user_data)
{
    auto global = (t_global_monitor*) user_data;
    setup_monitors (global);
    update_monitors (global);
    return TRUE;
}

static gboolean
monitor_set_size(XfcePanelPlugin *plugin, int size, t_global_monitor *global)
{
    gtk_container_set_border_width (GTK_CONTAINER (global->ebox), (size > 26 ? 2 : 1));
    for(gsize i = 0; i < G_N_ELEMENTS (global->monitor); i++)
    {
        if (xfce_panel_plugin_get_orientation (plugin) == GTK_ORIENTATION_HORIZONTAL)
        {
            gtk_widget_set_size_request(GTK_WIDGET(global->monitor[i]->status), 8, -1);
        }
        else
        {
            gtk_widget_set_size_request(GTK_WIDGET(global->monitor[i]->status), -1, 8);
        }
    }

    setup_monitors (global);

    return TRUE;
}


static void
monitor_set_mode (XfcePanelPlugin *plugin, XfcePanelPluginMode mode, t_global_monitor *global)
{
  GtkOrientation panel_orientation = xfce_panel_plugin_get_orientation (plugin);
  GtkOrientation orientation = (mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL) ?
    GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;

  /* Set small in all modes except deskbar mode */
  if (mode == XFCE_PANEL_PLUGIN_MODE_DESKBAR)
      xfce_panel_plugin_set_small(XFCE_PANEL_PLUGIN(plugin), FALSE);
  else
      xfce_panel_plugin_set_small(XFCE_PANEL_PLUGIN(plugin), TRUE);

  monitor_update_orientation (plugin, panel_orientation, orientation, global);
  monitor_set_size (plugin, xfce_panel_plugin_get_size (plugin), global);
}

#ifdef HAVE_UPOWER_GLIB
static void
upower_changed_cb(UpClient *client, GParamSpec *pspec, t_global_monitor *global)
{
    setup_timer(global);
}
#endif /* HAVE_UPOWER_GLIB */

static void
command_entry_changed_cb(GtkEntry *entry, t_global_monitor *global)
{
    g_free (global->command.command_text);
    global->command.command_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
    global->command.enabled = (strlen (global->command.command_text) != 0);
}

static void
switch_cb(GtkSwitch *check_button, gboolean state, t_global_monitor *global)
{
    gpointer sensitive_widget = g_object_get_data (G_OBJECT (check_button), "sensitive_widget");
    gtk_switch_set_state (check_button, state);
    if (sensitive_widget)
        gtk_revealer_set_reveal_child (GTK_REVEALER (sensitive_widget), state);
}

static void
monitor_dialog_response (GtkWidget *dlg, int response,
                         t_global_monitor *global)
{
    if (response == GTK_RESPONSE_HELP)
    {
        xfce_dialog_show_help (GTK_WINDOW (dlg), PACKAGE_NAME, NULL, NULL);
    }
    else
    {
        gtk_widget_destroy (dlg);
        xfce_panel_plugin_unblock_menu (global->plugin);
    }
}

static void
change_timeout_cb(GtkSpinButton *spin, t_global_monitor *global)
{
    global->timeout = gtk_spin_button_get_value(spin);

    setup_timer(global);
}

#ifdef HAVE_UPOWER_GLIB
static void
change_timeout_seconds_cb(GtkSpinButton *spin, t_global_monitor *global)
{
    global->timeout_seconds = gtk_spin_button_get_value (spin);
    global->use_timeout_seconds = (global->timeout_seconds != 0);
    setup_timer (global);
}
#endif

/* Creates a label, its mnemonic will point to target.
 * Returns the widget. */
static GtkWidget *new_label (GtkGrid *grid, guint row, const gchar *labeltext, GtkWidget *target)
{
    GtkWidget *label = gtk_label_new_with_mnemonic (labeltext);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start (label, 12);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), target);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    return label;
}

/* Create a new monitor setting  with gtkswitch, and eventually a color button and a checkbox + entry */
static void
new_monitor_setting (t_global_monitor *global,
                     GtkGrid *grid, int position,
                     const gchar *title, bool color,
                     const gchar *setting)
{
    GtkWidget *sw, *label;
    gchar *markup, *setting_name;
    gboolean enabled = TRUE;

    sw = gtk_switch_new();
    gtk_widget_set_halign (sw, GTK_ALIGN_END);
    gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top (sw, 12);
    setting_name = g_strconcat (setting, "-enabled", NULL);
    g_object_get (G_OBJECT (global->config), setting_name, &enabled, NULL);
    g_object_bind_property (G_OBJECT (global->config), setting_name,
                            G_OBJECT (sw), "active",
                            GBindingFlags (G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
    g_signal_connect (GTK_WIDGET(sw), "state-set",
                      G_CALLBACK(switch_cb), global);
    g_free (setting_name);

    markup = g_markup_printf_escaped ("<b>%s</b>", title);
    label = gtk_label_new (markup);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top (label, 12);
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    g_free (markup);
    gtk_grid_attach(GTK_GRID(grid), label, 0, position, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), sw, 1, position, 1, 1);


    GtkWidget *revealer = gtk_revealer_new ();
    GtkWidget *subgrid = gtk_grid_new ();
    gtk_container_add (GTK_CONTAINER (revealer), subgrid);
    gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
    g_object_set_data (G_OBJECT(sw), "sensitive_widget", revealer);
    gtk_grid_attach(GTK_GRID(grid), revealer, 0, position + 1, 2, 1);
    gtk_grid_set_column_spacing (GTK_GRID(subgrid), 12);
    gtk_grid_set_row_spacing (GTK_GRID(subgrid), 6);

    label = gtk_label_new_with_mnemonic (_("Label:"));
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start (label, 12);
    gtk_grid_attach (GTK_GRID(subgrid), label, 0, 0, 1, 1);

    /* Entry for the optional monitor label */
    GtkWidget *entry = gtk_entry_new ();
    gtk_widget_set_hexpand (entry, TRUE);
    if (g_strcmp0 (setting, "uptime") != 0)
        gtk_widget_set_tooltip_text (entry, _("Leave empty to disable the label"));
    else
        gtk_widget_set_tooltip_text (entry, _("Use percent-formatting to format the time (see help page for details)."));
    setting_name = g_strconcat (setting, "-label", NULL);
    g_object_bind_property (G_OBJECT (global->config), setting_name,
                            G_OBJECT (entry), "text",
                            GBindingFlags (G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
    g_free (setting_name);
    gtk_grid_attach(GTK_GRID(subgrid), entry, 1, 0, 1, 1);

    if (color)
    {
        /* Colorbutton to set the progressbar color */
        GtkWidget *button = gtk_color_button_new ();
        gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (button), TRUE);

        gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
        gtk_widget_set_halign(button, GTK_ALIGN_START);
        gtk_widget_set_margin_start (button, 12);
        setting_name = g_strconcat (setting, "-color", NULL);
        g_object_bind_property (G_OBJECT (global->config), setting_name,
                                G_OBJECT (button), "rgba",
                                GBindingFlags (G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
        g_free (setting_name);
        gtk_grid_attach(GTK_GRID(subgrid), button, 2, 0, 1, 1);
    }

    switch_cb (GTK_SWITCH (sw), enabled, global);
}

static void
monitor_create_options(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    SystemloadConfig *config = global->config;
    GtkWidget *label, *entry, *button, *box;

    static const gchar *FRAME_TEXT[] = {
            N_ ("CPU monitor"),
            N_ ("Memory monitor"),
            N_ ("Network monitor"),
            N_ ("Swap monitor"),
            N_ ("Uptime monitor")
    };
    static const gchar *SETTING_TEXT[] = {
            "cpu",
            "memory",
            "network",
            "swap"
    };

    xfce_panel_plugin_block_menu (plugin);

    GtkWidget *dlg;
    dlg = xfce_titled_dialog_new_with_mixed_buttons (_("System Load Monitor"),
                     GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               "window-close-symbolic", _("_Close"), GTK_RESPONSE_OK,
                                               "help-browser", _("_Help"), GTK_RESPONSE_HELP,
                                               NULL);

    g_signal_connect (G_OBJECT (dlg), "response",
                      G_CALLBACK (monitor_dialog_response), global);

    gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name (GTK_WINDOW (dlg), "org.xfce.panel.systemload");

    GtkBox *content = GTK_BOX(gtk_dialog_get_content_area (GTK_DIALOG(dlg)));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing (GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing (GTK_GRID(grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_box_pack_start (content, grid, TRUE, TRUE, 0);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), _("<b>General</b>"));
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

    /* Update interval */
    button = gtk_spin_button_new_with_range (MIN_TIMEOUT, MAX_TIMEOUT, 50);
    gtk_label_set_mnemonic_widget (GTK_LABEL(label), button);
    gtk_widget_set_halign (button, GTK_ALIGN_START);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (button), (gfloat)global->timeout);
    g_object_bind_property (G_OBJECT (config), "timeout",
                            G_OBJECT (button), "value",
                            GBindingFlags (G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
    g_signal_connect (G_OBJECT (button), "value-changed", G_CALLBACK(change_timeout_cb), global);
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    label = gtk_label_new ("ms");
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
    gtk_grid_attach (GTK_GRID (grid), box, 1, 1, 1, 1);
    new_label (GTK_GRID (grid), 1, _("Update interval:"), button);

#ifdef HAVE_UPOWER_GLIB
    /* Power-saving interval */
    button = gtk_spin_button_new_with_range (0, 10, 1);
    gtk_widget_set_halign (button, GTK_ALIGN_START);
    gtk_widget_set_tooltip_text(GTK_WIDGET(button), _("Update interval when running on battery (uses regular update interval if set to zero)"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (button), (gfloat)global->timeout_seconds);
    g_object_bind_property (G_OBJECT (config), "timeout-seconds",
                            G_OBJECT (button), "value",
                            GBindingFlags (G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
    g_signal_connect (G_OBJECT (button), "value-changed", G_CALLBACK(change_timeout_seconds_cb), global);
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    label = gtk_label_new ("s");
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
    gtk_grid_attach (GTK_GRID (grid), box, 1, 2, 1, 1);
    new_label (GTK_GRID (grid), 2, _("Power-saving interval:"), button);
#endif

    /* System Monitor */
    entry = gtk_entry_new ();
    gtk_widget_set_hexpand (entry, TRUE);
    gtk_entry_set_text (GTK_ENTRY(entry), global->command.command_text);
    gtk_widget_set_tooltip_text(GTK_WIDGET(entry), _("Launched when clicking on the plugin"));
    g_object_bind_property (G_OBJECT (config), "system-monitor-command",
                            G_OBJECT (entry), "text",
                            GBindingFlags (G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));
    g_signal_connect (G_OBJECT(entry), "changed",
                      G_CALLBACK(command_entry_changed_cb), global);
    gtk_grid_attach (GTK_GRID (grid), entry, 1, 3, 1, 1);
    new_label (GTK_GRID (grid), 3, _("System monitor:"), entry);

    /* Add options for the monitors */
    for(gsize i = 0; i < G_N_ELEMENTS (global->monitor); i++)
    {
        const SystemloadMonitor monitor = VISUAL_ORDER[i];
        new_monitor_setting (global, GTK_GRID(grid), 4 + 2 * i,
                             _(FRAME_TEXT[monitor]),
                             true,
                             SETTING_TEXT[monitor]);
    }

    /* Uptime monitor options */
    new_monitor_setting (global, GTK_GRID(grid), 4 + 2*G_N_ELEMENTS (global->monitor),
                         _(FRAME_TEXT[4]), FALSE, "uptime");

    gtk_widget_show_all (dlg);
}

static void
monitor_show_about(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    /* List of authors (in alphabetical order) */
    const gchar *auth[] = {
      "David Schneider <dnschneid@gmail.com>",
      "Florian Rivoal <frivoal@xfce.org>",
      "Jan Ziak <0xe2.0x9a.0x9b@xfce.org>",
      "Landry Breuil <landry@xfce.org>",
      "Riccardo Persichetti <riccardo.persichetti@tin.it>",
      "Simon Steinbeiß <simon@xfce.org>",
      NULL
    };

    gtk_show_about_dialog (NULL,
      "logo-icon-name", "org.xfce.panel.systemload",
      "license", xfce_get_license_text (XFCE_LICENSE_TEXT_BSD),
      "version", VERSION_FULL,
      "program-name", PACKAGE_NAME,
      "comments", _("Monitor CPU load, swap usage and memory footprint"),
      "website", "https://docs.xfce.org/panel-plugins/xfce4-systemload-plugin/start",
      "copyright", "Copyright \302\251 2003-" COPYRIGHT_YEAR " The Xfce development team",
      "authors", auth, NULL);
}

void
systemload_construct (XfcePanelPlugin *plugin)
{
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

#ifdef HAVE_LIBGTOP
    /* Consider add glibtop_close() somewhere */
    glibtop_init();
#endif

    t_global_monitor *global = monitor_control_new (plugin);

    create_monitor (global);
    monitor_set_mode (plugin, xfce_panel_plugin_get_mode (plugin), global);

    setup_monitors (global);

    gtk_container_add (GTK_CONTAINER (plugin), global->ebox);

    update_monitors (global);

#ifdef HAVE_UPOWER_GLIB
    if (global->upower) {
        g_signal_connect (global->upower, "notify", G_CALLBACK(upower_changed_cb), global);
    }
#endif /* HAVE_UPOWER_GLIB */

    g_signal_connect (plugin, "free-data", G_CALLBACK (monitor_free), global);
    g_signal_connect (plugin, "size-changed", G_CALLBACK (monitor_set_size), global);
    g_signal_connect (plugin, "mode-changed", G_CALLBACK (monitor_set_mode), global);
    g_signal_connect (plugin, "button-press-event", G_CALLBACK (click_event), global);

    xfce_panel_plugin_menu_show_configure (plugin);
    g_signal_connect (plugin, "configure-plugin", G_CALLBACK (monitor_create_options), global);

    xfce_panel_plugin_menu_show_about(plugin);
    g_signal_connect (plugin, "about", G_CALLBACK (monitor_show_about), global);
}
