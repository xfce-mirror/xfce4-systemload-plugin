/*
 * Copyright (c) 2003 Riccardo Persichetti <riccardo.persichetti@tin.it>
 * Copyright (c) 2010 Florian Rivoal <frivoal@xfce.org>
 * Copyright (c) 2012 David Schneider <dnschneid@gmail.com>
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

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#ifdef HAVE_UPOWER_GLIB
#include <upower.h>
#endif

#include "cpu.h"
#include "memswap.h"
#include "uptime.h"

/* for xml: */
static gchar *MONITOR_ROOT[] = { "SL_Cpu", "SL_Mem", "SL_Swap", "SL_Uptime" };

static gchar *DEFAULT_TEXT[] = { "cpu", "mem", "swap" };
static gchar *DEFAULT_COLOR[] = { "#0000c0", "#00c000", "#f0f000" };

#define UPDATE_TIMEOUT 250
#define UPDATE_TIMEOUT_SECONDS 1

#define BORDER 8

/* check for new Xfce 4.10 panel features */
#ifdef LIBXFCE4PANEL_CHECK_VERSION
#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
#define HAS_PANEL_49
#endif
#endif

enum { CPU_MONITOR, MEM_MONITOR, SWAP_MONITOR };

typedef struct
{
    gboolean enabled;
    gboolean use_label;
    GdkColor color;
    gchar    *label_text;
} t_monitor_options;

typedef struct
{
    GtkWidget  *box;
    GtkWidget  *label;
    GtkWidget  *status;
    GtkWidget  *ebox;
    GtkWidget  *tooltip_text;

    gulong     history[4];
    gulong     value_read;

    t_monitor_options options;
} t_monitor;

typedef struct
{
    GtkWidget  *label;
    GtkWidget  *ebox;
    GtkWidget  *tooltip_text;

    gulong     value_read;
    gboolean   enabled;
} t_uptime_monitor;

typedef struct
{
    XfcePanelPlugin   *plugin;
    GtkWidget         *ebox;
    GtkWidget         *box;
    guint             timeout, timeout_seconds;
    gboolean          use_timeout_seconds;
    guint             timeout_id;
    t_monitor         *monitor[3];
    t_uptime_monitor  *uptime;
#ifdef HAVE_UPOWER_GLIB
    UpClient          *upower;
#endif
} t_global_monitor;

static gint
update_monitors(t_global_monitor *global)
{

    gchar caption[128];
    gulong mem, swap, MTotal, MUsed, STotal, SUsed;
    gint count, days, hours, mins;

    if (global->monitor[0]->options.enabled)
        global->monitor[0]->history[0] = read_cpuload();
    if (global->monitor[1]->options.enabled || global->monitor[2]->options.enabled) {
        read_memswap(&mem, &swap, &MTotal, &MUsed, &STotal, &SUsed);
        global->monitor[1]->history[0] = mem;
        global->monitor[2]->history[0] = swap;
    }
    if (global->uptime->enabled)
        global->uptime->value_read = read_uptime();

    for(count = 0; count < 3; count++)
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

            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(global->monitor[count]->status),
                 global->monitor[count]->value_read / 100.0);
        }
    }
    if (global->monitor[0]->options.enabled)
    {
        g_snprintf(caption, sizeof(caption), _("System Load: %ld%%"),
                   global->monitor[0]->value_read);
        gtk_label_set_text(GTK_LABEL(global->monitor[0]->tooltip_text), caption);
    }

    if (global->monitor[1]->options.enabled)
    {
        g_snprintf(caption, sizeof(caption), _("Memory: %ldMB of %ldMB used"),
                   MUsed >> 10 , MTotal >> 10);
        gtk_label_set_text(GTK_LABEL(global->monitor[1]->tooltip_text), caption);
    }

    if (global->monitor[2]->options.enabled)
    {
        if (STotal)
            g_snprintf(caption, sizeof(caption), _("Swap: %ldMB of %ldMB used"),
                       SUsed >> 10, STotal >> 10);
        else
            g_snprintf(caption, sizeof(caption), _("No swap"));

        gtk_label_set_text(GTK_LABEL(global->monitor[2]->tooltip_text), caption);
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
        gtk_label_set_text(GTK_LABEL(global->uptime->tooltip_text), caption);
    }
    return TRUE;
}

