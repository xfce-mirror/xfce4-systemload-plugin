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

#include <libxfce4util/i18n.h>
#include <libxfcegui4/dialogs.h>
#include <panel/plugins.h>
#include <panel/xfce.h>

#include "cpu.h"
#include "memswap.h"
#include "uptime.h"

/* for xml: */
static gchar *MONITOR_ROOT[] = { "SL_Cpu", "SL_Mem", "SL_Swap", "SL_Uptime" };

static GtkTooltips *tooltips = NULL;

extern xmlDocPtr xmlconfig;
#define MYDATA(node) xmlNodeListGetString(xmlconfig, node->children, 1)

static gchar *DEFAULT_TEXT[] = { "cpu", "mem", "swap" };
static gchar *DEFAULT_COLOR[] = { "#0000c0", "#00c000", "#f0f000" };

#define UPDATE_TIMEOUT 250

#define MAX_LENGTH 10

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

static t_global_monitor *
monitor_new(void)
{
    t_global_monitor *global;
    GtkRcStyle *rc;
    gint count;

    global = g_new(t_global_monitor, 1);
    global->timeout_id = 0;
    global->ebox = gtk_event_box_new();
    gtk_widget_show(global->ebox);
    global->box = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(global->box);

    if (!tooltips) {
        tooltips = gtk_tooltips_new();
    }

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

        global->monitor[count]->ebox = gtk_event_box_new();
        gtk_widget_show(global->monitor[count]->ebox);

        global->monitor[count]->box = GTK_WIDGET(gtk_hbox_new(FALSE, 0));
        gtk_container_set_border_width(GTK_CONTAINER(global->monitor[count]->box),
                                       border_width);
        gtk_widget_show(GTK_WIDGET(global->monitor[count]->box));

        gtk_container_add(GTK_CONTAINER(global->monitor[count]->ebox),
                          GTK_WIDGET(global->monitor[count]->box));

        global->monitor[count]->label =
            gtk_label_new(global->monitor[count]->options.label_text);
        gtk_widget_show(global->monitor[count]->label);
        gtk_box_pack_start(GTK_BOX(global->monitor[count]->box),
                           GTK_WIDGET(global->monitor[count]->label),
                           FALSE, FALSE, 0);

        global->monitor[count]->status = GTK_WIDGET(gtk_progress_bar_new());
        gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(global->monitor[count]->status),
                                         GTK_PROGRESS_BOTTOM_TO_TOP);

        rc = gtk_widget_get_modifier_style(GTK_WIDGET(global->monitor[count]->status));
        if (!rc) {
            rc = gtk_rc_style_new();
        }
        if (rc) {
            rc->color_flags[GTK_STATE_PRELIGHT] |= GTK_RC_BG;
            rc->bg[GTK_STATE_PRELIGHT] = global->monitor[count]->options.color;
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
    global->uptime = g_new(t_uptime_monitor, 1);
    global->uptime->enabled = TRUE;
    global->uptime->ebox = gtk_event_box_new();
    gtk_widget_show(global->uptime->ebox);

    global->uptime->box = GTK_WIDGET(gtk_vbox_new(FALSE, 0));
    gtk_container_set_border_width(GTK_CONTAINER(global->uptime->box),
                                   border_width);
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

    return global;
}

static void
monitor_set_orientation (Control * ctrl, int orientation)
{
    t_global_monitor *global = ctrl->data;
    GtkRcStyle *rc;
    gint count;

    if (global->timeout_id)
        g_source_remove(global->timeout_id);

    gtk_widget_hide(GTK_WIDGET(global->ebox));
    gtk_container_remove(GTK_CONTAINER(global->ebox), GTK_WIDGET(global->box));
    if (orientation == HORIZONTAL)
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

        if (orientation == HORIZONTAL)
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

        gtk_container_set_border_width(GTK_CONTAINER(global->monitor[count]->box),
                                       border_width);
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
    gtk_container_set_border_width(GTK_CONTAINER(global->uptime->box),
                                   border_width);
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

    global->timeout_id = g_timeout_add(UPDATE_TIMEOUT,
                                       (GtkFunction)update_monitors,
                                       global);
}

static gboolean
monitor_control_new(Control *ctrl)
{
    t_global_monitor *global;

    global = monitor_new();

    gtk_container_add(GTK_CONTAINER(ctrl->base), GTK_WIDGET(global->ebox));

    if (!global->timeout_id) {
        global->timeout_id = g_timeout_add(UPDATE_TIMEOUT,
                                           (GtkFunction)update_monitors,
                                           global);
    }

    ctrl->data = (gpointer)global;
    ctrl->with_popup = FALSE;

    gtk_widget_set_size_request(ctrl->base, -1, -1);

    return TRUE;
}

static void
monitor_free(Control *ctrl)
{
    t_global_monitor *global;
    gint count;

    g_return_if_fail(ctrl != NULL);
    g_return_if_fail(ctrl->data != NULL);

    global = (t_global_monitor *)ctrl->data;

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
        gtk_widget_show(GTK_WIDGET(global->uptime->ebox));
    }


}

