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
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <math.h>

#include <cfg.h>
#include <graph.h>
#include <parray.h>
#include <plog.h>
#include <psensor.h>
#include <ptime.h>

/* horizontal padding */
static const int GRAPH_H_PADDING = 8;
/* vertical padding */
static const int GRAPH_V_PADDING = 8;

bool is_smooth_curves_enabled;

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

static GtkStyleContext *style;
/* Foreground color of the current desktop theme */
static GdkRGBA theme_fg_color;
/* Background color of the current desktop theme */
static GdkRGBA theme_bg_color;

static void update_theme(GtkWidget *w)
{
	style = gtk_widget_get_style_context(w);

	gtk_style_context_get_background_color(style,
					       GTK_STATE_FLAG_NORMAL,
					       &theme_bg_color);
	gtk_style_context_get_color(style,
				    GTK_STATE_FLAG_NORMAL,
				    &theme_fg_color);
}

unsigned int compute_values_max_length(const struct config *c)
{
    if (!c || c->sensor_update_interval == 0) {
        return 10;
    }
    
    const unsigned int duration = c->graph_monitoring_duration * 60;
    const unsigned int interval = c->sensor_update_interval;
    
    // ceil: (duration + interval/2) / interval
    unsigned int n = 6 + (duration + interval / 2) / interval;
    
    return n;
}

static struct psensor **list_filter_graph_enabled(const struct psensor **sensors)
{
	if (!sensors)
		return NULL;

	const size_t n = psensor_list_size(sensors);
	struct psensor **result = (struct psensor **)calloc((n + 1), sizeof(struct psensor *));
	if (result == NULL)
		return NULL;

	const struct psensor **cur = sensors;
	size_t i = 0;
	for (; i < n && *cur; cur++ )
	{
		const struct psensor *s = *cur;
		bool is_graph_enabled = config_is_sensor_graph_enabled(s->id);
		if (is_graph_enabled)
		{
			result[i] = (struct psensor *)s;
			++i;
		}
	}

	result[i] = NULL;

	return result;
}

static time_t get_graph_end_time_s(const struct psensor **all_sensors)
{
    time_t latest_time = 0;

    while (all_sensors && *all_sensors) {
        const struct psensor *sensor = *all_sensors;
        
        if (sensor->measures_count == 0) {
            all_sensors++;
            continue;
        }

        int skip_count;
        if (is_smooth_curves_enabled)
            skip_count = 2;
        else
            skip_count = 0;

		struct measure_iterator it_reverse;
		measure_iterator_init_reverse(&it_reverse, sensor);
		struct measure *m;
        while (measure_iterator_prev(&it_reverse, &m)) {
			if (   m->value == UNKNOWN_DOUBLE_VALUE 
				|| !(m->time.tv_sec))
				continue;
			
			if (skip_count == 0) {
				if (m->time.tv_sec > latest_time) {
					latest_time = m->time.tv_sec;
					break;
				}
			}
			else
			{
				skip_count--;
			}
		}
		
        all_sensors++;
    }

    return latest_time;
}

static time_t get_graph_begin_time_s(const struct config *cfg, time_t etime)
{
	if (!etime)
		return 0;

	return etime - ((time_t)cfg->graph_monitoring_duration * (time_t)60);
}

static double compute_y(const double value, const double min, const double max, const double height,const double off)
{
    if (max <= min) {
        return height / 2.0 + off;
    }
    
    const double range = max - min;
    double normalized = (value - min) / range;
    
    if (normalized < 0.0) normalized = 0.0;
    if (normalized > 1.0) normalized = 1.0;
    
    const double result = height - (height * normalized) + off;
    
    return result;
}

static void draw_left_region(cairo_t *cr, const struct graph_info *info)
{
	cairo_set_source_rgb(cr,
			     theme_bg_color.red,
			     theme_bg_color.green,
			     theme_bg_color.blue);

	cairo_rectangle(cr, 0, 0, info->plot_x, info->canvas_height);
	cairo_fill(cr);
}

static void draw_right_region(cairo_t *cr, const struct graph_info *info)
{
	cairo_set_source_rgb(cr,
			     theme_bg_color.red,
			     theme_bg_color.green,
			     theme_bg_color.blue);


	cairo_rectangle(cr,
			info->plot_x + info->plot_width,
			0,
			info->plot_x + info->plot_width + GRAPH_H_PADDING,
			info->canvas_height);
	cairo_fill(cr);
}

