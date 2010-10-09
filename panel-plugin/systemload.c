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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "cpu.h"
#include "memswap.h"
#include "uptime.h"

/* for xml: */
static gchar *MONITOR_ROOT[] = { "SL_Cpu", "SL_Mem", "SL_Swap", "SL_Uptime" };

static GtkTooltips *tooltips = NULL;

static gchar *DEFAULT_TEXT[] = { "cpu", "mem", "swap" };
static gchar *DEFAULT_COLOR[] = { "#0000c0", "#00c000", "#f0f000" };

#define UPDATE_TIMEOUT 250

#define MAX_LENGTH 10

#define BORDER 8

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

    gulong     history[4];
    gulong     value_read;

    t_monitor_options options;

    /*options*/
    GtkWidget *opt_enabled;
    GtkBox    *opt_vbox;
    GtkWidget *opt_entry;
    GtkBox    *opt_hbox;
    GtkWidget *opt_use_label;
    GtkWidget *opt_button;
    GtkWidget *opt_da;
} t_monitor;

typedef struct
{
    GtkWidget  *box;
    GtkWidget  *label_up;
    GtkWidget  *label_down;
    GtkWidget  *ebox;

    gulong     value_read;
    gboolean enabled;

    /*options*/
    GtkWidget *opt_enabled;
} t_uptime_monitor;

typedef struct
{
    XfcePanelPlugin   *plugin;
    GtkWidget         *ebox;
    GtkWidget         *box;
    guint             timeout_id;
    t_monitor         *monitor[3];
    t_uptime_monitor  *uptime;

    /* options dialog */
    GtkWidget  *opt_dialog;
} t_global_monitor;