static void
monitor_read_config(Control *ctrl, xmlNodePtr node)
{
    xmlChar *value;
    t_global_monitor *global;
    gint count;

    global = (t_global_monitor *)ctrl->data;
    
    if (node == NULL || node->children == NULL)
        return;

    for (node = node->children; node; node = node->next)
    {
        for(count = 0; count < 3; count++)
        {
            if (xmlStrEqual(node->name, (const xmlChar *)MONITOR_ROOT[count]))
            {
                if ((value = xmlGetProp(node, (const xmlChar *)"Enabled")))
                {
                    global->monitor[count]->options.enabled = atoi(value);
                    g_free(value);
                }
                if ((value = xmlGetProp(node, (const xmlChar *)"Use_Label")))
                {
                    global->monitor[count]->options.use_label = atoi(value);
                    g_free(value);
                }
                if ((value = xmlGetProp(node, (const xmlChar *)"Color")))
                {
                    gdk_color_parse(value,
                                    &global->monitor[count]->options.color);
                    g_free(value);
                }
                if ((value = xmlGetProp(node, (const xmlChar *) "Text")))
                {
                    if (global->monitor[count]->options.label_text)
                        g_free(global->monitor[count]->options.label_text);
                    global->monitor[count]->options.label_text =
                        g_strdup((gchar *)value);
                    g_free(value);
                }
                break;
            }
        }
        if (xmlStrEqual(node->name, (const xmlChar *)MONITOR_ROOT[3]))
        {
            if ((value = xmlGetProp(node, (const xmlChar *)"Enabled")))
            {
                global->uptime->enabled = atoi(value);
                g_free(value);
            }
        }
    }
    setup_monitor(global);
}

static void
monitor_write_config(Control *ctrl, xmlNodePtr parent)
{
    xmlNodePtr root;
    char value[10];
    t_global_monitor *global;
    gint count;

    global = (t_global_monitor *)ctrl->data;

    for(count = 0; count < 3; count++)
    {
        root = xmlNewTextChild(parent, NULL, MONITOR_ROOT[count], NULL);

        g_snprintf(value, 2, "%d", global->monitor[count]->options.enabled);
        xmlSetProp(root, "Enabled", value);

        g_snprintf(value, 2, "%d", global->monitor[count]->options.use_label);
        xmlSetProp(root, "Use_Label", value);

        g_snprintf(value, 8, "#%02X%02X%02X",
                   (guint)global->monitor[count]->options.color.red >> 8,
                   (guint)global->monitor[count]->options.color.green >> 8,
                   (guint)global->monitor[count]->options.color.blue >> 8);

        xmlSetProp(root, "Color", value);

        if (global->monitor[count]->options.label_text) {
            xmlSetProp(root, "Text",
                       global->monitor[count]->options.label_text);
        }
        else {
            xmlSetProp(root, "Text", DEFAULT_TEXT[count]);
        }
    }

    root = xmlNewTextChild(parent, NULL, MONITOR_ROOT[3], NULL);

    g_snprintf(value, 2, "%d", global->uptime->enabled);
    xmlSetProp(root, "Enabled", value);

}

static void
monitor_attach_callback(Control *ctrl, const gchar *signal, GCallback cb,
    gpointer data)
{
    t_global_monitor *global;

    global = (t_global_monitor *)ctrl->data;
    g_signal_connect(global->ebox, signal, cb, data);
}

static void
monitor_set_size(Control *ctrl, int size)
{
    /* do the resize */
    t_global_monitor *global;
    gint count;

    global = (t_global_monitor *)ctrl->data;

    for(count = 0; count < 3; count++)
    {
        if (settings.orientation == HORIZONTAL)
        {
            gtk_widget_set_size_request(GTK_WIDGET(global->monitor[count]->status),
                                        6 + 2 * size, icon_size[size]);
        }
        else
        {
            gtk_widget_set_size_request(GTK_WIDGET(global->monitor[count]->status),
                                        icon_size[size], 6 + 2 * size);
        }
        gtk_widget_queue_resize(GTK_WIDGET(global->monitor[count]->status));
    }
    setup_monitor(global);
}