static void
draw_graph_background(cairo_t *cr, const struct config *config, const struct graph_info *info)
{
	const struct color *bgcolor;

	bgcolor = config->graph_bgcolor;

	if (config->alpha_channel_enabled)
		cairo_set_source_rgba(cr,
				      theme_bg_color.red,
				      theme_bg_color.green,
				      theme_bg_color.blue,
				      config->graph_bg_alpha);
	else
		cairo_set_source_rgb(cr,
				     theme_bg_color.red,
				     theme_bg_color.green,
				     theme_bg_color.blue);

	cairo_rectangle(cr, info->plot_x, 0, info->plot_width, info->canvas_height);
	cairo_fill(cr);

	if (config->alpha_channel_enabled)
		cairo_set_source_rgba(cr,
				      bgcolor->red,
				      bgcolor->green,
				      bgcolor->blue,
				      config->graph_bg_alpha);
	else
		cairo_set_source_rgb(cr,
				     bgcolor->red,
				     bgcolor->green,
				     bgcolor->blue);

	cairo_rectangle(cr,
			info->plot_x,
			info->plot_y,
			info->plot_width,
			info->plot_height);
	cairo_fill(cr);
}

/* setup dash style */
static double dashes[] = {
	1.0,		/* ink */
	2.0,		/* skip */
};
static int ndash = ARRAY_SIZE(dashes);

static void draw_background_lines(cairo_t *cr, const int min, const int max,
                                  const struct config *config, const graph_info_st *info)
{
    const struct color *color = config->graph_fgcolor;
    
    cairo_set_line_width(cr, 1);
    cairo_set_dash(cr, dashes, ndash, 0);
    cairo_set_source_rgb(cr, color->red, color->green, color->blue);
    
    /* vertical lines (time) - 5 lines */
    for (int i = 0; i <= 5; i++) {
        double x = info->plot_x + (i * info->plot_width / 5.0);
        cairo_move_to(cr, x, info->plot_y);
        cairo_line_to(cr, x, info->plot_y + info->plot_height);
    }
    
    /* horizontal lines (value) - Always 5 lines, regardless of range */
    double range = (double)(max - min);
    
    /* Handle edge case: min == max */
    if (range <= 0.1) {
        /* Just draw one line in the middle */
        double y = info->plot_y + (info->plot_height / 2.0);
        cairo_move_to(cr, info->plot_x, y);
        cairo_line_to(cr, info->plot_x + info->plot_width, y);
    } else {
        /* Draw 5 lines dividing the range into 6 equal segments */
        for (int i = 0; i <= 5; i++) {
            /* Calculate value at this position */
            double fraction = i / 5.0;  // 0.0, 0.2, 0.4, 0.6, 0.8, 1.0
            double value = min + (fraction * range);
            
            /* Calculate Y position */
            double y = compute_y(value, min, max, 
                                info->plot_height, info->plot_y);
            
            /* Draw the line */
            cairo_move_to(cr, info->plot_x, y);
            cairo_line_to(cr, info->plot_x + info->plot_width, y);
            
            /* Optional: Draw value label on left side */
            //if (config->show_grid_labels) 
			//{
                char label[32];
                //if (config->is_temperature) {
                    snprintf(label, sizeof(label), "%.0f°C", value);
                // } else {
                //     snprintf(label, sizeof(label), "%.0f%%", value);
                // }
                
                cairo_text_extents_t extents;
                cairo_text_extents(cr, label, &extents);
                
                /* Draw label to the left of the line */
                cairo_move_to(cr, info->plot_x - extents.width - 5, 
                            y + (extents.height / 2));
                cairo_show_text(cr, label);
            //}
        }
    }
    
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
}

typedef void (*draw_sensor_curve_function_type) (const struct psensor *sensor, cairo_t *cr,const double min, const double max, const time_t begin_time, const time_t end_time, const struct graph_info *info);