static gboolean tooltip_cb0(GtkWidget *widget, gint x, gint y, gboolean keyboard, GtkTooltip *tooltip, t_global_monitor *global)
{
        gtk_tooltip_set_custom(tooltip, global->monitor[0]->tooltip_text);
        return TRUE;
}

static gboolean tooltip_cb1(GtkWidget *widget, gint x, gint y, gboolean keyboard, GtkTooltip *tooltip, t_global_monitor *global)
{
        gtk_tooltip_set_custom(tooltip, global->monitor[1]->tooltip_text);
        return TRUE;
}

static gboolean tooltip_cb2(GtkWidget *widget, gint x, gint y, gboolean keyboard, GtkTooltip *tooltip, t_global_monitor *global)
{
        gtk_tooltip_set_custom(tooltip, global->monitor[2]->tooltip_text);
        return TRUE;
}

static gboolean tooltip_cb3(GtkWidget *widget, gint x, gint y, gboolean keyboard, GtkTooltip *tooltip, t_global_monitor *global)
{
        gtk_tooltip_set_custom(tooltip, global->uptime->tooltip_text);
        return TRUE;
}

static void
monitor_update_orientation (XfcePanelPlugin  *plugin,
                            GtkOrientation    panel_orientation,
                            GtkOrientation    orientation,
                            t_global_monitor *global)
{
    gint count;

    gtk_widget_hide(GTK_WIDGET(global->ebox));

    if (global->box)
        gtk_container_remove(GTK_CONTAINER(global->ebox), 
                             GTK_WIDGET(global->box));
    
    if (panel_orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        global->box = gtk_hbox_new(FALSE, 0);
    }
    else
    {
        global->box = gtk_vbox_new(FALSE, 0);
    }
    gtk_widget_show(global->box);

    for(count = 0; count < 3; count++)
    {
        global->monitor[count]->label =
            gtk_label_new(global->monitor[count]->options.label_text);
        gtk_label_set_angle(GTK_LABEL(global->monitor[count]->label),
                            (orientation == GTK_ORIENTATION_HORIZONTAL) ? 0 : -90);

        global->monitor[count]->status = GTK_WIDGET(gtk_progress_bar_new());

        if (panel_orientation == GTK_ORIENTATION_HORIZONTAL)
        {
            global->monitor[count]->box = GTK_WIDGET(gtk_hbox_new(FALSE, 0));
            gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(global->monitor[count]->status), GTK_PROGRESS_BOTTOM_TO_TOP);
        }
        else
        {
            global->monitor[count]->box = GTK_WIDGET(gtk_vbox_new(FALSE, 0));
            gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(global->monitor[count]->status), GTK_PROGRESS_LEFT_TO_RIGHT);
        }

        gtk_box_pack_start(GTK_BOX(global->monitor[count]->box),
                           GTK_WIDGET(global->monitor[count]->label),
                           FALSE, FALSE, 0);

        global->monitor[count]->ebox = gtk_event_box_new();
        gtk_widget_show(global->monitor[count]->ebox);
        gtk_container_add(GTK_CONTAINER(global->monitor[count]->ebox),
                          GTK_WIDGET(global->monitor[count]->box));

        gtk_widget_modify_bg(GTK_WIDGET(global->monitor[count]->status),
                             GTK_STATE_PRELIGHT,
                             &global->monitor[count]->options.color);
        gtk_widget_modify_bg(GTK_WIDGET(global->monitor[count]->status),
                             GTK_STATE_SELECTED,
                             &global->monitor[count]->options.color);
        gtk_widget_modify_base(GTK_WIDGET(global->monitor[count]->status),
                               GTK_STATE_SELECTED,
                               &global->monitor[count]->options.color);
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(global->monitor[count]->ebox), FALSE);
        gtk_event_box_set_above_child(GTK_EVENT_BOX(global->monitor[count]->ebox), TRUE);

        gtk_widget_show(GTK_WIDGET(global->monitor[count]->status));

        gtk_box_pack_start(GTK_BOX(global->monitor[count]->box),
                           GTK_WIDGET(global->monitor[count]->status),
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(global->box),
                           GTK_WIDGET(global->monitor[count]->ebox),
                           FALSE, FALSE, 0);

        if(global->monitor[count]->options.enabled)
        {
            gtk_widget_show(GTK_WIDGET(global->monitor[count]->ebox));
            gtk_widget_show(GTK_WIDGET(global->monitor[count]->box));
            if (global->monitor[count]->options.use_label)
                gtk_widget_show(global->monitor[count]->label);

            gtk_widget_show(GTK_WIDGET(global->monitor[count]->status));
        }
    }

    global->uptime->ebox = gtk_event_box_new();
    if(global->uptime->enabled)
        gtk_widget_show(global->uptime->ebox);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(global->uptime->ebox), FALSE);

    gtk_widget_set_has_tooltip(global->monitor[0]->ebox, TRUE);
    gtk_widget_set_has_tooltip(global->monitor[1]->ebox, TRUE);
    gtk_widget_set_has_tooltip(global->monitor[2]->ebox, TRUE);
    gtk_widget_set_has_tooltip(global->uptime->ebox, TRUE);
    g_signal_connect(global->monitor[0]->ebox, "query-tooltip", G_CALLBACK(tooltip_cb0), global);
    g_signal_connect(global->monitor[1]->ebox, "query-tooltip", G_CALLBACK(tooltip_cb1), global);
    g_signal_connect(global->monitor[2]->ebox, "query-tooltip", G_CALLBACK(tooltip_cb2), global);
    g_signal_connect(global->uptime->ebox, "query-tooltip", G_CALLBACK(tooltip_cb3), global);


    global->uptime->label = gtk_label_new("");
    gtk_label_set_angle(GTK_LABEL(global->uptime->label),
                        (orientation == GTK_ORIENTATION_HORIZONTAL) ? 0 : -90);

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
    int count;
    t_global_monitor *global;
    
    global = g_new(t_global_monitor, 1);
