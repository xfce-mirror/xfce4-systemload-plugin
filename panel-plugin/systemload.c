/*
 * Copyright (c) 2003 Riccardo Persichetti <ricpersi@libero.it>
 * Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
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

/* for xml: */
#define SLOAD_ROOT "SystemLoad"

static GtkTooltips *tooltips = NULL;

extern xmlDocPtr xmlconfig;
#define MYDATA(node) xmlNodeListGetString(xmlconfig, node->children, 1)

#define DEFAULTTEXT    "cpu"
#define DEFAULTCOLOR   "#0000c0"
#define UPDATE_TIMEOUT 250

#define MAX_LENGTH 10

typedef struct
{
    gboolean use_label;
    GdkColor color;
    gchar    *label_text;
} t_sload_options;

typedef struct
{
    GtkWidget       *ebox;
    GtkBox          *hbox;
    GtkWidget       *label;
    GtkProgressBar  *status;

    guint            timeout_id;
    gulong           cpu_history[4];
    gulong           cpu_used;

    t_sload_options options;

    /* options dialog */
    GtkWidget *opt_dialog;
    GtkWidget *opt_entry;
    GtkBox    *opt_hbox;
    GtkWidget *opt_use_label;
    GtkWidget *opt_da;

} t_sload;


static gint
update_cpu(t_sload *sload)
{

    gchar caption[128];

    sload->cpu_history[0] = read_cpuload();
    if (sload->cpu_history[0] > 100)
        sload->cpu_history[0] = 100;
    else if (sload->cpu_history[0] < 0)
        sload->cpu_history[0] = 0;

    sload->cpu_used = (sload->cpu_history[0] + sload->cpu_history[1] +
                       sload->cpu_history[2] + sload->cpu_history[3]) / 4;

    sload->cpu_history[3] = sload->cpu_history[2];
    sload->cpu_history[2] = sload->cpu_history[1];
    sload->cpu_history[1] = sload->cpu_history[0];

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sload->status),
                                  sload->cpu_used / 100.0);

    g_snprintf(caption, sizeof(caption), _("System Load: %ld%%"),
               sload->cpu_used);

    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(sload->ebox), caption, NULL);

    return TRUE;
}

static t_sload *
sload_new(void)
{
    t_sload *sload;
    GtkRcStyle *rc;

    if (!tooltips) {
        tooltips = gtk_tooltips_new();
    }

    sload = g_new(t_sload, 1);

    sload->options.use_label = TRUE;
    sload->options.label_text = g_strdup(DEFAULTTEXT);
    gdk_color_parse(DEFAULTCOLOR, &sload->options.color);
    sload->timeout_id = 0;

    sload->cpu_history[0] = 0;
    sload->cpu_history[1] = 0;
    sload->cpu_history[2] = 0;
    sload->cpu_history[3] = 0;

    sload->ebox = gtk_event_box_new();
    gtk_widget_show(sload->ebox);

    sload->hbox = GTK_BOX(gtk_hbox_new(FALSE, 0));
    gtk_widget_set_name(GTK_WIDGET(sload->hbox), "system_load");
    gtk_container_set_border_width(GTK_CONTAINER(sload->hbox), border_width);

    gtk_widget_show(GTK_WIDGET(sload->hbox));

    sload->label = gtk_label_new(sload->options.label_text);

    gtk_widget_show(sload->label);

    gtk_box_pack_start(GTK_BOX(sload->hbox), GTK_WIDGET(sload->label),
                       FALSE, FALSE, 0);

    sload->status = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_orientation(sload->status, GTK_PROGRESS_BOTTOM_TO_TOP);

    rc = gtk_widget_get_modifier_style(GTK_WIDGET(sload->status));
    if (!rc) {
        rc = gtk_rc_style_new();
    }

    if (rc) {
        rc->color_flags[GTK_STATE_PRELIGHT] |= GTK_RC_BG;
        rc->bg[GTK_STATE_PRELIGHT] = sload->options.color;
    }

    gtk_widget_modify_style(GTK_WIDGET(sload->status), rc);
    gtk_widget_show(GTK_WIDGET(sload->status));

    gtk_box_pack_start(GTK_BOX(sload->hbox), GTK_WIDGET(sload->status),
                       FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(sload->ebox), GTK_WIDGET(sload->hbox));
    
    return sload;
}

static gboolean
sload_control_new(Control *ctrl)
{
    t_sload *sload;

    sload = sload_new();

    gtk_container_add(GTK_CONTAINER(ctrl->base), GTK_WIDGET(sload->ebox));

    if (!sload->timeout_id) {
        sload->timeout_id = g_timeout_add(UPDATE_TIMEOUT,
                                          (GtkFunction)update_cpu, sload);
    }

    ctrl->data = (gpointer)sload;
    ctrl->with_popup = FALSE;

    gtk_widget_set_size_request(ctrl->base, -1, -1);

    return TRUE;
}