/* ==================== SIMPLER VERSION: EVERY 4 POINTS BEZIER ==================== */
static void draw_sensor_segmented_bezier(const struct psensor *sensor, cairo_t *cr, const double min, const double max, const time_t begin_time, const time_t end_time, const struct graph_info *info)
{
    /* This version draws Bezier curve for EVERY 4 points, ensuring all points are used */    
    if (!sensor || sensor->measures_count < 4 || begin_time >= end_time) {
        return;
    }
    
    GdkRGBA *color = config_get_sensor_color(sensor->id);
    if (!color) return;
    
    cairo_set_source_rgb(cr, color->red, color->green, color->blue);
    gdk_rgba_free(color);
    
    const double time_scale = info->plot_width / (double)(end_time - begin_time);
    const double value_range = (max > min) ? (max - min) : 1.0;
    //printf("%s minmax=%.1f:%.1f time=%ld:%ld:%ld \n", sensor->name,min,max, begin_time, end_time, end_time - begin_time);
    /* We'll draw as we iterate - no need to store all points */
    double segment_x[4], segment_y[4];
    int segment_idx = 0;
    bool has_started = false;
    
    struct measure_iterator it;
    measure_iterator_init(&it, sensor);
    
    struct measure *m;
    while (measure_iterator_next(&it, &m)) {
        time_t t = m->time.tv_sec;
        double v = m->value;
        
        if (v == UNKNOWN_DOUBLE_VALUE || t == 0 || t < begin_time || t > end_time) {
            /* Skip invalid, but continue with next valid point */
            continue;
        }
        
        /* Calculate point */
        segment_x[segment_idx] = info->plot_x + ((double)(t - begin_time) * time_scale);
        
        double normalized = (v - min) / value_range;
        if (normalized < 0.0) normalized = 0.0;
        if (normalized > 1.0) normalized = 1.0;
        segment_y[segment_idx] = info->plot_y + ((1.0 - normalized) * info->plot_height);
        
        segment_idx++;
        
        /* When we have 4 points, draw a Bezier segment */
        if (segment_idx == 4) {
            if (!has_started) {
                cairo_move_to(cr, segment_x[0], segment_y[0]);
                has_started = true;
            }
            
            /* Draw cubic Bezier through these 4 points */
            /* Control points: interpolated between points */
            double cp1_x = (segment_x[0] + 2*segment_x[1]) / 3.0;
            double cp1_y = (segment_y[0] + 2*segment_y[1]) / 3.0;
            
            double cp2_x = (2*segment_x[2] + segment_x[3]) / 3.0;
            double cp2_y = (2*segment_y[2] + segment_y[3]) / 3.0;
            
            cairo_curve_to(cr, cp1_x, cp1_y, cp2_x, cp2_y, segment_x[3], segment_y[3]);
            
            /* Shift for next segment: last point becomes first of next */
            segment_x[0] = segment_x[3];
            segment_y[0] = segment_y[3];
            segment_idx = 1; // We keep the last point
        }
    }
    
    /* Draw any remaining points (less than 4) as polyline */
    if (segment_idx > 1) {
        if (!has_started) {
            cairo_move_to(cr, segment_x[0], segment_y[0]);
        }
        for (int i = 1; i < segment_idx; i++) {
            cairo_line_to(cr, segment_x[i], segment_y[i]);
        }
    }
    
    if (has_started || segment_idx > 1) {
        cairo_stroke(cr);
    }
}

static void draw_sensor_curve(const struct psensor *s, cairo_t *cr, const double min, const double max, const time_t begin_time, const time_t ending_time, const struct graph_info *info)
{
    //cairo_new_path(cr);
	// ... setup color ...
    GdkRGBA *color = config_get_sensor_color(s->id);
    cairo_set_source_rgb(cr, color->red, color->green, color->blue);

    struct measure_iterator it;
    measure_iterator_init(&it, s);
    
    struct measure *m;
    bool first = true;
    //printf("%s ", s->name);   
    time_t time_range = ending_time - begin_time;
    if (time_range <= 0) {
        time_range = 1;  // Tránh chia cho 0
    }
    
    while (measure_iterator_next(&it, &m)) {
        if (m->value == UNKNOWN_DOUBLE_VALUE || !(m->time.tv_sec))
            continue;
            
        time_t vdt = m->time.tv_sec - begin_time;        
        //
        double normalized_x = (double)vdt / time_range;
        if (normalized_x < 0.0) normalized_x = 0.0;
        if (normalized_x > 1.0) normalized_x = 1.0;
        
        double x = normalized_x * info->plot_width + info->plot_x;
        double y = compute_y(m->value, min, max, info->plot_height, info->plot_y);
        
        // printf("DEBUG:time=%ld|bt=%ld|et=%ld|vdt=%ld|width=%.1f|xoff=%.1f|xy=%05.2f:%05.2f minmax=%.2f:%.2f v=%.2f|\n",
       	// 		       m->time.tv_sec,
		// 			            begin_time,    ending_time,    vdt,    info->plot_width,
		// 						                                 info->plot_x,
		// 														           x,y,             min,max,       m->value);
        //printf("%ld:%.2f=%05.2f:%05.2f minmax=%.2f:%.2f\n", m->time.tv_sec,m->value, x,y, min, max);	
        
        if (first) {
            cairo_move_to(cr, x, y);
            first = false;
        } else {
            cairo_line_to(cr, x, y);
        }
    }
	cairo_stroke(cr);
	gdk_rgba_free(color);
}