static gint
update_monitors(t_global_monitor *global)
{

    gchar caption[128];
    gulong mem, swap, MTotal, MUsed, STotal, SUsed;
    gint count, days, hours, mins;

    global->monitor[0]->history[0] = read_cpuload();
    read_memswap(&mem, &swap, &MTotal, &MUsed, &STotal, &SUsed);
    global->monitor[1]->history[0] = mem;
    global->monitor[2]->history[0] = swap;
    global->uptime->value_read = read_uptime();

    for(count = 0; count < 3; count++)
    {
        if (global->monitor[count]->options.enabled)
        {
            if (global->monitor[count]->history[0] > 100)
                global->monitor[count]->history[0] = 100;
            else if (global->monitor[count]->history[0] < 0)
                global->monitor[count]->history[0] = 0;

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
        gtk_tooltips_set_tip(tooltips, GTK_WIDGET(global->monitor[0]->ebox),
                             caption, NULL);
    }

    if (global->monitor[1]->options.enabled)
    {
        g_snprintf(caption, sizeof(caption), _("Memory: %ldMB of %ldMB used"),
                   MUsed >> 10 , MTotal >> 10);
        gtk_tooltips_set_tip(tooltips, GTK_WIDGET(global->monitor[1]->ebox),
                             caption, NULL);
    }

    if (global->monitor[2]->options.enabled)
    {
        if (STotal)
            g_snprintf(caption, sizeof(caption), _("Swap: %ldMB of %ldMB used"),
                       SUsed >> 10, STotal >> 10);
        else
            g_snprintf(caption, sizeof(caption), _("No swap"));

        gtk_tooltips_set_tip(tooltips, GTK_WIDGET(global->monitor[2]->ebox),
                             caption, NULL);
    }

    if (global->uptime->enabled)
    {
        days = global->uptime->value_read / 86400;
        hours = (global->uptime->value_read / 3600) % 24;
        mins = (global->uptime->value_read / 60) % 60;
        g_snprintf(caption, sizeof(caption), _("%d days"), days);
        gtk_label_set_text(GTK_LABEL(global->uptime->label_up),
                           caption);
        g_snprintf(caption, sizeof(caption), "%d:%02d", hours, mins);
        gtk_label_set_text(GTK_LABEL(global->uptime->label_down),
                           caption);

        g_snprintf(caption, sizeof(caption), _("Uptime:"));
        gtk_tooltips_set_tip(tooltips, GTK_WIDGET(global->uptime->ebox),
                             caption, NULL);
    }
    return TRUE;
}

static void
monitor_set_orientation (XfcePanelPlugin *plugin, GtkOrientation orientation,
                         t_global_monitor *global)
{
    GtkRcStyle *rc;
    gint count;

    gtk_widget_hide(GTK_WIDGET(global->ebox));

    if (global->box)
        gtk_container_remove(GTK_CONTAINER(global->ebox), 
                             GTK_WIDGET(global->box));
    
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
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
        gtk_widget_show(global->monitor[count]->label);

        global->monitor[count]->status = GTK_WIDGET(gtk_progress_bar_new());

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
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

        gtk_widget_show(GTK_WIDGET(global->monitor[count]->box));

        global->monitor[count]->ebox = gtk_event_box_new();
        gtk_widget_show(global->monitor[count]->ebox);
        gtk_container_add(GTK_CONTAINER(global->monitor[count]->ebox),
                          GTK_WIDGET(global->monitor[count]->box));

        rc = gtk_widget_get_modifier_style(GTK_WIDGET(global->monitor[count]->status));
        if (!rc) {
            rc = gtk_rc_style_new();
        }
        if (rc) {
            rc->color_flags[GTK_STATE_PRELIGHT] |= GTK_RC_BG;
            rc->bg[GTK_STATE_PRELIGHT] =
                global->monitor[count]->options.color;
        }

        gtk_widget_modify_style(GTK_WIDGET(global->monitor[count]->status), rc);
        gtk_widget_show(GTK_WIDGET(global->monitor[count]->status));

        gtk_box_pack_start(GTK_BOX(global->monitor[count]->box),
                           GTK_WIDGET(global->monitor[count]->status),
                           FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(global->box),
                           GTK_WIDGET(global->monitor[count]->ebox),
                           FALSE, FALSE, 0);

    }

    global->uptime->ebox = gtk_event_box_new();
    gtk_widget_show(global->uptime->ebox);

    global->uptime->box = GTK_WIDGET(gtk_vbox_new(FALSE, 0));
    gtk_widget_show(GTK_WIDGET(global->uptime->box));

    gtk_container_add(GTK_CONTAINER(global->uptime->ebox),
                      GTK_WIDGET(global->uptime->box));

    global->uptime->label_up = gtk_label_new("");
    gtk_widget_show(global->uptime->label_up);
    gtk_box_pack_start(GTK_BOX(global->uptime->box),
                       GTK_WIDGET(global->uptime->label_up),
                       FALSE, FALSE, 0);
    global->uptime->label_down = gtk_label_new("");
    gtk_widget_show(global->uptime->label_down);
    gtk_box_pack_start(GTK_BOX(global->uptime->box),
                       GTK_WIDGET(global->uptime->label_down),
                       FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(global->box),
                       GTK_WIDGET(global->uptime->ebox),
                       FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(global->ebox), GTK_WIDGET(global->box));
    gtk_widget_show(GTK_WIDGET(global->ebox));

    update_monitors (global);
}

static t_global_monitor *
monitor_control_new(XfcePanelPlugin *plugin)
{
    int count;
    t_global_monitor *global;

    tooltips = gtk_tooltips_new ();
    g_object_ref (tooltips);
    gtk_object_sink (GTK_OBJECT (tooltips));
    
    global = g_new(t_global_monitor, 1);
    global->plugin = plugin;
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
    }
    
    global->uptime = g_new(t_uptime_monitor, 1);
    global->uptime->enabled = TRUE;
    
    return global;
}

static void
monitor_free(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    gint count;

    if (global->timeout_id)
        g_source_remove(global->timeout_id);

    for(count = 0; count < 3; count++)
    {
        if (global->monitor[count]->options.label_text)
            g_free(global->monitor[count]->options.label_text);
    }
    g_free(global);
}

