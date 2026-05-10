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
#ifndef PSENSOR_GRAPH_H
#define PSENSOR_GRAPH_H

#include <gtk/gtk.h>

#include <cfg.h>
#include <psensor.h>

extern bool is_smooth_curves_enabled;

/* Graph rendering dimensions and offsets */
typedef struct graph_info {
	/* Plotting area position relative to canvas */
	double plot_x;      /* X offset of plotting area */
	double plot_y;      /* Y offset of plotting area */
	
	/* Plotting area dimensions */
	double plot_width;  /* Width of data drawing area */
	double plot_height; /* Height of data drawing area */
	
	/* Total canvas dimensions */
	double canvas_width;  /* Total drawing area width */
	double canvas_height; /* Total drawing area height */
} graph_info_st;
// ┌─────────────────────────────────────────┐
// │ Canvas (width x height)                 │
// │                                         │
// │   ┌─────────────────────────────┐       │
// │   │ Plotting area               │       │
// │   │ (plot_width x plot_height)  │       │
// │   │                             │       │
// │   │   [Data curves drawn here]  │       │
// │   │                             │       │
// │   └─────────────────────────────┘       │
// │    ↑plot_y                              │
// │    └──→plot_x                           │
// │                                         │
// └─────────────────────────────────────────┘

typedef struct {
    graph_info_st layout;
    char *str_min;
    char *str_max;
    char *str_unit;
    char *str_btime;
    char *str_etime;
    time_t begin_time;
    time_t end_time;
	ALL_MINMAX all_minmax;
} GraphDrawingContext;


void graph_update(Psensor **sensors,
		  GtkWidget *w_graph,
		  struct config *config,
		  GtkWidget *window);

void
redraw_graph(cairo_surface_t *graph_surface,
	         cairo_t *cr,
	         const Psensor *const *sensors,
	         GtkWidget *w_graph,
	         const struct config *config,
	         GtkWidget *window);

/* chỉ vẽ background (nền, trục, nhãn) */
void redraw_background_only(cairo_surface_t *surface, cairo_t *cr,
                            const Psensor *const *sensors, GtkWidget *w_graph,
                            const struct config *config, GtkWidget *window);

/* chỉ vẽ curves (từ đầu) */
void redraw_curves_only(cairo_surface_t *surface, cairo_t *cr,
                        const Psensor *const *sensors, GtkWidget *w_graph,
                        const struct config *config, GtkWidget *window);

/* Compute the number of measures which must be kept. */
unsigned int compute_values_max_length(const struct config *);

/* dịch chuyển */
void graph_shift_and_append(cairo_surface_t *graph_surface,
                            const Psensor * const *sensors,
                            GtkWidget *w_graph,
                            const struct config *config,
                            GtkWidget *window,
                            double *last_values_buffer,
                            size_t buffer_size);
                            
const Psensor **list_filter_graph_enabled(const Psensor * const *sensors);
#endif