static void display_no_graphs_warning(cairo_t *cr, int x, int y)
{
	char *msg;

	msg = strdup(_("No graphs enabled"));

	cairo_select_font_face(cr,
			       "sans-serif",
			       CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 18.0);

	cairo_move_to(cr, x, y);
	cairo_show_text(cr, msg);

	free(msg);
}

typedef struct {
    double width;
    double height;
} TextDimensions;

typedef struct {
    double time_labels_height;
    double value_labels_width;
} LabelMargins;

static TextDimensions measure_labels(cairo_t *cr, const char *str_btime, const char *str_etime, const char *strmax, const char *strmin) 
{
    TextDimensions dims = {0};
    cairo_text_extents_t ext;
    
    // Measure time labels (for bottom margin)
    cairo_text_extents(cr, str_btime, &ext);
    dims.height = ext.height;
    
    cairo_text_extents(cr, str_etime, &ext);
    if (ext.height > dims.height) dims.height = ext.height;
    
    // Measure value labels (for left margin)  
    cairo_text_extents(cr, strmax, &ext);
    dims.width = ext.width;
    
    cairo_text_extents(cr, strmin, &ext);
    if (ext.width > dims.width) dims.width = ext.width;
    
    return dims;
}

static LabelMargins calculate_margins(const TextDimensions *label_dims) 
{
    LabelMargins margins;
    
    // Bottom margin: time labels + padding
    margins.time_labels_height = label_dims->height + (2 * GRAPH_V_PADDING);
    
    // Left margin: value labels + padding  
    margins.value_labels_width = label_dims->width + (2 * GRAPH_H_PADDING);
    
    return margins;
}