void
setup_monitor(t_global_monitor *global)
{
    GtkRcStyle *rc;
    gint count;

    gtk_widget_hide(GTK_WIDGET(global->uptime->ebox));

    for(count = 0; count < 3; count++)
    {
        gtk_widget_hide(GTK_WIDGET(global->monitor[count]->ebox));
        gtk_widget_hide(global->monitor[count]->label);
        gtk_label_set_text(GTK_LABEL(global->monitor[count]->label),
                           global->monitor[count]->options.label_text);

        gtk_widget_hide(GTK_WIDGET(global->monitor[count]->status));
        rc = gtk_widget_get_modifier_style(GTK_WIDGET(global->monitor[count]->status));
        if (!rc) {
            rc = gtk_rc_style_new();
        }

        if (rc) {
            rc->color_flags[GTK_STATE_PRELIGHT] |= GTK_RC_BG;
            rc->bg[GTK_STATE_PRELIGHT] = global->monitor[count]->options.color;
        }

        gtk_widget_modify_style(GTK_WIDGET(global->monitor[count]->status), rc);

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
            gtk_container_set_border_width(GTK_CONTAINER(global->uptime->box), 2);
        }
        gtk_widget_show(GTK_WIDGET(global->uptime->ebox));
    }
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

static void
monitor_apply_options(t_global_monitor *global)
{
    gint count;

    for(count = 0; count < 3; count++)
    {
        if (global->monitor[count]->options.label_text)
            g_free(global->monitor[count]->options.label_text);

        global->monitor[count]->options.label_text =
            g_strdup(gtk_entry_get_text(GTK_ENTRY(global->monitor[count]->opt_entry)));
    }
    setup_monitor(global);
}

static void
label_changed(t_global_monitor *global, gint count)
{
    if (global->monitor[count]->options.label_text)
        g_free(global->monitor[count]->options.label_text);

    global->monitor[count]->options.label_text =
        g_strdup(gtk_entry_get_text(GTK_ENTRY(global->monitor[count]->opt_entry)));

    setup_monitor(global);
}

static void
label_changed_cb0(GtkWidget *button, t_global_monitor *global)
{
    label_changed(global, 0);
}

static void
label_changed_cb1(GtkWidget *button, t_global_monitor *global)
{
    label_changed(global, 1);
}

static void
label_changed_cb2(GtkWidget *entry, t_global_monitor *global)
{
    label_changed(global, 2);
}

static void
monitor_toggled(t_global_monitor *global, gint count)
{
    global->monitor[count]->options.enabled =
        !global->monitor[count]->options.enabled;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(global->monitor[count]->opt_enabled),
                                 global->monitor[count]->options.enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(global->monitor[count]->opt_vbox),
                             global->monitor[count]->options.enabled);

    setup_monitor(global);
}

static void
monitor_toggled_cb0(GtkWidget *check_button, t_global_monitor *global)
{
    monitor_toggled(global, 0);
}

static void
monitor_toggled_cb1(GtkWidget *check_button, t_global_monitor *global)
{
    monitor_toggled(global, 1);
}

static void
monitor_toggled_cb2(GtkWidget *check_button, t_global_monitor *global)
{
    monitor_toggled(global, 2);
}

static void
uptime_toggled_cb(GtkWidget *check_button, t_global_monitor *global)
{
    global->uptime->enabled = !global->uptime->enabled;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(global->uptime->opt_enabled),
                                 global->uptime->enabled);
    setup_monitor(global);
}

static void
label_toggled(t_global_monitor *global, gint count)
{
    global->monitor[count]->options.use_label =
        !global->monitor[count]->options.use_label;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(global->monitor[count]->opt_use_label),
                                 global->monitor[count]->options.use_label);
    gtk_widget_set_sensitive(GTK_WIDGET(global->monitor[count]->opt_entry),
                             global->monitor[count]->options.use_label);

    setup_monitor(global);
}

static void
label_toggled_cb0(GtkWidget *check_button, t_global_monitor *global)
{
    label_toggled(global, 0);
}

static void
label_toggled_cb1(GtkWidget *check_button, t_global_monitor *global)
{
    label_toggled(global, 1);
}