#ifdef HAVE_UPOWER_GLIB
    global->upower = up_client_new();
#endif
    global->plugin = plugin;
    global->timeout = UPDATE_TIMEOUT;
    global->timeout_seconds = UPDATE_TIMEOUT_SECONDS;
    global->use_timeout_seconds = TRUE;
    global->timeout_id = 0;
    global->ebox = gtk_event_box_new();
    gtk_container_set_border_width (GTK_CONTAINER (global->ebox), BORDER/2);
    gtk_widget_show(global->ebox);
    global->box = NULL;

    xfce_panel_plugin_add_action_widget (plugin, global->ebox);
    
    for(count = 0; count < 3; count++)
    {
        global->monitor[count] = g_new(t_monitor, 1);
        global->monitor[count]->options.label_text =
            g_strdup(DEFAULT_TEXT[count]);
        gdk_color_parse(DEFAULT_COLOR[count],
                        &global->monitor[count]->options.color);

        global->monitor[count]->options.use_label = TRUE;
        global->monitor[count]->options.enabled = TRUE;

        global->monitor[count]->history[0] = 0;
        global->monitor[count]->history[1] = 0;
        global->monitor[count]->history[2] = 0;
        global->monitor[count]->history[3] = 0;
        global->monitor[count]->tooltip_text = gtk_label_new(NULL);
        g_object_ref(global->monitor[count]->tooltip_text);

    }
    
    global->uptime = g_new(t_uptime_monitor, 1);
    global->uptime->enabled = TRUE;
    global->uptime->tooltip_text = gtk_label_new(NULL);
    g_object_ref(global->uptime->tooltip_text);
    
    return global;
}

