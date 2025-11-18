/*
 * Copyright (C) 2010-2016 jeanfi@gmail.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include <pmutex.h>

#include "graph.h"
#include "ui_graph.h"

gboolean should_update_ui(struct ui_psensor *ui)
{
    GtkWindow *window = GTK_WINDOW(ui->main_window);
    
    // 1. 
    if (!window || !gtk_widget_get_visible(GTK_WIDGET(window))) {
        return FALSE;
    }
    
    // 2. 
    if (!gtk_window_is_active(window)) {
        return FALSE;
    }
    
    // 3. 
    GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
    if (gdk_window) {
        GdkWindowState state = gdk_window_get_state(gdk_window);
        if (state & GDK_WINDOW_STATE_ICONIFIED) {
            return FALSE;
        }
    }
    
    return TRUE;
}

static int
on_graph_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->type != GDK_BUTTON_PRESS
	   || event->button != 3)
	{
		return FALSE;
	}
	gtk_menu_popup_at_pointer(GTK_MENU(((struct ui_psensor *)data)->popup_menu),
	                         (const GdkEvent*)event);

	return TRUE;
}

static int draw_callback_lock(pthread_mutex_t *m) {
	return pmutex_lock(m);
}
static int draw_callback_unlock(pthread_mutex_t *m) {
	return pmutex_unlock(m);
}

gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
	struct ui_psensor *ui_psensor = (struct ui_psensor *)user_data;
	
	struct config *cfg = ui_psensor->config;

    MyWidgetData *widget_data = &cfg->widget_data;
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    if (!widget_data->cache_valid || 
        widget_data->last_width != allocation.width || 
        widget_data->last_height != allocation.height ||
	    cfg->is_new_data) {
        
        // 
        if (widget_data->cache_surface) {
            cairo_surface_destroy(widget_data->cache_surface);
        }
        
        // 
        widget_data->cache_surface = cairo_surface_create_similar(
            cairo_get_target(cr),
            CAIRO_CONTENT_COLOR_ALPHA,
            allocation.width,
            allocation.height
        );
		widget_data->last_width = allocation.width;
        widget_data->last_height = allocation.height;
        
        // 
        cairo_t *cache_cr = cairo_create(widget_data->cache_surface);
        
        // 
        draw_callback_lock(&ui_psensor->sensors_mutex);
        redraw_graph(widget_data->cache_surface,
			cache_cr,
			 (const struct psensor**)ui_psensor->sensors,
		     ui_get_graph_widget(),
		     ui_psensor->config,
		     ui_psensor->main_window);
		
		cfg->is_new_data = false;
		draw_callback_unlock(&ui_psensor->sensors_mutex);
		        
        cairo_destroy(cache_cr);
        widget_data->cache_valid = TRUE;
    }

    // 
    cairo_set_source_surface(cr, widget_data->cache_surface, 0, 0);
    cairo_paint(cr);
    
    return TRUE;
}

static void smooth_curves_enabled_changed_cbk(void *data)
{
	is_smooth_curves_enabled = config_is_smooth_curves_enabled();
}

void ui_graph_create(struct ui_psensor *sensor_context)
{
	log_debug("ui_graph_create()");

	GtkWidget *w_graph = ui_get_graph_widget();

	is_smooth_curves_enabled = config_is_smooth_curves_enabled();
	g_signal_connect_after(config_get_GSettings(),
			"changed::graph-smooth-curves-enabled",
			G_CALLBACK(smooth_curves_enabled_changed_cbk),
			NULL);

	g_signal_connect(GTK_WIDGET(w_graph),
			"draw",
			G_CALLBACK(draw_callback),
			sensor_context);

	gtk_widget_add_events(w_graph, GDK_BUTTON_PRESS_MASK);

	g_signal_connect(GTK_WIDGET(w_graph),
			"button_press_event",
			(GCallback) on_graph_clicked, sensor_context);
	
	log_debug("ui_graph_create() ends");
}
