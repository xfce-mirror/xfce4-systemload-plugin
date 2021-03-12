/*
 * Copyright (c) 2003 Riccardo Persichetti <riccardo.persichetti@tin.it>
 * Copyright (c) 2010 Florian Rivoal <frivoal@xfce.org>
 * Copyright (c) 2012 David Schneider <dnschneid@gmail.com>
 * Copyright (c) 2014-2017 Landry Breuil <landry@xfce.org>
 * Copyright (c) 2021 Simon Steinbeiss <simon@xfce.org>
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

#include "settings.h"
#include "cpu.h"
#include "memswap.h"
#include "uptime.h"



typedef struct
{
    gboolean enabled;
    gboolean use_label;
    GdkRGBA  color;
    gchar    *label_text;
} t_monitor_options;

typedef struct
{
    gboolean enabled;
    gchar    *command_text;
} t_command;

typedef struct
{
    GtkWidget  *box;
    GtkWidget  *label;
    GtkWidget  *status;
    GtkWidget  *ebox;

    gulong     history[4];
    gulong     value_read;

    t_monitor_options options;
} t_monitor;

typedef struct
{
    GtkWidget  *label;
    GtkWidget  *ebox;

    gulong     value_read;
    gboolean   enabled;
} t_uptime_monitor;

typedef struct
{
    XfcePanelPlugin   *plugin;
    SystemloadConfig  *config;
    GtkWidget         *ebox;
    GtkWidget         *box;
    guint             timeout, timeout_seconds;
    gboolean          use_timeout_seconds;
    guint             timeout_id;
    t_command         command;
    t_monitor         *monitor[3];
    t_uptime_monitor  *uptime;
#ifdef HAVE_UPOWER_GLIB
    UpClient          *upower;
#endif
} t_global_monitor;

static gboolean
spawn_system_monitor(GtkWidget *w, t_global_monitor *global)
{
    // Spawn defined command; In-terminal: false, Startup-notify: false
    return xfce_spawn_command_line_on_screen(gdk_screen_get_default(),
                                             global->command.command_text,
                                             FALSE, FALSE, NULL);
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
    GtkAllocation alloc;
    gint max_alloc;

    /*
     * Try to avoid a call to GTK's bar_set_fraction() if the new fraction
     * isn't changing the number of pixels (height or width of the bar) already
     * displayed on the screen.
     *
     * max_alloc is independent from horizontal/vertical orientation of the bar.
     */

    gtk_widget_get_allocation(GTK_WIDGET(bar), &alloc);

    max_alloc = MAX(alloc.width, alloc.height);
    if (max_alloc > 0)
        fraction = round(fraction * max_alloc) / max_alloc;

    if (gtk_progress_bar_get_fraction(bar) != fraction)
        gtk_progress_bar_set_fraction(bar, fraction);
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

    gchar caption[128];
    gulong mem, swap, MTotal, MUsed, STotal, SUsed;
    gint days, hours, mins;
    gsize count;

    if (global->monitor[0]->options.enabled)
        global->monitor[0]->history[0] = read_cpuload();
    if (global->monitor[1]->options.enabled || global->monitor[2]->options.enabled) {
        read_memswap(&mem, &swap, &MTotal, &MUsed, &STotal, &SUsed);
        global->monitor[1]->history[0] = mem;
        global->monitor[2]->history[0] = swap;
    }
    if (systemload_config_get_uptime_enabled (global->config))
        global->uptime->value_read = read_uptime();

    for(count = 0; count < G_N_ELEMENTS (global->monitor); count++)
    {
        if (global->monitor[count]->options.enabled)
        {
            if (global->monitor[count]->history[0] > 100)
                global->monitor[count]->history[0] = 100;

            global->monitor[count]->value_read =
                (global->monitor[count]->history[0] +
                 global->monitor[count]->history[1] +
                 global->monitor[count]->history[2] +
                 global->monitor[count]->history[3]) / 4;

            global->monitor[count]->history[3] =
                global->monitor[count]->history[2];
            global->monitor[count]->history[2] =
                global->monitor[count]->history[1];
            global->monitor[count]->history[1] =
                global->monitor[count]->history[0];

            set_fraction(GTK_PROGRESS_BAR(global->monitor[count]->status),
                 global->monitor[count]->value_read / 100.0);
        }
    }
    if (global->monitor[0]->options.enabled)
    {
        g_snprintf(caption, sizeof(caption), _("System Load: %ld%%"),
                   global->monitor[0]->value_read);
        set_tooltip(global->monitor[0]->ebox, caption);
    }

    if (global->monitor[1]->options.enabled)
    {
        g_snprintf(caption, sizeof(caption), _("Memory: %ldMB of %ldMB used"),
                   MUsed >> 10 , MTotal >> 10);
        set_tooltip(global->monitor[1]->ebox, caption);
    }

    if (global->monitor[2]->options.enabled)
    {
        if (STotal)
            g_snprintf(caption, sizeof(caption), _("Swap: %ldMB of %ldMB used"),
                       SUsed >> 10, STotal >> 10);
        else
            g_snprintf(caption, sizeof(caption), _("No swap"));

        set_tooltip(global->monitor[2]->ebox, caption);
    }

    if (global->uptime->enabled)
    {
        days = global->uptime->value_read / 86400;
        hours = (global->uptime->value_read / 3600) % 24;
        mins = (global->uptime->value_read / 60) % 60;
        if (days > 0) {
            g_snprintf(caption, sizeof(caption), ngettext("%d day", "%d days", days), days);
            gtk_label_set_text(GTK_LABEL(global->uptime->label),
                               caption);
            g_snprintf(caption, sizeof(caption),
                       ngettext("Uptime: %d day %d:%02d", "Uptime: %d days %d:%02d", days),
                       days, hours, mins);
        }
        else
        {
            g_snprintf(caption, sizeof(caption), "%d:%02d", hours, mins);
            gtk_label_set_text(GTK_LABEL(global->uptime->label),
                               caption);
            g_snprintf(caption, sizeof(caption), _("Uptime: %d:%02d"), hours, mins);
        }
        set_tooltip(global->uptime->ebox, caption);
    }
}