static void
label_toggled_cb2(GtkWidget *check_button, t_global_monitor *global)
{
    label_toggled(global, 2);
}

static gboolean
expose_event_cb(GtkWidget *widget, GdkEventExpose *event)
{
if (widget->window)
    {
        GtkStyle *style;

        style = gtk_widget_get_style(widget);

        gdk_draw_rectangle(widget->window,
                           style->bg_gc[GTK_STATE_NORMAL],
                           TRUE,
                           event->area.x, event->area.y,
                           event->area.width, event->area.height);
    }

  return TRUE;
}

static void
change_color(t_global_monitor *global, gint count)
{
    GtkWidget *dialog;
    GtkColorSelection *colorsel;
    gint response;

    dialog = gtk_color_selection_dialog_new(_("Select color"));
    gtk_window_set_transient_for(GTK_WINDOW(dialog),
                                 GTK_WINDOW(global->opt_dialog));
    colorsel =
        GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(dialog)->colorsel);
    gtk_color_selection_set_previous_color(colorsel,
                                           &global->monitor[count]->options.color);
    gtk_color_selection_set_current_color(colorsel,
                                          &global->monitor[count]->options.color);
    gtk_color_selection_set_has_palette(colorsel, TRUE);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK)
    {
        gtk_color_selection_get_current_color(colorsel,
                                              &global->monitor[count]->options.color);
        gtk_widget_modify_bg(global->monitor[count]->opt_da,
                             GTK_STATE_NORMAL,
                             &global->monitor[count]->options.color);
        setup_monitor(global);
    }

    gtk_widget_destroy(dialog);
}

static void
change_color_cb0(GtkWidget *button, t_global_monitor *global)
{
    change_color(global, 0);
}

static void
change_color_cb1(GtkWidget *button, t_global_monitor *global)
{
    change_color(global, 1);
}

static void
change_color_cb2(GtkWidget *button, t_global_monitor *global)
{
    change_color(global, 2);
}

static void
monitor_dialog_response (GtkWidget *dlg, int response, 
                         t_global_monitor *global)
{
    monitor_apply_options (global);
    gtk_widget_destroy (dlg);
    xfce_panel_plugin_unblock_menu (global->plugin);
    monitor_write_config (global->plugin, global);
}