static void
monitor_free(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    gint count;

#ifdef HAVE_UPOWER_GLIB
    if (global->upower) {
        g_object_unref(global->upower);
        global->upower = NULL;
    }
#endif

    if (global->timeout_id)
        g_source_remove(global->timeout_id);

    for(count = 0; count < 3; count++)
    {
        if (global->monitor[count]->options.label_text)
            g_free(global->monitor[count]->options.label_text);
        gtk_widget_destroy(global->monitor[count]->tooltip_text);
        g_free(global->monitor[count]);
    }

    gtk_widget_destroy(global->uptime->tooltip_text);


    g_free(global->uptime);

    g_free(global);
}

static void
setup_timer(t_global_monitor *global)
{
    if (global->timeout_id)
        g_source_remove(global->timeout_id);
#ifdef HAVE_UPOWER_GLIB
    if (global->upower && global->use_timeout_seconds) {
        if (up_client_get_on_battery(global->upower)) {
            if (!up_client_get_lid_is_closed(global->upower)) {
                global->timeout_id = g_timeout_add_seconds(
                                        global->timeout_seconds,
                                        (GSourceFunc)update_monitors, global);
            } else {
                /* Don't do any timeout if the lid is closed on battery */
                global->timeout_id = 0;
            }
            return;
        }
    }
#endif
    global->timeout_id = g_timeout_add(global->timeout, (GSourceFunc)update_monitors, global);
}