static void
monitor_update_orientation (XfcePanelPlugin  *plugin,
                            GtkOrientation    panel_orientation,
                            GtkOrientation    orientation,
                            t_global_monitor *global)
{
    gsize count;
    gtk_orientable_set_orientation(GTK_ORIENTABLE(global->box), panel_orientation);
    for(count = 0; count < G_N_ELEMENTS (global->monitor); count++)
    {
        gtk_orientable_set_orientation(GTK_ORIENTABLE(global->monitor[count]->box), panel_orientation);
        gtk_label_set_angle(GTK_LABEL(global->monitor[count]->label),
                            (orientation == GTK_ORIENTATION_HORIZONTAL) ? 0 : -90);
        gtk_progress_bar_set_inverted (GTK_PROGRESS_BAR(global->monitor[count]->status), (panel_orientation == GTK_ORIENTATION_HORIZONTAL));
        gtk_orientable_set_orientation (GTK_ORIENTABLE(global->monitor[count]->status), !panel_orientation);
    }
    gtk_label_set_angle(GTK_LABEL(global->uptime->label),
                        (orientation == GTK_ORIENTATION_HORIZONTAL) ? 0 : -90);
}

static void
create_monitor (t_global_monitor *global)
{
    gsize count;
#if GTK_CHECK_VERSION (3, 16, 0)
    GtkCssProvider *css_provider;
#endif

    global->box = gtk_box_new(xfce_panel_plugin_get_orientation(global->plugin), 0);
    gtk_widget_show(global->box);

    for(count = 0; count < G_N_ELEMENTS (global->monitor); count++)
    {
        global->monitor[count]->label =
            gtk_label_new(global->monitor[count]->options.label_text);

        global->monitor[count]->status = GTK_WIDGET(gtk_progress_bar_new());
#if GTK_CHECK_VERSION (3, 16, 0)
        css_provider = gtk_css_provider_new ();
        gtk_style_context_add_provider (
            GTK_STYLE_CONTEXT (gtk_widget_get_style_context (GTK_WIDGET (global->monitor[count]->status))),
            GTK_STYLE_PROVIDER (css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_data (css_provider, "\
            progressbar.horizontal trough { min-height: 4px; }\
            progressbar.horizontal progress { min-height: 4px; }\
            progressbar.vertical trough { min-width: 4px; }\
            progressbar.vertical progress { min-width: 4px; }",
             -1, NULL);
        g_object_set_data(G_OBJECT(global->monitor[count]->status), "css_provider", css_provider);
#endif

        global->monitor[count]->box = gtk_box_new(xfce_panel_plugin_get_orientation(global->plugin), 0);

        gtk_box_pack_start(GTK_BOX(global->monitor[count]->box),
                           GTK_WIDGET(global->monitor[count]->label),
                           FALSE, FALSE, 2);

        global->monitor[count]->ebox = gtk_event_box_new();
        gtk_widget_show(global->monitor[count]->ebox);
        gtk_container_add(GTK_CONTAINER(global->monitor[count]->ebox),
                          GTK_WIDGET(global->monitor[count]->box));

        gtk_event_box_set_visible_window(GTK_EVENT_BOX(global->monitor[count]->ebox), FALSE);
        gtk_event_box_set_above_child(GTK_EVENT_BOX(global->monitor[count]->ebox), TRUE);

        gtk_widget_show(GTK_WIDGET(global->monitor[count]->status));

        gtk_box_pack_start(GTK_BOX(global->monitor[count]->box),
                           GTK_WIDGET(global->monitor[count]->status),
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(global->box),
                           GTK_WIDGET(global->monitor[count]->ebox),
                           FALSE, FALSE, 0);

        gtk_widget_show_all(GTK_WIDGET(global->monitor[count]->ebox));
    }

    global->uptime->ebox = gtk_event_box_new();
    if(global->uptime->enabled)
        gtk_widget_show(global->uptime->ebox);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(global->uptime->ebox), FALSE);

    global->uptime->label = gtk_label_new("");

    gtk_widget_show(global->uptime->label);
    gtk_container_add(GTK_CONTAINER(global->uptime->ebox),
                      GTK_WIDGET(global->uptime->label));

    gtk_box_pack_start(GTK_BOX(global->box),
                       GTK_WIDGET(global->uptime->ebox),
                       FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(global->ebox), GTK_WIDGET(global->box));
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(global->ebox), FALSE);
    gtk_widget_show(GTK_WIDGET(global->ebox));

    update_monitors (global);
}