static void
sload_free(Control *ctrl)
{
    t_sload *sload;

    g_return_if_fail(ctrl != NULL);
    g_return_if_fail(ctrl->data != NULL);

    sload = (t_sload *)ctrl->data;

    if (sload->timeout_id)
        g_source_remove(sload->timeout_id);

    if (sload->options.label_text)
        g_free(sload->options.label_text);
    
    g_free(sload);
}

void
setup_sload(t_sload *sload)
{
    GtkRcStyle *rc;

    gtk_widget_hide(sload->label);
    gtk_label_set_text(GTK_LABEL(sload->label), sload->options.label_text);
    if (sload->options.use_label)
    {
        gtk_widget_show(sload->label);
    }

    gtk_widget_hide(GTK_WIDGET(sload->status));
    rc = gtk_widget_get_modifier_style(GTK_WIDGET(sload->status));
    if (!rc) {
        rc = gtk_rc_style_new();
    }

    if (rc) {
        rc->color_flags[GTK_STATE_PRELIGHT] |= GTK_RC_BG;
        rc->bg[GTK_STATE_PRELIGHT] = sload->options.color;
    }

    gtk_widget_modify_style(GTK_WIDGET(sload->status), rc);
    gtk_widget_show(GTK_WIDGET(sload->status));

}

static void
sload_apply_options_cb(GtkWidget *button, t_sload *sload)
{
    if (sload->options.label_text)
        g_free(sload->options.label_text);

    sload->options.label_text =
        g_strdup(gtk_entry_get_text(GTK_ENTRY(sload->opt_entry)));

    setup_sload(sload);
}    

static void
sload_read_config(Control *ctrl, xmlNodePtr node)
{
    xmlChar *value;
    t_sload *sload;
    
    sload = (t_sload *)ctrl->data;
    
    if (node == NULL || node->children == NULL)
        return;

    node = node->children;

    if (!xmlStrEqual(node->name, (const xmlChar *)SLOAD_ROOT))
        return;

    if ((value = xmlGetProp(node, (const xmlChar *)"Use_Label")))
    {
        sload->options.use_label = atoi(value);
        g_free(value);
    }

    if ((value = xmlGetProp(node, (const xmlChar *)"Color")))
    {
        gdk_color_parse(value, &sload->options.color);
        g_free(value);
    }

    if ((value = xmlGetProp(node, (const xmlChar *) "Text")))
    {
        if (sload->options.label_text)
            g_free(sload->options.label_text);
        sload->options.label_text = g_strdup((gchar *)value);
        g_free(value);
    }

    setup_sload(sload);
}

static void
sload_write_config(Control *ctrl, xmlNodePtr parent)
{
    xmlNodePtr root;
    char value[10];
    t_sload *sload;

    sload = (t_sload *)ctrl->data;
    
    root = xmlNewTextChild(parent, NULL, SLOAD_ROOT, NULL);

    g_snprintf(value, 2, "%d", sload->options.use_label);
    xmlSetProp(root, "Use_Label", value);

    g_snprintf(value, 8, "#%02X%02X%02X",
               (guint)sload->options.color.red >> 8,
               (guint)sload->options.color.green >> 8,
               (guint)sload->options.color.blue >> 8);
    xmlSetProp(root, "Color", value);

    if (sload->options.label_text) {
        xmlSetProp(root, "Text", sload->options.label_text);
    }
    else {
        xmlSetProp(root, "Text", DEFAULTTEXT);
    }
}

static void
sload_attach_callback(Control *ctrl, const gchar *signal, GCallback cb,
    gpointer data)
{
    t_sload *sload;

    sload = (t_sload *)ctrl->data;
    g_signal_connect(sload->ebox, signal, cb, data);
}

static void
sload_set_size(Control *ctrl, int size)
{
    /* do the resize */
    t_sload *sload = (t_sload *)ctrl->data;
    
    gtk_widget_set_size_request(GTK_WIDGET(sload->status), 6 + 2 * size,
                                icon_size[size]);

    gtk_widget_queue_resize(GTK_WIDGET(sload->status));
}

static void
label_changed_cb(GtkWidget *entry, t_sload *sload)
{
    if (sload->options.label_text)
        g_free(sload->options.label_text);

    sload->options.label_text =
        g_strdup(gtk_entry_get_text(GTK_ENTRY(sload->opt_entry)));

    setup_sload(sload);
}

static void
use_label_toggled_cb(GtkWidget *check_button, t_sload *sload)
{
    sload->options.use_label = !sload->options.use_label;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sload->opt_use_label),
                                 !sload->options.use_label);
    gtk_widget_set_sensitive(GTK_WIDGET(sload->opt_hbox),
                             sload->options.use_label);

    setup_sload(sload);
}