static void
setup_monitor(t_global_monitor *global)
{
    gint count;

    gtk_widget_hide(GTK_WIDGET(global->uptime->ebox));

    for(count = 0; count < 3; count++)
    {
        gtk_widget_hide(GTK_WIDGET(global->monitor[count]->ebox));
        gtk_widget_hide(global->monitor[count]->label);
        gtk_label_set_text(GTK_LABEL(global->monitor[count]->label),
                           global->monitor[count]->options.label_text);

        gtk_widget_modify_bg(GTK_WIDGET(global->monitor[count]->status),
                             GTK_STATE_PRELIGHT,
                             &global->monitor[count]->options.color);
        gtk_widget_modify_bg(GTK_WIDGET(global->monitor[count]->status),
                             GTK_STATE_SELECTED,
                             &global->monitor[count]->options.color);
        gtk_widget_modify_base(GTK_WIDGET(global->monitor[count]->status),
                               GTK_STATE_SELECTED,
                               &global->monitor[count]->options.color);

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

static void
monitor_read_config(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    gint count;
    const char *value;
    char *file;
    XfceRc *rc;
    
    if (!(file = xfce_panel_plugin_lookup_rc_file (plugin)))
        return;
    
    rc = xfce_rc_simple_open (file, TRUE);
    g_free (file);

    if (!rc)
        return;
    
    if (xfce_rc_has_group (rc, "Main"))
    {
        xfce_rc_set_group (rc, "Main");
        global->timeout = xfce_rc_read_int_entry (rc, "Timeout", global->timeout);
        global->timeout_seconds = xfce_rc_read_int_entry (
                rc, "Timeout_Seconds", global->timeout_seconds);
        global->use_timeout_seconds = xfce_rc_read_bool_entry (
                rc, "Use_Timeout_Seconds", global->use_timeout_seconds);
    }

    for(count = 0; count < 3; count++)
    {
        if (xfce_rc_has_group (rc, MONITOR_ROOT[count]))
        {
            xfce_rc_set_group (rc, MONITOR_ROOT[count]);
            
            global->monitor[count]->options.enabled = 
                xfce_rc_read_bool_entry (rc, "Enabled", TRUE);

            global->monitor[count]->options.use_label = 
                xfce_rc_read_bool_entry (rc, "Use_Label", TRUE);
            
            if ((value = xfce_rc_read_entry (rc, "Color", NULL)))
            {
                gdk_color_parse(value,
                                &global->monitor[count]->options.color);
            }
            if ((value = xfce_rc_read_entry (rc, "Text", NULL)) && *value)
            {
                if (global->monitor[count]->options.label_text)
                    g_free(global->monitor[count]->options.label_text);
                global->monitor[count]->options.label_text =
                    g_strdup(value);
            }
        }
        if (xfce_rc_has_group (rc, MONITOR_ROOT[3]))
        {
            xfce_rc_set_group (rc, MONITOR_ROOT[3]);
            
            global->uptime->enabled = 
                xfce_rc_read_bool_entry (rc, "Enabled", TRUE);
        }
    }

    xfce_rc_close (rc);
}

static void
monitor_write_config(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    char value[10];
    gint count;
    XfceRc *rc;
    char *file;

    if (!(file = xfce_panel_plugin_save_location (plugin, TRUE)))
        return;
    
    rc = xfce_rc_simple_open (file, FALSE);
    g_free (file);

    if (!rc)
        return;

    xfce_rc_set_group (rc, "Main");
    xfce_rc_write_int_entry (rc, "Timeout", global->timeout);
    xfce_rc_write_int_entry (rc, "Timeout_Seconds", global->timeout_seconds);
    xfce_rc_write_bool_entry (rc, "Use_Timeout_Seconds",
                              global->use_timeout_seconds);

    for(count = 0; count < 3; count++)
    {
        xfce_rc_set_group (rc, MONITOR_ROOT[count]);

        xfce_rc_write_bool_entry (rc, "Enabled", 
                global->monitor[count]->options.enabled);
        
        xfce_rc_write_bool_entry (rc, "Use_Label", 
                global->monitor[count]->options.use_label);

        g_snprintf(value, 8, "#%02X%02X%02X",
                   (guint)global->monitor[count]->options.color.red >> 8,
                   (guint)global->monitor[count]->options.color.green >> 8,
                   (guint)global->monitor[count]->options.color.blue >> 8);

        xfce_rc_write_entry (rc, "Color", value);

        xfce_rc_write_entry (rc, "Text", 
            global->monitor[count]->options.label_text ?
                global->monitor[count]->options.label_text : "");
    }

    xfce_rc_set_group (rc, MONITOR_ROOT[3]);

    xfce_rc_write_bool_entry (rc, "Enabled",
            global->uptime->enabled);

    xfce_rc_close (rc);
}

static gboolean
monitor_set_size(XfcePanelPlugin *plugin, int size, t_global_monitor *global)
{
    gint count;

    for(count = 0; count < 3; count++)
    {
        if (xfce_panel_plugin_get_orientation (plugin) == 
                GTK_ORIENTATION_HORIZONTAL)
        {
            gtk_widget_set_size_request(GTK_WIDGET(global->monitor[count]->status),
                                        BORDER, size - BORDER);
        }
        else
        {
            gtk_widget_set_size_request(GTK_WIDGET(global->monitor[count]->status),
                                        size - BORDER, BORDER);
        }
    }
    
    setup_monitor(global);

    return TRUE;
}


#ifdef HAS_PANEL_49
static void
monitor_set_mode (XfcePanelPlugin *plugin, XfcePanelPluginMode mode,
                  t_global_monitor *global)
{
  GtkOrientation panel_orientation = xfce_panel_plugin_get_orientation (plugin);
  GtkOrientation orientation = (mode == XFCE_PANEL_PLUGIN_MODE_VERTICAL) ?
    GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;

  monitor_update_orientation (plugin, panel_orientation, orientation, global);
  monitor_set_size (plugin, xfce_panel_plugin_get_size (plugin), global);
}


#else
static void
monitor_set_orientation (XfcePanelPlugin *plugin, GtkOrientation orientation,
                         t_global_monitor *global)
{
  monitor_update_orientation (plugin, orientation, GTK_ORIENTATION_HORIZONTAL, global);
  monitor_set_size (plugin, xfce_panel_plugin_get_size (plugin), global);
}
#endif


#ifdef HAVE_UPOWER_GLIB
static void
upower_changed_cb(UpClient *client, t_global_monitor *global)
{
    setup_timer(global);
}
#endif

static void
entry_changed_cb(GtkEntry *entry, t_global_monitor *global)
{
    gchar** charvar = (gchar**)g_object_get_data (G_OBJECT(entry), "charvar");
    g_free(*charvar);
    *charvar = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    setup_monitor(global);
}

static void
check_button_cb(GtkToggleButton *check_button, t_global_monitor *global)
{
    gboolean oldstate;
    gboolean* boolvar;
    gpointer sensitive_widget;
    boolvar = (gboolean*)g_object_get_data(G_OBJECT(check_button), "boolvar");
    sensitive_widget = g_object_get_data(G_OBJECT(check_button), "sensitive_widget");
    oldstate = *boolvar;
    *boolvar = gtk_toggle_button_get_active(check_button);
    if (sensitive_widget)
        gtk_widget_set_sensitive(GTK_WIDGET(sensitive_widget), *boolvar);
    if (oldstate != *boolvar)
        setup_monitor(global);
}

static void
color_set_cb(GtkColorButton *color_button, t_global_monitor *global)
{
    GdkColor* colorvar;
    colorvar = (GdkColor*)g_object_get_data(G_OBJECT(color_button), "colorvar");
    gtk_color_button_get_color(color_button, colorvar);
    setup_monitor(global);
}

static void
monitor_dialog_response (GtkWidget *dlg, int response, 
                         t_global_monitor *global)
{
    gtk_widget_destroy (dlg);
    xfce_panel_plugin_unblock_menu (global->plugin);
    monitor_write_config (global->plugin, global);
}

static void
change_timeout_cb(GtkSpinButton *spin, t_global_monitor *global)
{
    global->timeout = gtk_spin_button_get_value(spin) * 1000;

    setup_timer(global);
}

#ifdef HAVE_UPOWER_GLIB
static void
change_timeout_seconds_cb(GtkSpinButton *spin, t_global_monitor *global)
{
    global->timeout_seconds = gtk_spin_button_get_value(spin);

    setup_timer(global);
}
#endif

/* Create a new frame, optionally with a checkbox.
 * Set boolvar to NULL if you do not want a checkbox.
 * Returns the GtkTable inside the frame. */
static GtkTable* new_frame(t_global_monitor *global, GtkBox *content,
                           const gchar *title, guint rows, gboolean *boolvar)
{
    GtkWidget *frame, *table, *check, *label;
    table = gtk_table_new (rows, 2, FALSE);
    gtk_table_set_col_spacings (GTK_TABLE(table), 12);
    gtk_table_set_row_spacings (GTK_TABLE(table), 6);
    frame = xfce_gtk_frame_box_new_with_content (title, table);
    gtk_box_pack_start_defaults (content, frame);
    if (boolvar) {
        check = gtk_check_button_new();
        /* Move frame label into check button */
        label = gtk_frame_get_label_widget (GTK_FRAME(frame));
        g_object_ref (G_OBJECT(label));
        gtk_container_remove (GTK_CONTAINER(frame), label);
        gtk_container_add (GTK_CONTAINER(check), label);
        g_object_unref (G_OBJECT(label));
        /* Assign check to be the frame's label */
        gtk_frame_set_label_widget (GTK_FRAME(frame), check);
        /* Configure and set check button */
        g_object_set_data (G_OBJECT(check), "sensitive_widget", table);
        g_object_set_data (G_OBJECT(check), "boolvar", boolvar);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(check), *boolvar);
        check_button_cb (GTK_TOGGLE_BUTTON(check), global);
        g_signal_connect (G_OBJECT(check), "toggled",
                          G_CALLBACK(check_button_cb), global);
    }
    return GTK_TABLE(table);
}

/* Creates a check box if boolvar is non-null, or a label if it is null.
 * If it is a check box, it will control the sensitivity of target.
 * If it is a label, its mnemonic will point to target.
 * Returns the widget. */
static GtkWidget *new_label_or_check_button(t_global_monitor *global,
                                            const gchar *labeltext,
                                            gboolean *boolvar, GtkWidget *target)
{
    GtkWidget *label;
    if (boolvar) {
        label = gtk_check_button_new_with_mnemonic (labeltext);
        g_object_set_data (G_OBJECT(label), "sensitive_widget", target);
        g_object_set_data (G_OBJECT(label), "boolvar", boolvar);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(label), *boolvar);
        check_button_cb (GTK_TOGGLE_BUTTON(label), global);
        g_signal_connect (GTK_WIDGET(label), "toggled",
                          G_CALLBACK(check_button_cb), global);
    } else {
        label = gtk_label_new_with_mnemonic (labeltext);
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5f); \
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), target);
    }
    return label;
}