static t_global_monitor *
monitor_control_new(XfcePanelPlugin *plugin)
{
    gsize count;
    t_global_monitor *global;

    global = g_new(t_global_monitor, 1);
#ifdef HAVE_UPOWER_GLIB
    global->upower = up_client_new();
#endif
    global->plugin = plugin;

    /* initialize xfconf */
    global->config = systemload_config_new (xfce_panel_plugin_get_property_base (plugin));

    global->timeout = systemload_config_get_timeout (global->config);
    if (global->timeout < 500)
        global->timeout = 500;
    global->timeout_seconds = systemload_config_get_timeout_seconds (global->config);
    if (global->timeout_seconds > 0)
        global->use_timeout_seconds = TRUE;

    global->use_timeout_seconds = TRUE;
    global->timeout_id = 0;
    global->ebox = gtk_event_box_new();
    gtk_widget_show(global->ebox);
    global->box = NULL;

    global->command.command_text = g_strdup (systemload_config_get_system_monitor_command (global->config));
    if (strlen(global->command.command_text) > 0)
        global->command.enabled = TRUE;

    xfce_panel_plugin_add_action_widget (plugin, global->ebox);

    for(count = 0; count < G_N_ELEMENTS (global->monitor); count++)
    {
        global->monitor[count] = g_new(t_monitor, 1);
        global->monitor[count]->history[0] = 0;
        global->monitor[count]->history[1] = 0;
        global->monitor[count]->history[2] = 0;
        global->monitor[count]->history[3] = 0;

    }
    global->monitor[CPU_MONITOR]->options.enabled = systemload_config_get_cpu_enabled (global->config);
    global->monitor[CPU_MONITOR]->options.use_label = systemload_config_get_cpu_use_label (global->config);
    global->monitor[CPU_MONITOR]->options.label_text = g_strdup (systemload_config_get_cpu_label (global->config));
    global->monitor[CPU_MONITOR]->options.color = *systemload_config_get_cpu_color (global->config);

    global->monitor[MEM_MONITOR]->options.enabled = systemload_config_get_memory_enabled (global->config);
    global->monitor[MEM_MONITOR]->options.use_label = systemload_config_get_memory_use_label (global->config);
    global->monitor[MEM_MONITOR]->options.label_text = g_strdup (systemload_config_get_memory_label (global->config));
    global->monitor[MEM_MONITOR]->options.color = *systemload_config_get_memory_color (global->config);

    global->monitor[SWAP_MONITOR]->options.enabled = systemload_config_get_swap_enabled (global->config);
    global->monitor[SWAP_MONITOR]->options.use_label = systemload_config_get_swap_use_label (global->config);
    global->monitor[SWAP_MONITOR]->options.label_text = g_strdup (systemload_config_get_swap_label (global->config));
    global->monitor[SWAP_MONITOR]->options.color = *systemload_config_get_swap_color (global->config);

    global->uptime = g_new(t_uptime_monitor, 1);
    global->uptime->enabled = systemload_config_get_uptime_enabled (global->config);

    return global;
}