static void
monitor_apply_options_cb(GtkWidget *button, t_global_monitor *global)
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
monitor_create_options(Control *control, GtkContainer *container, GtkWidget *done)
{
    t_global_monitor *global;
    GtkBox           *vbox, *global_vbox;
    GtkBox           *hbox;
    GtkWidget        *color_label;
    GtkWidget        *align;
    GtkWidget        *frame;
    GtkSizeGroup     *sg;
    gint             count;
    static gchar     *FRAME_TEXT[] = {
	    N_("CPU monitor"),
	    N_("Memory monitor"),
	    N_("Swap monitor"),
	    N_("Uptime monitor")
    };

    global = (t_global_monitor *)control->data;
    global->opt_dialog = gtk_widget_get_toplevel(done);

    global_vbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
    gtk_widget_show(GTK_WIDGET(global_vbox));
    gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(global_vbox));

    for(count = 0; count < 3; count++)
    {
        frame = xfce_framebox_new(_(FRAME_TEXT[count]), TRUE);
        gtk_widget_show(GTK_WIDGET(frame));

        vbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
        gtk_widget_show(GTK_WIDGET(vbox));

        global->monitor[count]->opt_enabled =
            gtk_check_button_new_with_mnemonic(_("Show monitor"));
        gtk_widget_show(global->monitor[count]->opt_enabled);
        gtk_box_pack_start(GTK_BOX(vbox),
                           GTK_WIDGET(global->monitor[count]->opt_enabled),
                           FALSE, FALSE, 0);

        global->monitor[count]->opt_vbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
        gtk_widget_show(GTK_WIDGET(global->monitor[count]->opt_vbox));

        global->monitor[count]->opt_hbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        gtk_widget_show(GTK_WIDGET(global->monitor[count]->opt_hbox));

        global->monitor[count]->opt_use_label =
            gtk_check_button_new_with_mnemonic(_("Text to display:"));
        gtk_widget_show(global->monitor[count]->opt_use_label);
        gtk_box_pack_start(GTK_BOX(global->monitor[count]->opt_hbox),
                           GTK_WIDGET(global->monitor[count]->opt_use_label),
                           FALSE, FALSE, 0);

        global->monitor[count]->opt_entry = gtk_entry_new();
        gtk_entry_set_max_length(GTK_ENTRY(global->monitor[count]->opt_entry),
                                 MAX_LENGTH);
        gtk_entry_set_text(GTK_ENTRY(global->monitor[count]->opt_entry),
                           global->monitor[count]->options.label_text);
        gtk_widget_show(global->monitor[count]->opt_entry);
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

        hbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
        gtk_widget_show(GTK_WIDGET(hbox));
        gtk_box_pack_start(GTK_BOX(global->monitor[count]->opt_vbox),
                           GTK_WIDGET(hbox), FALSE, FALSE, 0);

        color_label = gtk_label_new(_("Bar color:"));
        gtk_misc_set_alignment(GTK_MISC(color_label), 0, 0.5);
        gtk_widget_show(GTK_WIDGET(color_label));
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(color_label),
                           FALSE, FALSE, 0);

        global->monitor[count]->opt_button = gtk_button_new();
        global->monitor[count]->opt_da = gtk_drawing_area_new();

        gtk_widget_modify_bg(global->monitor[count]->opt_da, GTK_STATE_NORMAL,
                             &global->monitor[count]->options.color);
        gtk_widget_set_size_request(global->monitor[count]->opt_da, 64, 12);
        gtk_container_add(GTK_CONTAINER(global->monitor[count]->opt_button),
                          global->monitor[count]->opt_da);
        gtk_widget_show(GTK_WIDGET(global->monitor[count]->opt_button));
        gtk_widget_show(GTK_WIDGET(global->monitor[count]->opt_da));
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
        gtk_widget_set_size_request(align, 5, 5);
        gtk_widget_show(GTK_WIDGET(align));
        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(align), FALSE, FALSE, 0);

        xfce_framebox_add (XFCE_FRAMEBOX(frame), GTK_WIDGET(vbox));
        gtk_box_pack_start(GTK_BOX(global_vbox),
                           GTK_WIDGET(frame),
                           FALSE, FALSE, 0);
    }

    /*uptime monitor options - start*/
    frame = xfce_framebox_new(_(FRAME_TEXT[3]), TRUE);
    gtk_widget_show(GTK_WIDGET(frame));

    vbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
    gtk_widget_show(GTK_WIDGET(vbox));

    global->uptime->opt_enabled =
        gtk_check_button_new_with_mnemonic(_("Show monitor"));
    gtk_widget_show(global->uptime->opt_enabled);
    gtk_box_pack_start(GTK_BOX(vbox),
                       GTK_WIDGET(global->uptime->opt_enabled),
                       FALSE, FALSE, 0);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(global->uptime->opt_enabled),
                                 global->uptime->enabled);

    xfce_framebox_add (XFCE_FRAMEBOX(frame), GTK_WIDGET(vbox));
    gtk_box_pack_start(GTK_BOX(global_vbox),
                       GTK_WIDGET(frame),
                       FALSE, FALSE, 0);

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

    g_signal_connect(GTK_WIDGET(done), "clicked",
                     G_CALLBACK(monitor_apply_options_cb), global);

}

G_MODULE_EXPORT void
xfce_control_class_init(ControlClass *cc)
{
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
    
    cc->name            = "system load";
    cc->caption         = _("System Load");

    cc->create_control  = (CreateControlFunc)monitor_control_new;

    cc->free            = monitor_free;
    cc->read_config     = monitor_read_config;
    cc->write_config    = monitor_write_config;
    cc->attach_callback = monitor_attach_callback;

    cc->create_options  = monitor_create_options;

    cc->set_size        = monitor_set_size;

    cc->set_orientation = monitor_set_orientation;
}

XFCE_PLUGIN_CHECK_INIT
