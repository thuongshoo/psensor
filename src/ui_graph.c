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

// === HELPER FUNCTIONS ===
static int draw_callback_lock(pthread_mutex_t *m) {
    return pmutex_lock(m);
}
static int draw_callback_unlock(pthread_mutex_t *m) {
    return pmutex_unlock(m);
}

static void clear_surface_to_transparent(cairo_surface_t *surface) {
    cairo_t *cr = cairo_create(surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_destroy(cr);
}

static void ensure_graph_surface(MyWidgetData *wd, cairo_t *cr, int w, int h, gboolean size_changed) {
    if (wd->graph_surface && !size_changed) return;
    
    if (wd->graph_surface) {
        cairo_surface_destroy(wd->graph_surface);
    }
    
    wd->graph_surface = cairo_surface_create_similar(
        cairo_get_target(cr), CAIRO_CONTENT_COLOR_ALPHA, w, h);
    clear_surface_to_transparent(wd->graph_surface);
    
    wd->last_width = w;
    wd->last_height = h;
    wd->cache_valid = FALSE;
}

static void init_last_values(struct ui_psensor *ui, MyWidgetData *wd) {
    if (wd->last_values_initialized) return;
    
    draw_callback_lock(&ui->sensors_mutex);
    
    const Psensor **enabled = list_filter_graph_enabled((const Psensor* const*)ui->sensors);
    if (!enabled) {
        wd->last_values_initialized = TRUE;
        draw_callback_unlock(&ui->sensors_mutex);
        return;
    }
    
    size_t count = 0;
    while (enabled[count]) count++;
    
    wd->last_values = (double*)calloc(count + 1, sizeof(double));
    if (wd->last_values) {
        wd->last_sensors_count = count;
        for (size_t i = 0; i < count; i++) {
            wd->last_values[i] = UNKNOWN_DOUBLE_VALUE;
        }
    }
    free((void*)enabled);
    
    wd->last_values_initialized = TRUE;
    draw_callback_unlock(&ui->sensors_mutex);
}

static void ensure_background_surface(struct ui_psensor *ui, MyWidgetData *wd, 
                                       cairo_t *cr, int w, int h, gboolean size_changed) {
    if (!size_changed && wd->background_valid) return;
    
    if (wd->background_surface) {
        cairo_surface_destroy(wd->background_surface);
    }
    
    wd->background_surface = cairo_surface_create_similar(
        cairo_get_target(cr), CAIRO_CONTENT_COLOR_ALPHA, w, h);
    
    cairo_t *bg_cr = cairo_create(wd->background_surface);
    draw_callback_lock(&ui->sensors_mutex);
    redraw_background_only(wd->background_surface, bg_cr,
                          (const Psensor*const*)ui->sensors,
                          ui_get_graph_widget(), ui->config, ui->main_window);
    draw_callback_unlock(&ui->sensors_mutex);
    cairo_destroy(bg_cr);
    
    wd->background_valid = TRUE;
}

static void redraw_graph_full(struct ui_psensor *ui, MyWidgetData *wd) {
    cairo_t *graph_cr = cairo_create(wd->graph_surface);
    
    cairo_set_operator(graph_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(graph_cr);
    cairo_set_operator(graph_cr, CAIRO_OPERATOR_OVER);
    
    draw_callback_lock(&ui->sensors_mutex);
    redraw_curves_only(wd->graph_surface, graph_cr,
                      (const Psensor*const*)ui->sensors,
                      ui_get_graph_widget(), ui->config, ui->main_window);
    draw_callback_unlock(&ui->sensors_mutex);
    
    cairo_destroy(graph_cr);
    wd->cache_valid = TRUE;
    wd->shift_count = 0;
}

static void update_last_values(struct ui_psensor *ui, MyWidgetData *wd) {
    if (!wd->last_values) return;
    
    draw_callback_lock(&ui->sensors_mutex);
    const Psensor **enabled = list_filter_graph_enabled((const Psensor* const *)ui->sensors);
    
    if (enabled) {
        for (size_t i = 0; i < wd->last_sensors_count && enabled[i]; i++) {
            const Psensor *s = enabled[i];
            wd->last_values[i] = (s->measures_count > 0) 
                ? s->measures[s->measures_count - 1].value 
                : UNKNOWN_DOUBLE_VALUE;
        }
        free((void*)enabled);
    }
    draw_callback_unlock(&ui->sensors_mutex);
}

static gboolean try_shift_graph(struct ui_psensor *ui, MyWidgetData *wd, 
                                 GtkWidget *widget) {
    if (!wd->cache_valid || !wd->graph_surface) return FALSE;
    
    int width = cairo_image_surface_get_width(wd->graph_surface);
    int height = cairo_image_surface_get_height(wd->graph_surface);
    
    if (width <= 1 || height <= 1) {
        wd->cache_valid = FALSE;
        ui->config->is_new_data = false;
        return FALSE;
    }
    
    draw_callback_lock(&ui->sensors_mutex);
    graph_shift_and_append(wd->graph_surface,
                          (const Psensor*const*)ui->sensors,
                          ui_get_graph_widget(),
                          ui->config, ui->main_window,
                          wd->last_values, wd->last_sensors_count);
    ui->config->is_new_data = false;
    draw_callback_unlock(&ui->sensors_mutex);
    
    wd->shift_count++;
    
    if (wd->shift_count > 500) {
        wd->cache_valid = FALSE;
        wd->shift_count = 0;
        gtk_widget_queue_draw(widget);
    }
    
    return TRUE;
}

static void composite_layers(cairo_t *cr, MyWidgetData *wd) {
    cairo_set_source_surface(cr, wd->background_surface, 0, 0);
    cairo_paint(cr);
    
    if (wd->graph_surface) {
        cairo_set_source_surface(cr, wd->graph_surface, 0, 0);
        cairo_paint(cr);
    }
}

// === MAIN CALLBACK (now much cleaner) ===
gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    struct ui_psensor *ui = (struct ui_psensor *)user_data;
    struct config *cfg = ui->config;
    MyWidgetData *wd = &cfg->widget_data;
    
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    
    gboolean size_changed = (wd->last_width != allocation.width || 
                            wd->last_height != allocation.height);
    
    // 1. Ensure surfaces
    ensure_graph_surface(wd, cr, allocation.width, allocation.height, size_changed);
    ensure_background_surface(ui, wd, cr, allocation.width, allocation.height, size_changed);
    
    // 2. Initialize data structures
    init_last_values(ui, wd);
    
    // 3. Draw graph content
    if (size_changed || !wd->cache_valid) {
        redraw_graph_full(ui, wd);
        update_last_values(ui, wd);
    } else if (cfg->is_new_data) {
        try_shift_graph(ui, wd, widget);
    }
    
    // 4. Composite final result
    composite_layers(cr, wd);
    
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

void ui_graph_cleanup(struct ui_psensor *ui_psensor)
{
    struct config *cfg = ui_psensor->config;
    MyWidgetData *widget_data = &cfg->widget_data;
    if (widget_data->last_values) {
        free(widget_data->last_values);
        widget_data->last_values = NULL;
    }

    if (widget_data->background_surface) {
        cairo_surface_destroy(widget_data->background_surface);
        widget_data->background_surface = NULL;
    }

    if (widget_data->graph_surface) {
        cairo_surface_destroy(widget_data->graph_surface);
        widget_data->graph_surface = NULL;
    }
}