/* Adds an entry box to the table, optionally with a checkbox to enable it.
 * Set boolvar to NULL if you do not want a checkbox. */
static void new_entry(t_global_monitor *global, GtkTable *table, guint row,
                      const gchar *labeltext, gchar **charvar,
                      gboolean *boolvar)
{
    GtkWidget *label, *entry;
    entry = gtk_entry_new ();
    g_object_set_data (G_OBJECT(entry), "charvar", charvar);
    gtk_entry_set_text (GTK_ENTRY(entry), *charvar);
    g_signal_connect (G_OBJECT(entry), "changed",
                      G_CALLBACK(entry_changed_cb), global);
    label = new_label_or_check_button(global, labeltext, boolvar, entry);
    gtk_table_attach_defaults(table, label,  0, 1, row, row+1);
    gtk_table_attach_defaults(table, entry, 1, 2, row, row+1);
}

/* Adds a color button to the table, optionally with a checkbox to enable it.
 * Set boolvar to NULL if you do not want a checkbox. */
static void new_color_button(t_global_monitor *global, GtkTable *table, guint row,
                             const gchar *labeltext, GdkColor* colorvar,
                             gboolean *boolvar)
{
    GtkWidget *label, *button;
    button = gtk_color_button_new_with_color(colorvar);
    g_object_set_data(G_OBJECT(button), "colorvar", colorvar);
    g_signal_connect(G_OBJECT(button), "color-set",
                     G_CALLBACK (color_set_cb), global);
    label = new_label_or_check_button(global, labeltext, boolvar, button);
    gtk_table_attach_defaults(table, label,  0, 1, row, row+1);
    gtk_table_attach_defaults(table, button, 1, 2, row, row+1);
}