void calculate_graph_layout_clean(const GtkAllocation *galloc, const char *str_btime, const char *str_etime, const char *strmax, const char *strmin, graph_info_st *graphInfo, cairo_t *cr)
{
    // Setup text rendering once
    cairo_select_font_face(cr, "sans-serif", 
                           CAIRO_FONT_SLANT_NORMAL, 
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    
    // Step 1: Measure all text labels
    TextDimensions label_dims = measure_labels(cr, str_btime, str_etime, strmax, strmin);
    
    // Step 2: Calculate required margins for labels
    LabelMargins margins = calculate_margins(&label_dims);
    
    // Step 3: Set canvas dimensions
    graphInfo->canvas_width = galloc->width;
    graphInfo->canvas_height = galloc->height;
    
    // Step 4: Calculate plotting area (central chart region)
    // Top padding only, bottom reserved for time labels
    graphInfo->plot_y = GRAPH_V_PADDING;
    graphInfo->plot_height = graphInfo->canvas_height 
                           - GRAPH_V_PADDING           // top padding
                           - margins.time_labels_height; // bottom labels + padding
    
    // Left side reserved for value labels, right side has padding
    graphInfo->plot_x = margins.value_labels_width;
    graphInfo->plot_width = graphInfo->canvas_width 
                          - margins.value_labels_width  // left labels + padding
                          - GRAPH_H_PADDING;            // right padding
}

/* ==================== TYPE DEFINITIONS ==================== */
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

/* ==================== HELPER FUNCTIONS ==================== */
static void calculate_plotting_area(const cairo_text_extents_t *extents_btime, const cairo_text_extents_t *extents_etime, const cairo_text_extents_t *extents_max, const cairo_text_extents_t *extents_min, graph_info_st *info)
{
    /* Vertical layout: reserve space for time labels at bottom */
    info->plot_y = GRAPH_V_PADDING;
    
    double plot_height = info->canvas_height - GRAPH_V_PADDING;
    double max_time_label_height = (extents_etime->height > extents_btime->height) 
                                 ? extents_etime->height 
                                 : extents_btime->height;
    
    plot_height -= GRAPH_V_PADDING + max_time_label_height + GRAPH_V_PADDING;
    info->plot_height = plot_height;
    
    /* Horizontal layout: reserve space for value labels at left */
    double max_value_label_width = (extents_min->width > extents_max->width) 
                                 ? extents_min->width 
                                 : extents_max->width;
    
    info->plot_x = (2 * GRAPH_H_PADDING) + max_value_label_width;
    info->plot_width = info->canvas_width - info->plot_x - GRAPH_H_PADDING;
}

static void calculate_graph_layout(GtkWidget *w_graph, const char *str_btime, const char *str_etime, const char *str_max, const char *str_min, graph_info_st *info)
{
    GtkAllocation allocation;
    gtk_widget_get_allocation(w_graph, &allocation);
    
    /* Initialize canvas dimensions */
    info->canvas_width = allocation.width;
    info->canvas_height = allocation.height;
    
    /* Setup text rendering for measurements */
    cairo_t *cr = cairo_create(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1));
    cairo_select_font_face(cr, "sans-serif", 
                           CAIRO_FONT_SLANT_NORMAL, 
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    
    /* Measure text dimensions */
    cairo_text_extents_t extents_btime, extents_etime, extents_max, extents_min;
    cairo_text_extents(cr, str_etime, &extents_etime);
    cairo_text_extents(cr, str_btime, &extents_btime);
    cairo_text_extents(cr, str_max, &extents_max);
    cairo_text_extents(cr, str_min, &extents_min);
    
    cairo_destroy(cr);
    
    /* Calculate plotting area position and size */
    calculate_plotting_area(&extents_btime, &extents_etime,
                           &extents_max, &extents_min, info);
}

static GraphDrawingContext* create_graph_context(const struct psensor **enabled_sensors, const struct config *config, GtkWidget *w_graph)
{
    GraphDrawingContext *ctx = calloc(1, sizeof(GraphDrawingContext));
    if (!ctx) return NULL;
    
    const unsigned int use_celsius = config_get_temperature_unit_2();
    ctx->all_minmax = get_all_minmax_value(enabled_sensors);
    
    /* Setup value ranges and labels */
    ctx->str_min = psensor_value_to_str(SENSOR_TYPE_TEMP, ctx->all_minmax.temp.min, use_celsius);
    ctx->str_max = psensor_value_to_str(SENSOR_TYPE_TEMP, ctx->all_minmax.temp.max, use_celsius);
    ctx->str_unit = psensor_unit_to_str(SENSOR_TYPE_TEMP, use_celsius);
    
    /* Setup time range and labels */
    ctx->end_time = get_graph_end_time_s(enabled_sensors);
    ctx->begin_time = get_graph_begin_time_s(config, ctx->end_time);
    ctx->str_btime = time_to_str(ctx->begin_time);
    ctx->str_etime = time_to_str(ctx->end_time);
    
    /* Calculate graph layout */
    calculate_graph_layout(w_graph, ctx->str_btime, ctx->str_etime,
                          ctx->str_max, ctx->str_min, &ctx->layout);
    
    return ctx;
}

static void free_graph_context(GraphDrawingContext *ctx)
{
    if (!ctx) return;
    
    free(ctx->str_min);
    free(ctx->str_max);
    free(ctx->str_unit);
    free(ctx->str_btime);
    free(ctx->str_etime);
    free(ctx);
}

/* ==================== DRAWING FUNCTIONS ==================== */

static void draw_graph_background_and_labels(cairo_t *cr, const struct config *config, const GraphDrawingContext *ctx)
{
    /* Draw background */
    draw_graph_background(cr, config, &ctx->layout);
    
    /* Set text color */
    cairo_set_source_rgb(cr, theme_fg_color.red,
                         theme_fg_color.green,
                         theme_fg_color.blue);
    
    /* Draw time labels at bottom */
    cairo_text_extents_t extents_etime;
    cairo_text_extents(cr, ctx->str_etime, &extents_etime);
    
    /* Begin time (left) */
    cairo_move_to(cr, ctx->layout.plot_x, 
                  ctx->layout.canvas_height - GRAPH_V_PADDING);
    cairo_show_text(cr, ctx->str_btime);
    
    /* End time (right) */
    cairo_move_to(cr, ctx->layout.canvas_width - extents_etime.width - GRAPH_H_PADDING,
                  ctx->layout.canvas_height - GRAPH_V_PADDING);
    cairo_show_text(cr, ctx->str_etime);
    
    /* Draw grid lines */
    draw_background_lines(cr, (int)ctx->all_minmax.temp.min, (int)ctx->all_minmax.temp.max,
                         config, &ctx->layout);
    
    /* Draw side regions */
    draw_left_region(cr, &ctx->layout);
    draw_right_region(cr, &ctx->layout);
}

static void draw_value_labels(cairo_t *cr, const GraphDrawingContext *ctx)
{
    cairo_set_source_rgb(cr, theme_fg_color.red,
                         theme_fg_color.green,
                         theme_fg_color.blue);
    
    /* Measure text for positioning */
    cairo_text_extents_t extents_max, extents_min;
    cairo_text_extents(cr, ctx->str_max, &extents_max);
    cairo_text_extents(cr, ctx->str_min, &extents_min);
    
    /* Draw max value and unit (top-left) */
    cairo_move_to(cr, GRAPH_H_PADDING, 2*extents_max.height + GRAPH_V_PADDING);
    cairo_show_text(cr, ctx->str_max);
    
    cairo_move_to(cr, GRAPH_H_PADDING, 4*extents_max.height + GRAPH_V_PADDING);
    cairo_show_text(cr, ctx->str_unit);
    
    /* Draw min value and unit (bottom-left) */
    cairo_move_to(cr, GRAPH_H_PADDING, 
                  ctx->layout.canvas_height - (4*extents_min.height) - ctx->layout.plot_y);
    cairo_show_text(cr, ctx->str_min);
    
    cairo_move_to(cr, GRAPH_H_PADDING, 
                  ctx->layout.canvas_height - (2*extents_min.height) - ctx->layout.plot_y);
    cairo_show_text(cr, ctx->str_unit);
}

static void draw_sensor_curves(cairo_t *cr, const struct psensor **enabled_sensors,const GraphDrawingContext *ctx, const struct config *config)
{
    if (ctx->begin_time == 0 || ctx->end_time == 0) {
        return;
    }
    
    /* Setup line drawing properties */
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, 1.0);
    
    /* Select curve drawing function */
    draw_sensor_curve_function_type draw_curve;
    if (is_smooth_curves_enabled) {
        draw_curve = &draw_sensor_segmented_bezier;
    } else {
        draw_curve = &draw_sensor_curve;
    }
    
    /* Draw each sensor */
    bool has_graphs = false;
    const struct psensor **sensor = enabled_sensors;
    
    while (*sensor) {
        has_graphs = true;
        const struct psensor *s = *sensor;
        
        /* Determine value range for this sensor type */
        double min_val, max_val;
        if (s->type & SENSOR_TYPE_RPM) {
            min_val = ctx->all_minmax.rpm.min;
            max_val = ctx->all_minmax.rpm.max;
        } else if (s->type & SENSOR_TYPE_PERCENT) {
            min_val = 0;
            max_val = ctx->all_minmax.percent.max;
        } else {
            min_val = ctx->all_minmax.temp.min;
            max_val = ctx->all_minmax.temp.max;
        }
        
        /* Draw the curve */
        draw_curve(s, cr, min_val, max_val,
                  ctx->begin_time, ctx->end_time,
                  &ctx->layout);
        
        sensor++;
    }
    
    /* Show warning if no graphs were drawn */
    if (!has_graphs) {
        display_no_graphs_warning(cr,
                                  ctx->layout.plot_x + 12,
                                  ctx->layout.plot_height / 2);
    }
}

/* ==================== MAIN FUNCTION ==================== */

void redraw_graph(cairo_surface_t *graph_surface, cairo_t *cr, const struct psensor **sensors, GtkWidget *w_graph, const struct config *config, GtkWidget *window)
{
    /* Early exit if widget not drawable */
    if (!gtk_widget_is_drawable(w_graph)) {
        return;
    }
    
    /* Initialize theme if needed */
    if (!style) {
        update_theme(window);
    }
    
    /* Get only enabled sensors */
    const struct psensor **enabled_sensors = 
        (const struct psensor **)list_filter_graph_enabled(sensors);
    
    /* Create drawing context with all needed data */
    GraphDrawingContext *ctx = create_graph_context(enabled_sensors, config, w_graph);
    if (!ctx) {
        free((void*)enabled_sensors);
        return;
    }
    
    /* Draw background, grid, and labels */
    draw_graph_background_and_labels(cr, config, ctx);
    draw_value_labels(cr, ctx);
    
    /* Draw sensor curves */
    draw_sensor_curves(cr, enabled_sensors, ctx, config);
    
    /* Cleanup */
    free_graph_context(ctx);
    free((void*)enabled_sensors);
}