static void
monitor_create_options(XfcePanelPlugin *plugin, t_global_monitor *global)
{
    GtkWidget           *dlg, *notebook;
    GtkWidget           *vbox, *dialog_vbox;
    GtkWidget           *hbox;
    GtkWidget           *color_label;
    GtkWidget           *align, *label;
    GtkSizeGroup        *sg;
    guint                count;
    static const gchar *FRAME_TEXT[] = {
	    N_ ("CPU monitor"),
	    N_ ("Memory monitor"),
	    N_ ("Swap monitor"),
	    N_ ("Uptime monitor")
    };

    xfce_panel_plugin_block_menu (plugin);
    
    dlg = xfce_titled_dialog_new_with_buttons (_("System Load Monitor"), NULL,
                                               GTK_DIALOG_DESTROY_WITH_PARENT |
                                               GTK_DIALOG_NO_SEPARATOR,
                                               GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                               NULL);
    
    global->opt_dialog = dlg;

    gtk_window_set_icon_name (GTK_WINDOW (dlg), "xfce4-settings");
    g_signal_connect (G_OBJECT (dlg), "response",
                      G_CALLBACK (monitor_dialog_response), global);

    gtk_container_set_border_width (GTK_CONTAINER (dlg), 2);
    
    dialog_vbox = GTK_DIALOG (dlg)->vbox;
                        
    notebook = gtk_notebook_new ();
    gtk_box_pack_start (GTK_BOX (dialog_vbox), notebook, FALSE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (notebook), BORDER-3);
    
    for(count = 0; count < 3; count++)
    {
        vbox = gtk_vbox_new(FALSE, BORDER);
        gtk_container_add (GTK_CONTAINER (notebook), vbox);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);

        global->monitor[count]->opt_enabled =
            gtk_check_button_new_with_mnemonic(_("Show monitor"));
        gtk_box_pack_start(GTK_BOX(vbox),
                           GTK_WIDGET(global->monitor[count]->opt_enabled),
                           FALSE, FALSE, 0);

        global->monitor[count]->opt_vbox = GTK_BOX(gtk_vbox_new(FALSE, 5));

        global->monitor[count]->opt_hbox = GTK_BOX(gtk_hbox_new(FALSE, 5));

        global->monitor[count]->opt_use_label =
            gtk_check_button_new_with_mnemonic(_("Text to display:"));
        gtk_box_pack_start(GTK_BOX(global->monitor[count]->opt_hbox),
                           GTK_WIDGET(global->monitor[count]->opt_use_label),
                           FALSE, FALSE, 0);

        global->monitor[count]->opt_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(global->monitor[count]->opt_entry),
                                 MAX_LENGTH);
        gtk_entry_set_text(GTK_ENTRY(global->monitor[count]->opt_entry),
                           global->monitor[count]->options.label_text);
        gtk_box_pack_start(GTK_BOX(global->monitor[count]->opt_hbox),
                           GTK_WIDGET(global->monitor[count]->opt_entry),
                           FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(global->monitor[count]->opt_vbox),
                           GTK_WIDGET(global->monitor[count]->opt_hbox),
                           FALSE, FALSE, 0);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(global->monitor[count]->opt_enabled),
                                     global->monitor[count]->options.enabled);
        gtk_widget_set_sensitive(GTK_WIDGET(global->monitor[count]->opt_vbox),
                                 global->monitor[count]->options.enabled);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(global->monitor[count]->opt_use_label),
                                     global->monitor[count]->options.use_label);
        gtk_widget_set_sensitive(GTK_WIDGET(global->monitor[count]->opt_entry),
                                 global->monitor[count]->options.use_label);

        hbox = gtk_hbox_new(FALSE, BORDER);
        gtk_box_pack_start(GTK_BOX(global->monitor[count]->opt_vbox),
                           hbox, FALSE, FALSE, 0);

        color_label = gtk_label_new(_("Bar color:"));
        gtk_misc_set_alignment(GTK_MISC(color_label), 0, 0.5);
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(color_label),
                           FALSE, FALSE, 0);

        global->monitor[count]->opt_button = gtk_button_new();
        global->monitor[count]->opt_da = gtk_drawing_area_new();

        gtk_widget_modify_bg(global->monitor[count]->opt_da, GTK_STATE_NORMAL,
                             &global->monitor[count]->options.color);
        gtk_widget_set_size_request(global->monitor[count]->opt_da, 64, 12);
        gtk_container_add(GTK_CONTAINER(global->monitor[count]->opt_button),
                          global->monitor[count]->opt_da);
        gtk_box_pack_start(GTK_BOX(hbox),
                           GTK_WIDGET(global->monitor[count]->opt_button),
                           FALSE, FALSE, 0);

        sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
        gtk_size_group_add_widget(sg, global->monitor[count]->opt_enabled);
        gtk_size_group_add_widget(sg, global->monitor[count]->opt_use_label);
        gtk_size_group_add_widget(sg, color_label);

        gtk_box_pack_start(GTK_BOX(vbox),
                           GTK_WIDGET(global->monitor[count]->opt_vbox),
                           FALSE, FALSE, 0);
        align = gtk_alignment_new(0, 0, 0, 0);
        gtk_widget_set_size_request(align, BORDER, BORDER);
        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(align), FALSE, FALSE, 0);

        label = gtk_label_new (_(FRAME_TEXT[count]));
        gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), count), label);
    }

    /*uptime monitor options - start*/
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_add (GTK_CONTAINER (notebook), vbox);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);

    global->uptime->opt_enabled =
        gtk_check_button_new_with_mnemonic(_("Show monitor"));
    gtk_widget_show(global->uptime->opt_enabled);
    gtk_box_pack_start(GTK_BOX(vbox),
                       GTK_WIDGET(global->uptime->opt_enabled),
                       FALSE, FALSE, 0);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(global->uptime->opt_enabled),
                                 global->uptime->enabled);

    label = gtk_label_new (_(FRAME_TEXT[3]));
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3), label);

    g_signal_connect(GTK_WIDGET(global->uptime->opt_enabled), "toggled",
                     G_CALLBACK(uptime_toggled_cb), global);
    /*uptime monitor options - end*/

    g_signal_connect(GTK_WIDGET(global->monitor[0]->opt_da), "expose_event",
                     G_CALLBACK(expose_event_cb), NULL);
    g_signal_connect(GTK_WIDGET(global->monitor[0]->opt_button), "clicked",
                     G_CALLBACK(change_color_cb0), global);
    g_signal_connect(GTK_WIDGET(global->monitor[0]->opt_use_label), "toggled",
                     G_CALLBACK(label_toggled_cb0), global);
    g_signal_connect(GTK_WIDGET(global->monitor[0]->opt_entry), "activate",
                     G_CALLBACK(label_changed_cb0), global);
    g_signal_connect(GTK_WIDGET(global->monitor[0]->opt_enabled), "toggled",
                     G_CALLBACK(monitor_toggled_cb0), global);

    g_signal_connect(GTK_WIDGET(global->monitor[1]->opt_da), "expose_event",
                     G_CALLBACK(expose_event_cb), NULL);
    g_signal_connect(GTK_WIDGET(global->monitor[1]->opt_button), "clicked",
                     G_CALLBACK(change_color_cb1), global);
    g_signal_connect(GTK_WIDGET(global->monitor[1]->opt_use_label), "toggled",
                     G_CALLBACK(label_toggled_cb1), global);
    g_signal_connect(GTK_WIDGET(global->monitor[1]->opt_entry), "activate",
                     G_CALLBACK(label_changed_cb1), global);
    g_signal_connect(GTK_WIDGET(global->monitor[1]->opt_enabled), "toggled",
                     G_CALLBACK(monitor_toggled_cb1), global);

    g_signal_connect(GTK_WIDGET(global->monitor[2]->opt_da), "expose_event",
                     G_CALLBACK(expose_event_cb), NULL);
    g_signal_connect(GTK_WIDGET(global->monitor[2]->opt_button), "clicked",
                     G_CALLBACK(change_color_cb2), global);
    g_signal_connect(GTK_WIDGET(global->monitor[2]->opt_use_label), "toggled",
                     G_CALLBACK(label_toggled_cb2), global);
    g_signal_connect(GTK_WIDGET(global->monitor[2]->opt_entry), "activate",
                     G_CALLBACK(label_changed_cb2), global);
    g_signal_connect(GTK_WIDGET(global->monitor[2]->opt_enabled), "toggled",
                     G_CALLBACK(monitor_toggled_cb2), global);

    gtk_widget_show_all (dlg);
}

static void
systemload_construct (XfcePanelPlugin *plugin)
{
    t_global_monitor *global;
 
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
    
    global = monitor_control_new (plugin);

    monitor_read_config (plugin, global);
    
    monitor_set_orientation (plugin, 
                             xfce_panel_plugin_get_orientation (plugin),
                             global);

    setup_monitor (global);

    gtk_container_add (GTK_CONTAINER (plugin), global->ebox);

    update_monitors (global);

    global->timeout_id = 
        g_timeout_add(UPDATE_TIMEOUT, (GSourceFunc)update_monitors, global);
    
    g_signal_connect (plugin, "free-data", G_CALLBACK (monitor_free), global);

    g_signal_connect (plugin, "save", G_CALLBACK (monitor_write_config), 
                      global);

    g_signal_connect (plugin, "size-changed", G_CALLBACK (monitor_set_size),
                      global);

    g_signal_connect (plugin, "orientation-changed", 
                      G_CALLBACK (monitor_set_orientation), global);

    xfce_panel_plugin_menu_show_configure (plugin);
    g_signal_connect (plugin, "configure-plugin", 
                      G_CALLBACK (monitor_create_options), global);
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (systemload_construct);