/* Adds a new spin button, optionally with a checkbox to enable it.
 * Set boolvar to NULL if you do not want a checkbox. */
static void new_spin_button(t_global_monitor *global, GtkTable *table, guint row,
                            const gchar *labeltext, const gchar *units,
                            gfloat value, gfloat min, gfloat max, gfloat step,
                            GCallback callback, gboolean* boolvar) {
    GtkWidget *label, *button, *box;
    /* Hbox for spin button + units */
    box = gtk_hbox_new(TRUE, 2);
    button = gtk_spin_button_new_with_range (min, max, step);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (button), value);
    g_signal_connect (G_OBJECT (button), "value-changed", callback, global);
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
    label = gtk_label_new (units);
    gtk_misc_set_alignment (GTK_MISC(label), 0, .5);
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    /* Label/check button */
    label = new_label_or_check_button(global, labeltext, boolvar, box);
    gtk_table_attach_defaults(table, label,  0, 1, row, row+1);
    gtk_table_attach_defaults(table, box, 1, 2, row, row+1);
}

static void
monitor_create_options(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    GtkWidget           *dlg;
    GtkBox              *content;
    GtkTable            *table;
    guint                count;
    t_monitor           *monitor;
    static const gchar *FRAME_TEXT[] = {
            N_ ("CPU monitor"),
            N_ ("Memory monitor"),
            N_ ("Swap monitor"),
            N_ ("Uptime monitor")
    };

    xfce_panel_plugin_block_menu (plugin);
    
    dlg = xfce_titled_dialog_new_with_buttons (_("System Load Monitor"), 
                     GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                               GTK_DIALOG_DESTROY_WITH_PARENT |
                                               GTK_DIALOG_NO_SEPARATOR,
                                               GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                               NULL);
    
    g_signal_connect (G_OBJECT (dlg), "response",
                      G_CALLBACK (monitor_dialog_response), global);

    gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name (GTK_WINDOW (dlg), "xfce4-settings");

    content = GTK_BOX(gtk_dialog_get_content_area (GTK_DIALOG(dlg)));

    table = new_frame(global, content, _("General"), 2, NULL);
    new_spin_button(global, table, 0,
            _("Update interval:"), _("s"),
            (gfloat)global->timeout/1000.0, 0.100, 10.000, .050,
            G_CALLBACK(change_timeout_cb), NULL);
#ifdef HAVE_UPOWER_GLIB
    new_spin_button(global, table, 1,
            _("Power-saving interval:"), _("s"),
            (gfloat)global->timeout_seconds, 1, 10, 1,
            G_CALLBACK(change_timeout_seconds_cb),
            &global->use_timeout_seconds);
#endif
    
    for(count = 0; count < 3; count++)
    {
        monitor = global->monitor[count];

        table = new_frame(global, content,
                          _(FRAME_TEXT[count]), 2, &monitor->options.enabled);

        new_entry(global, table, 0,
                  _("Text to display:"), &monitor->options.label_text,
                  &monitor->options.use_label);

        new_color_button(global, table, 1,
                         _("Bar color:"), &monitor->options.color, NULL);
    }

    /*uptime monitor options - start*/
    table = new_frame(global, content,
                      _(FRAME_TEXT[3]), 1, &global->uptime->enabled);
    /*uptime monitor options - end*/

    gtk_widget_show_all (dlg);
}