static void
monitor_free(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    gsize count;

#ifdef HAVE_UPOWER_GLIB
    if (global->upower) {
        g_object_unref(global->upower);
        global->upower = NULL;
    }
#endif

    if (global->timeout_id)
        g_source_remove(global->timeout_id);

    g_free(global->command.command_text);

    for(count = 0; count < G_N_ELEMENTS (global->monitor); count++)
    {
        if (global->monitor[count]->options.label_text)
            g_free(global->monitor[count]->options.label_text);
        g_free(global->monitor[count]);
    }

    g_free(global->uptime);

    g_free(global);
}

static gboolean
update_monitors_cb(gpointer user_data)
{
    t_global_monitor *global = user_data;

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
setup_monitor(t_global_monitor *global)
{
    gsize count;
#if GTK_CHECK_VERSION (3, 16, 0)
    gchar *css, *color;
#endif

    gtk_widget_hide(GTK_WIDGET(global->uptime->ebox));

    for(count = 0; count < G_N_ELEMENTS (global->monitor); count++)
    {
        gtk_widget_hide(GTK_WIDGET(global->monitor[count]->ebox));
        gtk_widget_hide(global->monitor[count]->label);
        gtk_label_set_text(GTK_LABEL(global->monitor[count]->label),
                           global->monitor[count]->options.label_text);

        if (&global->monitor[count]->options.color)
        {
#if GTK_CHECK_VERSION (3, 16, 0)
        color = gdk_rgba_to_string(&global->monitor[count]->options.color);
#if GTK_CHECK_VERSION (3, 20, 0)
        css = g_strdup_printf("progressbar progress { background-color: %s; background-image: none; border-color: %s; }", color, color);
#else
        css = g_strdup_printf(".progressbar progress { background-color: %s; background-image: none; }", color);
#endif
        gtk_css_provider_load_from_data (
            g_object_get_data(G_OBJECT(global->monitor[count]->status), "css_provider"),
            css, strlen(css), NULL);
        g_free(color);
        g_free(css);
#else
        gtk_widget_override_background_color(GTK_WIDGET(global->monitor[count]->status),
                             GTK_STATE_PRELIGHT,
                             &global->monitor[count]->options.color);
        gtk_widget_override_background_color(GTK_WIDGET(global->monitor[count]->status),
                             GTK_STATE_SELECTED,
                             &global->monitor[count]->options.color);
        gtk_widget_override_color(GTK_WIDGET(global->monitor[count]->status),
                               GTK_STATE_SELECTED,
                               &global->monitor[count]->options.color);
#endif
        }

        if(global->monitor[count]->options.enabled)
        {
            gtk_widget_show(GTK_WIDGET(global->monitor[count]->ebox));
            if (global->monitor[count]->options.use_label)
                gtk_widget_show(global->monitor[count]->label);

            gtk_widget_show(GTK_WIDGET(global->monitor[count]->status));
        }
    }
    if(global->uptime->enabled)
    {
        if (global->monitor[0]->options.enabled ||
            global->monitor[1]->options.enabled ||
            global->monitor[2]->options.enabled)
        {
            gtk_container_set_border_width(GTK_CONTAINER(global->uptime->ebox), 2);
        }
        gtk_widget_show(GTK_WIDGET(global->uptime->ebox));
    }

    setup_timer(global);
}

static gboolean
monitor_set_size(XfcePanelPlugin *plugin, int size, t_global_monitor *global)
{
    gsize count;

    gtk_container_set_border_width (GTK_CONTAINER (global->ebox), (size > 26 ? 2 : 1));
    for(count = 0; count < G_N_ELEMENTS (global->monitor); count++)
    {
        if (xfce_panel_plugin_get_orientation (plugin) ==
                GTK_ORIENTATION_HORIZONTAL)
        {
            gtk_widget_set_size_request(GTK_WIDGET(global->monitor[count]->status),
                                        8, -1);
        }
        else
        {
            gtk_widget_set_size_request(GTK_WIDGET(global->monitor[count]->status),
                                        -1, 8);
        }
    }

    setup_monitor(global);

    return TRUE;
}


static void
monitor_set_mode (XfcePanelPlugin *plugin, XfcePanelPluginMode mode,
                  t_global_monitor *global)
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
#if UP_CHECK_VERSION(0, 99, 0)
upower_changed_cb(UpClient *client, GParamSpec *pspec, t_global_monitor *global)
#else /* UP_CHECK_VERSION < 0.99 */
upower_changed_cb(UpClient *client, t_global_monitor *global)
#endif /* UP_CHECK_VERSION */
{
    setup_timer(global);
}
#endif /* HAVE_UPOWER_GLIB */

static void
entry_changed_cb(GtkEntry *entry, t_global_monitor *global)
{
    gchar **charvar = (gchar**) g_object_get_data (G_OBJECT (entry), "charvar");
    gboolean *use_label = (gboolean*) g_object_get_data (G_OBJECT (entry), "boolvar");
    g_free (*charvar);
    *use_label = (gtk_entry_get_text_length (entry) != 0);
    *charvar = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
    setup_monitor (global);
}

static void
switch_cb(GtkSwitch *check_button, gboolean state, t_global_monitor *global)
{
    gboolean *boolvar = (gboolean*) g_object_get_data (G_OBJECT (check_button), "boolvar");
    gpointer sensitive_widget = g_object_get_data (G_OBJECT (check_button), "sensitive_widget");
    gboolean oldstate = *boolvar;
    *boolvar = state;
    gtk_switch_set_state (check_button, state);
    if (sensitive_widget)
        gtk_revealer_set_reveal_child (GTK_REVEALER (sensitive_widget), state);
    if (oldstate != state)
        setup_monitor (global);
}

static void
color_set_cb(GtkColorButton *color_button, t_global_monitor *global)
{
    GdkRGBA* colorvar;
    colorvar = (GdkRGBA*)g_object_get_data(G_OBJECT(color_button), "colorvar");
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_button), colorvar);
    setup_monitor(global);
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
    gboolean *use_timeout_seconds = (gboolean*) g_object_get_data (G_OBJECT (spin), "boolvar");
    global->timeout_seconds = gtk_spin_button_get_value (spin);
    *use_timeout_seconds = (global->timeout_seconds != 0);
    setup_timer (global);
}
#endif