static gboolean
expose_event_cb(GtkWidget *widget, GdkEventExpose *event, gpointer data)
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
change_color_cb(GtkWidget *button, t_sload *sload)
{
  GtkWidget *dialog;
  GtkColorSelection *colorsel;
  gint response;
  
  dialog = gtk_color_selection_dialog_new("Select color");

  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(sload->opt_dialog));
  
  colorsel = GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(dialog)->colorsel);

  gtk_color_selection_set_previous_color(colorsel, &sload->options.color);
  gtk_color_selection_set_current_color(colorsel, &sload->options.color);
  gtk_color_selection_set_has_palette(colorsel, TRUE);
  
  response = gtk_dialog_run(GTK_DIALOG(dialog));

  if (response == GTK_RESPONSE_OK)
  {
      gtk_color_selection_get_current_color(colorsel, &sload->options.color);
      gtk_widget_modify_bg(sload->opt_da, GTK_STATE_NORMAL, &sload->options.color);
      setup_sload(sload);
  }
  
  gtk_widget_destroy(dialog);
}

static void
sload_add_options(Control *control, GtkContainer *container, GtkWidget *done)
{
    t_sload         *sload;
    GtkBox          *vbox;
    GtkWidget       *text_label;
    GtkBox          *hbox;
    GtkWidget       *color_label;
    GtkWidget       *button;
    GtkWidget       *align;
    GtkSizeGroup    *sg;

    sload = (t_sload *)control->data;

    sload->opt_dialog = gtk_widget_get_toplevel(done);

    vbox = GTK_BOX(gtk_vbox_new(FALSE, 5));
    gtk_widget_show(GTK_WIDGET(vbox));
    gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(vbox));

    sload->opt_hbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
    gtk_widget_show(GTK_WIDGET(sload->opt_hbox));
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(sload->opt_hbox), FALSE,
                       FALSE, 0);

    text_label = gtk_label_new(_("Text to display:"));
    gtk_misc_set_alignment(GTK_MISC(text_label), 0, 0.5);
    gtk_widget_show(GTK_WIDGET(text_label));
    gtk_box_pack_start(GTK_BOX(sload->opt_hbox), GTK_WIDGET(text_label),
                       FALSE, FALSE, 0);

    sload->opt_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(sload->opt_entry), MAX_LENGTH);
    gtk_entry_set_text(GTK_ENTRY(sload->opt_entry), sload->options.label_text);
    gtk_widget_show(sload->opt_entry);
    gtk_box_pack_start(GTK_BOX(sload->opt_hbox), GTK_WIDGET(sload->opt_entry),
                       FALSE, FALSE, 0);
    
    sload->opt_use_label = gtk_check_button_new_with_mnemonic(_("Hide text"));
    gtk_widget_show(sload->opt_use_label);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(sload->opt_use_label),
                       FALSE, FALSE, 0);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sload->opt_use_label),
                                 !sload->options.use_label);
    gtk_widget_set_sensitive(GTK_WIDGET(sload->opt_hbox),
                             sload->options.use_label);

    align = gtk_alignment_new(0, 0, 0, 0);
    gtk_widget_set_size_request(align, 8, 8);
    gtk_widget_show(GTK_WIDGET(align));
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(align), FALSE, FALSE, 0);

    hbox = GTK_BOX(gtk_hbox_new(FALSE, 5));
    gtk_widget_show(GTK_WIDGET(hbox));
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, FALSE, 0);

    color_label = gtk_label_new(_("Bar color:"));
    gtk_misc_set_alignment(GTK_MISC(color_label), 0, 0.5);
    gtk_widget_show(GTK_WIDGET(color_label));
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(color_label), FALSE, FALSE, 0);

    button = gtk_button_new();
    sload->opt_da = gtk_drawing_area_new();
    gtk_widget_modify_bg(sload->opt_da, GTK_STATE_NORMAL,
                         &sload->options.color);
    gtk_widget_set_size_request(sload->opt_da, 64, 12);
    gtk_container_add(GTK_CONTAINER(button), sload->opt_da);
    gtk_widget_show(GTK_WIDGET(button));
    gtk_widget_show(GTK_WIDGET(sload->opt_da));
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(button), FALSE, FALSE, 0);

    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    gtk_size_group_add_widget(sg, text_label);
    gtk_size_group_add_widget(sg, color_label);

    g_signal_connect(GTK_WIDGET(sload->opt_da), "expose_event",
		     G_CALLBACK(expose_event_cb), sload);
    g_signal_connect(GTK_WIDGET(button), "clicked",
                     G_CALLBACK(change_color_cb), sload);
    g_signal_connect(GTK_WIDGET(sload->opt_use_label), "toggled",
		     G_CALLBACK(use_label_toggled_cb), sload);
    g_signal_connect(GTK_WIDGET(sload->opt_entry), "activate",
                     G_CALLBACK(label_changed_cb), sload);
    g_signal_connect(GTK_WIDGET(done), "clicked",
                     G_CALLBACK(sload_apply_options_cb), sload);
}

G_MODULE_EXPORT void
xfce_control_class_init(ControlClass *cc)
{
    cc->name            = "system load";
    cc->caption         = _("System Load");

    cc->create_control  = (CreateControlFunc)sload_control_new;

    cc->free            = sload_free;
    cc->read_config     = sload_read_config;
    cc->write_config    = sload_write_config;
    cc->attach_callback = sload_attach_callback;

    cc->add_options     = sload_add_options;

    cc->set_size        = sload_set_size;
}

XFCE_PLUGIN_CHECK_INIT