static void
monitor_show_about(XfcePanelPlugin *plugin, t_global_monitor *global)
{
   GdkPixbuf *icon;
   const gchar *auth[] = {
      "Riccardo Persichetti <riccardo.persichetti@tin.it>",
      "Florian Rivoal <frivoal@xfce.org>",
      "David Schneider <dnschneid@gmail.com>", NULL };
   icon = xfce_panel_pixbuf_from_source("utilities-system-monitor", NULL, 32);
   gtk_show_about_dialog(NULL,
      "logo", icon,
      "license", xfce_get_license_text (XFCE_LICENSE_TEXT_BSD),
      "version", PACKAGE_VERSION,
      "program-name", PACKAGE_NAME,
      "comments", _("Monitor CPU load, swap usage and memory footprint"),
      "website", "http://goodies.xfce.org/projects/panel-plugins/xfce4-systemload-plugin",
      "copyright", _("Copyright (c) 2003-2012\n"),
      "authors", auth, NULL);

   if(icon)
      g_object_unref(G_OBJECT(icon));
}

static void
systemload_construct (XfcePanelPlugin *plugin)
{
    t_global_monitor *global;
 
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
    
    global = monitor_control_new (plugin);

    monitor_read_config (plugin, global);
    
#ifdef HAS_PANEL_49
    monitor_set_mode (plugin,
                      xfce_panel_plugin_get_mode (plugin),
                      global);
#else
    monitor_set_orientation (plugin, 
                             xfce_panel_plugin_get_orientation (plugin),
                             global);
#endif

    setup_monitor (global);

    gtk_container_add (GTK_CONTAINER (plugin), global->ebox);

    update_monitors (global);

#ifdef HAVE_UPOWER_GLIB
    if (global->upower) {
        g_signal_connect (global->upower, "changed",
                          G_CALLBACK(upower_changed_cb), global);
    }
#endif
    
    g_signal_connect (plugin, "free-data", G_CALLBACK (monitor_free), global);

    g_signal_connect (plugin, "save", G_CALLBACK (monitor_write_config), 
                      global);

    g_signal_connect (plugin, "size-changed", G_CALLBACK (monitor_set_size),
                      global);

#ifdef HAS_PANEL_49
    g_signal_connect (plugin, "mode-changed",
                      G_CALLBACK (monitor_set_mode), global);
#else
    g_signal_connect (plugin, "orientation-changed", 
                      G_CALLBACK (monitor_set_orientation), global);
#endif

    xfce_panel_plugin_menu_show_configure (plugin);
    g_signal_connect (plugin, "configure-plugin", 
                      G_CALLBACK (monitor_create_options), global);

    xfce_panel_plugin_menu_show_about(plugin);
    g_signal_connect (plugin, "about", G_CALLBACK (monitor_show_about),
                       global);
}

XFCE_PANEL_PLUGIN_REGISTER (systemload_construct);