/* Creates a label, its mnemonic will point to target.
 * Returns the widget. */
static GtkWidget *new_label (GtkGrid *grid, guint row,
                             const gchar *labeltext, GtkWidget *target)
{
    GtkWidget *label;

    label = gtk_label_new_with_mnemonic (labeltext);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start (label, 12);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), target);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    return label;
}

/* Create a new monitor setting  with gtkswitch, and eventually a color button and a checkbox + entry */
static void
new_monitor_setting (t_global_monitor *global, GtkGrid *grid, int position,
                     const gchar *title, gboolean *boolvar, GdkRGBA *colorvar,
                     gboolean *use_label, gchar **labeltext, const gchar *setting)
{
    GtkWidget *sw, *label;
    gchar *markup, *setting_name;
    GtkWidget *revealer, *subgrid, *button, *entry;

    sw = gtk_switch_new();
    g_object_set_data (G_OBJECT(sw), "boolvar", boolvar);
    gtk_switch_set_active (GTK_SWITCH(sw), *boolvar);
    gtk_widget_set_halign (sw, GTK_ALIGN_END);
    gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top (sw, 12);
    setting_name = g_strconcat (setting, "-enabled", NULL);
    g_object_bind_property (G_OBJECT (global->config), setting_name,
                            G_OBJECT (sw), "active",
                            G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
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


    if (g_strcmp0 (setting, "uptime") != 0)
    {
        revealer = gtk_revealer_new ();
        subgrid = gtk_grid_new ();
        gtk_container_add (GTK_CONTAINER (revealer), subgrid);
        gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
        g_object_set_data (G_OBJECT(sw), "sensitive_widget", revealer);
        gtk_grid_attach(GTK_GRID(grid), revealer, 0, position + 1, 2, 1);
        gtk_grid_set_column_spacing (GTK_GRID(subgrid), 12);
        gtk_grid_set_row_spacing (GTK_GRID(subgrid), 6);

        label = gtk_label_new_with_mnemonic (_("Options:"));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_start (label, 12);
        gtk_grid_attach (GTK_GRID(subgrid), label, 0, 0, 1, 1);

        /* Entry for the optional monitor label */
        entry = gtk_entry_new ();
        gtk_widget_set_hexpand (entry, TRUE);
        gtk_widget_set_margin_start (entry, 12);
        g_object_set_data (G_OBJECT(entry), "charvar", labeltext);
        g_object_set_data (G_OBJECT(entry), "boolvar", use_label);
        setting_name = g_strconcat (setting, "-label", NULL);
        g_object_bind_property (G_OBJECT (global->config), setting_name,
                                G_OBJECT (entry), "text",
                                G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
        g_free (setting_name);
        if (*use_label)
            gtk_entry_set_text (GTK_ENTRY (entry), *labeltext);
        g_signal_connect (G_OBJECT(entry), "changed",
        G_CALLBACK(entry_changed_cb), global);
        gtk_grid_attach(GTK_GRID(subgrid), entry, 1, 0, 1, 1);
        if (colorvar != NULL)
        {
            /* Colorbutton to set the progressbar color */
            button = gtk_color_button_new_with_rgba(colorvar);
            gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
            gtk_widget_set_halign(button, GTK_ALIGN_START);
            setting_name = g_strconcat (setting, "-color", NULL);
            g_object_bind_property (G_OBJECT (global->config), setting_name,
                                    G_OBJECT (button), "rgba",
                                    G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
            g_free (setting_name);
            g_object_set_data(G_OBJECT(button), "colorvar", colorvar);
            g_signal_connect(G_OBJECT(button), "color-set",
                         G_CALLBACK (color_set_cb), global);
            gtk_grid_attach(GTK_GRID(subgrid), button, 2, 0, 1, 1);
        }
    }

    switch_cb (GTK_SWITCH (sw), *boolvar, global);
}

static void
monitor_create_options(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    GtkWidget *dlg;
    GtkBox    *content;
    GtkWidget *grid, *label, *entry, *button, *box;
    gsize      count;

    static const gchar *FRAME_TEXT[] = {
            N_ ("CPU monitor"),
            N_ ("Memory monitor"),
            N_ ("Swap monitor"),
            N_ ("Uptime monitor")
    };
    static const gchar *DEFAULT_TEXT[] = {
            "cpu",
            "memory",
            "swap"
    };

    xfce_panel_plugin_block_menu (plugin);

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

    content = GTK_BOX(gtk_dialog_get_content_area (GTK_DIALOG(dlg)));

    grid = gtk_grid_new();
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
    button = gtk_spin_button_new_with_range (500, 10000, 50);
    gtk_label_set_mnemonic_widget (GTK_LABEL(label), button);
    gtk_widget_set_halign (button, GTK_ALIGN_START);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (button), (gfloat)global->timeout);
    g_object_bind_property (G_OBJECT (global->config), "timeout",
                            G_OBJECT (button), "value",
                            G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
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
    g_object_set_data (G_OBJECT(button), "boolvar", &global->use_timeout_seconds);
    g_object_bind_property (G_OBJECT (global->config), "timeout-seconds",
                            G_OBJECT (button), "value",
                            G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
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
    g_object_set_data (G_OBJECT(entry), "charvar", &global->command.command_text);
    gtk_entry_set_text (GTK_ENTRY(entry), global->command.command_text);
    g_object_set_data (G_OBJECT(entry), "boolvar", &global->command.enabled);
    gtk_widget_set_tooltip_text(GTK_WIDGET(entry), _("Launched when clicking on the plugin"));
    g_signal_connect (G_OBJECT(entry), "changed",
                      G_CALLBACK(entry_changed_cb), global);
    gtk_grid_attach (GTK_GRID (grid), entry, 1, 3, 1, 1);
    label = new_label (GTK_GRID (grid), 3, _("System monitor:"), entry);

    /* Add options for the three monitors */
    for(count = 0; count < G_N_ELEMENTS (global->monitor); count++)
    {
        t_monitor *monitor = global->monitor[count];

        new_monitor_setting(global, GTK_GRID(grid), 4 + 2 * count,
                           _(FRAME_TEXT[count]),
                           &monitor->options.enabled,
                           &monitor->options.color,
                           &monitor->options.use_label,
                           &monitor->options.label_text,
                           (DEFAULT_TEXT[count]));
    }

    /* Uptime monitor options */
    new_monitor_setting(global, GTK_GRID(grid), 11,
                       _(FRAME_TEXT[3]), &global->uptime->enabled,
                       NULL, NULL, NULL, "uptime");

    gtk_widget_show_all (dlg);
}

static void
monitor_show_about(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    const gchar *auth[] = {
      "Riccardo Persichetti <riccardo.persichetti@tin.it>",
      "Florian Rivoal <frivoal@xfce.org>",
      "Landry Breuil <landry@xfce.org>",
      "David Schneider <dnschneid@gmail.com>",
      "Simon Steinbeiß", NULL };

    gtk_show_about_dialog (NULL,
      "logo-icon-name", "org.xfce.panel.systemload",
      "license", xfce_get_license_text (XFCE_LICENSE_TEXT_BSD),
      "version", PACKAGE_VERSION,
      "program-name", PACKAGE_NAME,
      "comments", _("Monitor CPU load, swap usage and memory footprint"),
      "website", "https://docs.xfce.org/panel-plugins/xfce4-systemload-plugin/start",
      "copyright", _("Copyright (c) 2003-2021\n"),
      "authors", auth, NULL);
}

static void
systemload_construct (XfcePanelPlugin *plugin)
{
    t_global_monitor *global;

    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    global = monitor_control_new (plugin);

    create_monitor (global);
    monitor_set_mode (plugin,
                      xfce_panel_plugin_get_mode (plugin),
                      global);

    setup_monitor (global);

    gtk_container_add (GTK_CONTAINER (plugin), global->ebox);

    update_monitors (global);

#ifdef HAVE_UPOWER_GLIB
    if (global->upower) {
#if UP_CHECK_VERSION(0, 99, 0)
        g_signal_connect (global->upower, "notify",
                          G_CALLBACK(upower_changed_cb), global);
#else /* UP_CHECK_VERSION < 0.99 */
        g_signal_connect (global->upower, "changed",
                          G_CALLBACK(upower_changed_cb), global);
#endif /* UP_CHECK_VERSION */
    }
#endif /* HAVE_UPOWER_GLIB */

    g_signal_connect (plugin, "free-data", G_CALLBACK (monitor_free), global);

    g_signal_connect (plugin, "size-changed", G_CALLBACK (monitor_set_size),
                      global);

    g_signal_connect (plugin, "mode-changed",
                      G_CALLBACK (monitor_set_mode), global);

    g_signal_connect (plugin, "button-press-event", G_CALLBACK (click_event),
                      global);

    xfce_panel_plugin_menu_show_configure (plugin);
    g_signal_connect (plugin, "configure-plugin",
                      G_CALLBACK (monitor_create_options), global);

    xfce_panel_plugin_menu_show_about(plugin);
    g_signal_connect (plugin, "about", G_CALLBACK (monitor_show_about),
                       global);
}

XFCE_PANEL_PLUGIN_REGISTER (systemload_construct);
