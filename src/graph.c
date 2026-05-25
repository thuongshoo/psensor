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
#include <graph.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <cfg.h>
#include <parray.h>
#include <plog.h>
#include <psensor.h>
#include <ptime.h>

/* horizontal padding */
const int GRAPH_H_PADDING = 8;
/* vertical padding */
const int GRAPH_V_PADDING = 8;

bool is_smooth_curves_enabled;

static GtkStyleContext *style;
/* Foreground color of the current desktop theme */
GdkRGBA theme_fg_color;
/* Background color of the current desktop theme */
GdkRGBA theme_bg_color;

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
    if (!c || c->sensor_update_interval == 0)
        return 10;

    const unsigned int duration = c->graph_monitoring_duration * 60U;
    const unsigned int interval = c->sensor_update_interval;

    // ceil: (duration + interval/2) / interval
    unsigned int n = 6 + (duration + interval / 2) / interval;

    return n;
}

const Psensor **list_filter_graph_enabled(const Psensor *const *sensors)
{
    if (!sensors)
        return NULL;

    const size_t n = psensor_list_size(sensors);
    const Psensor **result = (const Psensor **)calloc((n + 1), sizeof(Psensor *));
    if (result == NULL)
        return NULL;

    const Psensor *const *cur = sensors;
    size_t i = 0;
    for (; i < n && *cur; cur++)
    {
        const Psensor *s = *cur;
        bool is_graph_enabled = config_is_sensor_graph_enabled(s->id);
        if (is_graph_enabled)
        {
            result[i] = s;
            ++i;
        }
    }

    result[i] = NULL;

    return result;
}

time_t get_graph_end_time_s(const Psensor *const *all_sensors)
{
    time_t latest_time = 0;

    while (all_sensors && *all_sensors)
    {
        const Psensor *sensor = *all_sensors;

        if (sensor->measures_count == 0)
        {
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
        while (measure_iterator_prev(&it_reverse, &m))
        {
            if (m->value == UNKNOWN_DOUBLE_VALUE || !(m->time.tv_sec))
                continue;

            if (skip_count == 0)
            {
                if (m->time.tv_sec > latest_time)
                {
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

time_t get_graph_begin_time_s(const struct config *cfg, time_t etime)
{
    if (!etime)
        return 0;

    return etime - ((time_t)cfg->graph_monitoring_duration * (time_t)60);
}

static double compute_y(const double value, const double min, const double max, const double height, const double off)
{
    if (max <= min)
    {
        return height / 2.0 + off;
    }

    const double range = max - min;
    double normalized = (value - min) / range;

    if (normalized < 0.0)
        normalized = 0.0;
    if (normalized > 1.0)
        normalized = 1.0;

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
    1.0, /* ink */
    2.0, /* skip */
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
    for (int i = 0; i <= 5; i++)
    {
        double x = info->plot_x + (i * info->plot_width / 5.0);
        cairo_move_to(cr, x, info->plot_y);
        cairo_line_to(cr, x, info->plot_y + info->plot_height);
    }

    /* horizontal lines (value) - Always 5 lines, regardless of range */
    double range = (double)(max - min);

    /* Handle edge case: min == max */
    if (range <= 0.1)
    {
        /* Just draw one line in the middle */
        double y = info->plot_y + (info->plot_height / 2.0);
        cairo_move_to(cr, info->plot_x, y);
        cairo_line_to(cr, info->plot_x + info->plot_width, y);
    }
    else
    {
        /* Draw 5 lines dividing the range into 6 equal segments */
        for (int i = 0; i <= 5; i++)
        {
            /* Calculate value at this position */
            double fraction = i / 5.0; // 0.0, 0.2, 0.4, 0.6, 0.8, 1.0
            double value = min + (fraction * range);

            /* Calculate Y position */
            double y = compute_y(value, min, max,
                                 info->plot_height, info->plot_y);

            /* Draw the line */
            cairo_move_to(cr, info->plot_x, y);
            cairo_line_to(cr, info->plot_x + info->plot_width, y);

            /* Optional: Draw value label on left side */
            // if (config->show_grid_labels)
            //{
            char label[32];
            // if (config->is_temperature) {
            snprintf(label, sizeof(label), "%.0f", value);
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

typedef void (*draw_sensor_curve_function_type)(const Psensor *sensor, cairo_t *cr, const double min, const double max, const time_t begin_time, const time_t end_time, const struct graph_info *info);

/* ==================== SIMPLER VERSION: EVERY 4 POINTS BEZIER ==================== */
static void draw_sensor_segmented_bezier(const Psensor *sensor, cairo_t *cr, const double min, const double max, const time_t begin_time, const time_t end_time, const struct graph_info *info)
{
    /* This version draws Bezier curve for EVERY 4 points, ensuring all points are used */
    if (!sensor || sensor->measures_count < 4 || begin_time >= end_time)
        return;

    GdkRGBA *color = config_get_sensor_color(sensor->id);
    if (!color)
        return;

    cairo_set_source_rgb(cr, color->red, color->green, color->blue);
    gdk_rgba_free(color);

    const double time_scale = info->plot_width / (double)(end_time - begin_time);
    const double value_range = (max > min) ? (max - min) : 1.0;
    // printf("%s minmax=%.1f:%.1f time=%ld:%ld:%ld \n", sensor->name,min,max, begin_time, end_time, end_time - begin_time);
    /* We'll draw as we iterate - no need to store all points */
    double segment_x[4], segment_y[4];
    int segment_idx = 0;
    bool has_started = false;

    struct measure_iterator it;
    measure_iterator_init(&it, sensor);

    struct measure *m;
    while (measure_iterator_next(&it, &m))
    {
        time_t t = m->time.tv_sec;
        double v = m->value;

        if (v == UNKNOWN_DOUBLE_VALUE || t == 0 || t < begin_time || t > end_time)
        {
            /* Skip invalid, but continue with next valid point */
            continue;
        }

        /* Calculate point */
        segment_x[segment_idx] = info->plot_x + ((double)(t - begin_time) * time_scale);

        double normalized = (v - min) / value_range;
        if (normalized < 0.0)
            normalized = 0.0;
        if (normalized > 1.0)
            normalized = 1.0;
        segment_y[segment_idx] = info->plot_y + ((1.0 - normalized) * info->plot_height);

        segment_idx++;

        /* When we have 4 points, draw a Bezier segment */
        if (segment_idx == 4)
        {
            if (!has_started)
            {
                cairo_move_to(cr, segment_x[0], segment_y[0]);
                has_started = true;
            }

            /* Draw cubic Bezier through these 4 points */
            /* Control points: interpolated between points */
            double cp1_x = (segment_x[0] + 2 * segment_x[1]) / 3.0;
            double cp1_y = (segment_y[0] + 2 * segment_y[1]) / 3.0;

            double cp2_x = (2 * segment_x[2] + segment_x[3]) / 3.0;
            double cp2_y = (2 * segment_y[2] + segment_y[3]) / 3.0;

            cairo_curve_to(cr, cp1_x, cp1_y, cp2_x, cp2_y, segment_x[3], segment_y[3]);

            /* Shift for next segment: last point becomes first of next */
            segment_x[0] = segment_x[3];
            segment_y[0] = segment_y[3];
            segment_idx = 1; // We keep the last point
        }
    }

    /* Draw any remaining points (less than 4) as polyline */
    if (segment_idx > 1)
    {
        if (!has_started)
        {
            cairo_move_to(cr, segment_x[0], segment_y[0]);
        }
        for (int i = 1; i < segment_idx; i++)
        {
            cairo_line_to(cr, segment_x[i], segment_y[i]);
        }
    }

    if (true == has_started || (segment_idx > 1))
        cairo_stroke(cr);
}

static void draw_sensor_curve(const Psensor *s, cairo_t *cr, const double min, const double max, const time_t begin_time, const time_t ending_time, const struct graph_info *info)
{
    // cairo_new_path(cr);
    //  ... setup color ...
    GdkRGBA *color = config_get_sensor_color(s->id);
    cairo_set_source_rgb(cr, color->red, color->green, color->blue);

    struct measure_iterator it;
    measure_iterator_init(&it, s);

    struct measure *m;
    bool first = true;
    // printf("%s ", s->name);
    time_t time_range = ending_time - begin_time;
    if (time_range <= 0)
    {
        time_range = 1; // Tránh chia cho 0
    }

    while (measure_iterator_next(&it, &m))
    {
        if (m->value == UNKNOWN_DOUBLE_VALUE || !(m->time.tv_sec))
            continue;

        time_t vdt = m->time.tv_sec - begin_time;
        //
        double normalized_x = (double)vdt / (double)time_range;
        if (normalized_x < 0.0)
            normalized_x = 0.0;
        if (normalized_x > 1.0)
            normalized_x = 1.0;

        double x = normalized_x * info->plot_width + info->plot_x;
        double y = compute_y(m->value, min, max, info->plot_height, info->plot_y);

        g_print("DEBUG:time=%ld|bt=%ld|et=%ld|vdt=%ld|width=%.1f|xoff=%.1f|xy=%05.2f:%05.2f minmax=%.2f:%.2f v=%.2f|\n",
                m->time.tv_sec,
                begin_time, ending_time, vdt, info->plot_width,
                info->plot_x,
                x, y, min, max, m->value);
        // printf("%ld:%.2f=%05.2f:%05.2f minmax=%.2f:%.2f\n", m->time.tv_sec,m->value, x,y, min, max);

        if (first)
        {
            cairo_move_to(cr, x, y);
            first = false;
        }
        else
        {
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

/* ==================== TYPE DEFINITIONS ==================== */

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
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surface);
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
    cairo_surface_destroy(surface);
    /* Calculate plotting area position and size */
    calculate_plotting_area(&extents_btime, &extents_etime,
                            &extents_max, &extents_min, info);
}

static GraphDrawingContext *create_graph_context(const Psensor *const *enabled_sensors, const struct config *config, GtkWidget *w_graph)
{
    GraphDrawingContext *ctx = calloc(1, sizeof(GraphDrawingContext));
    if (!ctx)
        return NULL;

    const Temperature_Unit temperature_unit = config_get_temperature_unit();
    ctx->all_minmax = get_all_minmax_value(enabled_sensors);

    /* Setup value ranges and labels */
    ctx->str_min = psensor_value_to_str(SENSOR_TYPE_TEMP, ctx->all_minmax.temp.min, temperature_unit);
    ctx->str_max = psensor_value_to_str(SENSOR_TYPE_TEMP, ctx->all_minmax.temp.max, temperature_unit);
    ctx->str_unit = psensor_unit_to_str(SENSOR_TYPE_TEMP, temperature_unit);

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
    if (!ctx)
        return;

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
    cairo_move_to(cr, GRAPH_H_PADDING, 2 * extents_max.height + GRAPH_V_PADDING);
    cairo_show_text(cr, ctx->str_max);

    cairo_move_to(cr, GRAPH_H_PADDING, 4 * extents_max.height + GRAPH_V_PADDING);
    cairo_show_text(cr, ctx->str_unit);

    /* Draw min value and unit (bottom-left) */
    cairo_move_to(cr, GRAPH_H_PADDING,
                  ctx->layout.canvas_height - (4 * extents_min.height) - ctx->layout.plot_y);
    cairo_show_text(cr, ctx->str_min);

    cairo_move_to(cr, GRAPH_H_PADDING,
                  ctx->layout.canvas_height - (2 * extents_min.height) - ctx->layout.plot_y);
    cairo_show_text(cr, ctx->str_unit);
}

static void draw_sensor_curves(cairo_t *cr, const Psensor *const *enabled_sensors, const GraphDrawingContext *ctx, const struct config *config)
{
    if (ctx->begin_time == 0 || ctx->end_time == 0)
        return;

    /* Setup line drawing properties */
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, 1.0);

    /* Select curve drawing function */
    draw_sensor_curve_function_type draw_curve;
    if (is_smooth_curves_enabled)
    {
        draw_curve = &draw_sensor_segmented_bezier;
    }
    else
    {
        draw_curve = &draw_sensor_curve;
    }

    /* Draw each sensor */
    bool has_graphs = false;
    const Psensor *const *sensor = enabled_sensors;

    while (*sensor)
    {
        has_graphs = true;
        const Psensor *s = *sensor;

        /* Determine value range for this sensor type */
        double min_val, max_val;
        if (s->type & SENSOR_TYPE_RPM)
        {
            min_val = ctx->all_minmax.rpm.min;
            max_val = ctx->all_minmax.rpm.max;
        }
        else if (s->type & SENSOR_TYPE_PERCENT)
        {
            min_val = 0;
            max_val = ctx->all_minmax.percent.max;
        }
        else
        {
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
    if (!has_graphs)
    {
        display_no_graphs_warning(cr,
                                  12 + (int)ctx->layout.plot_x,
                                  (int)(ctx->layout.plot_height / 2));
    }
}

/* ==================== MAIN FUNCTION ==================== */

void redraw_graph(cairo_surface_t *graph_surface, cairo_t *cr,
                  const Psensor *const *graph_enabled_sensors,
                  GtkWidget *w_graph, const struct config *config, GtkWidget *window)
{
    if (!gtk_widget_is_drawable(w_graph))
    {
        return;
    }

    if (!style)
    {
        update_theme(window);
    }

    // KHÔNG gọi list_filter_graph_enabled nữa, dùng trực tiếp graph_enabled_sensors

    GraphDrawingContext *ctx = create_graph_context(graph_enabled_sensors, config, w_graph);
    if (!ctx)
    {
        return;
    }

    draw_graph_background_and_labels(cr, config, ctx);
    draw_value_labels(cr, ctx);
    draw_sensor_curves(cr, graph_enabled_sensors, ctx, config);

    free_graph_context(ctx);
    // KHÔNG free(graph_enabled_sensors) vì caller sở hữu
}

void redraw_background_only(cairo_surface_t *surface, cairo_t *cr,
                            const Psensor *const *graph_enabled_sensors,
                            GtkWidget *w_graph,
                            const struct config *config,
                            GtkWidget *window)
{
    if (!gtk_widget_is_drawable(w_graph))
        return;

    if (!style)
        update_theme(window);

    // KHÔNG gọi list_filter_graph_enabled nữa

    GraphDrawingContext *ctx = create_graph_context(graph_enabled_sensors, config, w_graph);
    if (!ctx)
    {
        return;
    }

    draw_graph_background_and_labels(cr, config, ctx);
    draw_value_labels(cr, ctx);

    free_graph_context(ctx);
}

void redraw_curves_only(cairo_surface_t *surface, cairo_t *cr,
                        const Psensor *const *graph_enabled_sensors,
                        GtkWidget *w_graph,
                        const struct config *config,
                        GtkWidget *window)
{
    if (!gtk_widget_is_drawable(w_graph))
        return;

    if (!style)
        update_theme(window);

    // KHÔNG gọi list_filter_graph_enabled nữa

    GraphDrawingContext *ctx = create_graph_context(graph_enabled_sensors, config, w_graph);
    if (!ctx)
    {
        return;
    }

    draw_sensor_curves(cr, graph_enabled_sensors, ctx, config);

    free_graph_context(ctx);
}

/* ===== HÀM MỚI: Dịch curves và vẽ thêm điểm ===== */
//////////////////////////////////////////////
/*
 * Đo và cache font metrics.
 * Chỉ cần gọi 1 lần, sau đó dùng FontMetrics để ước lượng extents.
 */
void measure_font_metrics(cairo_t *cr, FontMetrics *fm)
{
    if (fm->measured)
    {
        return;
    }

    cairo_select_font_face(cr, "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);

    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);
    fm->font_height = font_extents.height;

    // Đo chiều rộng của từng loại ký tự
    cairo_text_extents_t extents;

    cairo_text_extents(cr, "0", &extents);
    fm->digit_width = extents.width;

    cairo_text_extents(cr, ":", &extents);
    fm->colon_width = extents.width;

    cairo_text_extents(cr, "°", &extents);
    fm->degree_width = extents.width;

    fm->measured = TRUE;
}

/*
 * Ước lượng text extents từ font metrics đã cache.
 */
static void estimate_text_extents(const FontMetrics *fm,
                                  const char *str,
                                  cairo_text_extents_t *out)
{
    if (!str || !fm->measured)
    {
        memset(out, 0, sizeof(*out));
        return;
    }

    out->height = fm->font_height;
    out->width = 0;

    for (const char *p = str; *p; p++)
    {
        if (*p >= '0' && *p <= '9')
        {
            out->width += fm->digit_width;
        }
        else if (*p == ':')
        {
            out->width += fm->colon_width;
        }
        // else if (*p == '°')
        else if ((unsigned char)*p == 0xC2 && (unsigned char)*(p + 1) == 0xB0)
        {
            // UTF-8 encoding của ° là C2 B0
            out->width += fm->degree_width;
            p++; // skip byte thứ 2
        }
        else
        {
            out->width += fm->digit_width; // fallback
        }
    }
}

/*
 * Lấy measure cuối cùng hợp lệ từ sensor (ring buffer safe).
 */
double get_last_valid_value(const Psensor *s)
{
    if (!s || s->measures_count == 0)
        return UNKNOWN_DOUBLE_VALUE;

    int skip = is_smooth_curves_enabled ? 2 : 0;
    struct measure_iterator it_rev;
    measure_iterator_init_reverse(&it_rev, s);
    struct measure *m;

    while (measure_iterator_prev(&it_rev, &m))
    {
        if (m->value == UNKNOWN_DOUBLE_VALUE || !(m->time.tv_sec))
            continue;

        if (skip > 0)
        {
            skip--;
            continue;
        }

        return m->value;
    }

    return UNKNOWN_DOUBLE_VALUE;
}

/*
 * Tính fixed range cho trục Y, tối thiểu 20°C.
 */
void calculate_fixed_plot_range(const Psensor *const *graph_enabled_sensors,
                                double *out_min,
                                double *out_max)
{
    if (!graph_enabled_sensors || !graph_enabled_sensors[0])
    {
        *out_min = 0;
        *out_max = 100;
        return;
    }

    ALL_MINMAX all_minmax = get_all_minmax_value(graph_enabled_sensors);

    double data_min = all_minmax.temp.min;
    double data_max = all_minmax.temp.max;
    double range = data_max - data_min;

    if (range < 20.0)
        range = 20.0;
}

/*
 * Tính pixels_per_point.
 */
double calculate_pixels_per_point(const struct config *cfg, int plot_width)
{
    unsigned int duration_seconds = cfg->graph_monitoring_duration * 60U;
    unsigned int update_interval = cfg->sensor_update_interval;

    if (update_interval == 0)
        update_interval = 1;

    if (duration_seconds == 0)
        duration_seconds = 60;

    double total_points = (double)duration_seconds / (double)update_interval;

    if (total_points <= 0)
        return 1.0;

    double ret = (double)plot_width / total_points;
    g_print("|calculatePixelsPerPoint=%1.0f|", ret);
    return ret;
}

/*
 * Tính min_shift_pixels từ DPI (1mm).
 */
int calculate_min_shift_pixels(GtkWidget *widget)
{
    GdkScreen *screen = gtk_widget_get_screen(widget);
    double dpi = gdk_screen_get_resolution(screen);

    if (dpi < 0)
        dpi = 96.0;

    double pixels_per_mm = dpi / 25.4;
    int min_pixels = (int)(pixels_per_mm * 1.0 + 0.5);

    if (min_pixels < 1)
        min_pixels = 1;

    g_print("|dpi=%1.0f minPixels=%d|", dpi, min_pixels);
    return min_pixels;
}

/*
 * Vẽ plot background lên surface.
 * Surface này chỉ chứa vùng plot (nền + grid lines).
 * KHÔNG chứa labels.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 */
void draw_plot_background(cairo_surface_t *surface,
                          cairo_t *cr,
                          const Psensor *const *graph_enabled_sensors,
                          GtkWidget *w_graph,
                          const struct config *config,
                          GtkWidget *window)
{
    if (!gtk_widget_is_drawable(w_graph))
        return;

    if (!style)
        update_theme(window);

    // Tạo context để lấy layout và min/max
    GraphDrawingContext *ctx = create_graph_context(graph_enabled_sensors,
                                                    config, w_graph);
    if (!ctx)
        return;

    // Vẽ nền plot
    const struct color *bgcolor = config->graph_bgcolor;

    if (config->alpha_channel_enabled)
    {
        cairo_set_source_rgba(cr,
                              theme_bg_color.red,
                              theme_bg_color.green,
                              theme_bg_color.blue,
                              config->graph_bg_alpha);
    }
    else
    {
        cairo_set_source_rgb(cr,
                             theme_bg_color.red,
                             theme_bg_color.green,
                             theme_bg_color.blue);
    }

    cairo_rectangle(cr, 0, 0,
                    ctx->layout.plot_width + 1,
                    ctx->layout.plot_height + 1);
    cairo_fill(cr);

    if (config->alpha_channel_enabled)
    {
        cairo_set_source_rgba(cr,
                              bgcolor->red,
                              bgcolor->green,
                              bgcolor->blue,
                              config->graph_bg_alpha);
    }
    else
    {
        cairo_set_source_rgb(cr,
                             bgcolor->red,
                             bgcolor->green,
                             bgcolor->blue);
    }

    cairo_rectangle(cr, 0, 0,
                    ctx->layout.plot_width,
                    ctx->layout.plot_height);
    cairo_fill(cr);

    // Dùng fixed range thay vì all_minmax
    double grid_min, grid_max;
    calculate_fixed_plot_range(graph_enabled_sensors, &grid_min, &grid_max);

    draw_background_lines(cr, (int)grid_min, (int)grid_max, config, &ctx->layout);

    free_graph_context(ctx);
}

/*
 * Vẽ curves lên surface (chỉ vùng plot, không có labels, không có nền).
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 */
void draw_curves_only(cairo_surface_t *surface,
                      cairo_t *cr,
                      const Psensor *const *graph_enabled_sensors,
                      GtkWidget *w_graph,
                      const struct config *config,
                      GtkWidget *window,
                      double fixed_min,
                      double fixed_max)
{
    if (!gtk_widget_is_drawable(w_graph))
        return;

    if (!style)
        update_theme(window);

    GraphDrawingContext *ctx = create_graph_context(graph_enabled_sensors,
                                                    config, w_graph);
    if (!ctx)
        return;

    // QUAN TRỌNG: Surface không có margin, plot_x = 0
    ctx->layout.plot_x = 0;
    ctx->layout.plot_y = 0;
    ctx->layout.plot_width = cairo_image_surface_get_width(surface);
    ctx->layout.plot_height = cairo_image_surface_get_height(surface);

    ctx->all_minmax.temp.min = fixed_min;
    ctx->all_minmax.temp.max = fixed_max;

    draw_sensor_curves(cr, graph_enabled_sensors, ctx, config);
    free_graph_context(ctx);
}

void draw_curves_only2(cairo_surface_t *surface,
                       cairo_t *cr,
                       const Psensor *const *graph_enabled_sensors,
                       GtkWidget *w_graph,
                       const struct config *config,
                       GtkWidget *window,
                       double fixed_min, // THÊM
                       double fixed_max) // THÊM
{
    if (!gtk_widget_is_drawable(w_graph))
        return;

    if (!style)
        update_theme(window);

    GraphDrawingContext *ctx = create_graph_context(graph_enabled_sensors,
                                                    config, w_graph);
    if (!ctx)
        return;

    // GHI ĐÈ min/max bằng fixed range
    ctx->all_minmax.temp.min = fixed_min;
    ctx->all_minmax.temp.max = fixed_max;

    draw_sensor_curves(cr, graph_enabled_sensors, ctx, config);
    free_graph_context(ctx);
}

void draw_curves_only1(cairo_surface_t *surface,
                       cairo_t *cr,
                       const Psensor *const *graph_enabled_sensors,
                       GtkWidget *w_graph,
                       const struct config *config,
                       GtkWidget *window)
{
    if (!gtk_widget_is_drawable(w_graph))
        return;

    if (!style)
        update_theme(window);

    GraphDrawingContext *ctx = create_graph_context(graph_enabled_sensors,
                                                    config, w_graph);
    if (!ctx)
        return;

    draw_sensor_curves(cr, graph_enabled_sensors, ctx, config);

    free_graph_context(ctx);
}

/*
 * Vẽ left labels (min, max, unit) lên surface nhỏ.
 * Surface kích thước vừa đủ cho text.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * font_metrics: font metrics đã cache để ước lượng extents.
 * out_str_*: output, caller có trách nhiệm free().
 */
void draw_left_labels(cairo_surface_t *surface,
                      cairo_t *cr,
                      const Psensor *const *graph_enabled_sensors,
                      const struct config *config,
                      GtkWidget *window,
                      const FontMetrics *font_metrics,
                      char **out_str_min,
                      char **out_str_max,
                      char **out_str_unit)
{
    if (!style)
        update_theme(window);

    Temperature_Unit temperature_unit = config_get_temperature_unit();

    // Tính fixed range
    double fixed_min, fixed_max;
    calculate_fixed_plot_range(graph_enabled_sensors, &fixed_min, &fixed_max);

    // Format labels
    char *str_max = psensor_value_to_str(SENSOR_TYPE_TEMP,
                                         fixed_max,
                                         temperature_unit);
    char *str_min = psensor_value_to_str(SENSOR_TYPE_TEMP,
                                         fixed_min,
                                         temperature_unit);
    char *str_unit = psensor_unit_to_str(SENSOR_TYPE_TEMP, temperature_unit);

    // Xuất ra cho caller cache
    if (out_str_max)
        free(*out_str_max);
    *out_str_max = strdup(str_max);

    if (out_str_min)
        free(*out_str_min);
    *out_str_min = strdup(str_min);

    if (out_str_unit)
        free(*out_str_unit);
    *out_str_unit = strdup(str_unit);

    // Chọn font
    cairo_select_font_face(cr, "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);

    // Màu text
    cairo_set_source_rgb(cr, theme_fg_color.red,
                         theme_fg_color.green,
                         theme_fg_color.blue);

    // Ước lượng extents từ font metrics cache
    cairo_text_extents_t extents_max, extents_min, extents_unit;
    estimate_text_extents(font_metrics, str_max, &extents_max);
    estimate_text_extents(font_metrics, str_min, &extents_min);
    estimate_text_extents(font_metrics, str_unit, &extents_unit);

    double max_width = extents_max.width;
    if (extents_min.width > max_width)
        max_width = extents_min.width;

    if (extents_unit.width > max_width)
        max_width = extents_unit.width;

    double line_height = font_metrics->font_height;

    // Vẽ max value (top)
    cairo_move_to(cr, 0, line_height);
    cairo_show_text(cr, str_max);

    // Vẽ unit dưới max
    cairo_move_to(cr, 0, 2 * line_height);
    cairo_show_text(cr, str_unit);

    // Vẽ min value (bottom area)
    // Vị trí bottom của surface
    int surf_height = cairo_image_surface_get_height(surface);
    cairo_move_to(cr, 0, surf_height - (2 * line_height));
    cairo_show_text(cr, str_min);

    // Vẽ unit dưới min
    cairo_move_to(cr, 0, surf_height - line_height);
    cairo_show_text(cr, str_unit);

    free(str_max);
    free(str_min);
    free(str_unit);
}

/*
 * Vẽ bottom labels (begin/end time) lên surface nhỏ.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * font_metrics: font metrics đã cache.
 * out_str_*: output, caller có trách nhiệm free().
 */
void draw_bottom_labels(cairo_surface_t *surface,
                        cairo_t *cr,
                        const Psensor *const *graph_enabled_sensors,
                        const struct config *config,
                        GtkWidget *window,
                        const FontMetrics *font_metrics,
                        char **out_str_btime,
                        char **out_str_etime)
{
    if (!style)
    {
        update_theme(window);
    }

    // Lấy thời gian
    time_t end_time = get_graph_end_time_s(graph_enabled_sensors);
    time_t begin_time = get_graph_begin_time_s(config, end_time);

    char *str_btime = time_to_str(begin_time);
    char *str_etime = time_to_str(end_time);

    if (out_str_btime)
    {
        free(*out_str_btime);
        *out_str_btime = strdup(str_btime);
    }
    if (out_str_etime)
    {
        free(*out_str_etime);
        *out_str_etime = strdup(str_etime);
    }

    // Chọn font
    cairo_select_font_face(cr, "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);

    // Màu text
    cairo_set_source_rgb(cr, theme_fg_color.red,
                         theme_fg_color.green,
                         theme_fg_color.blue);

    // Ước lượng extents
    cairo_text_extents_t extents_etime;
    estimate_text_extents(font_metrics, str_etime, &extents_etime);

    int surf_width = cairo_image_surface_get_width(surface);
    int surf_height = cairo_image_surface_get_height(surface);

    // Begin time (left)
    cairo_move_to(cr, 0, surf_height - GRAPH_V_PADDING);
    cairo_show_text(cr, str_btime);

    // End time (right)
    cairo_move_to(cr, surf_width - extents_etime.width - GRAPH_H_PADDING,
                  surf_height - GRAPH_V_PADDING);
    cairo_show_text(cr, str_etime);

    free(str_btime);
    free(str_etime);
}

/*
 * Dịch toàn bộ graph_surface sang trái shift_pixels pixel,
 * xóa vùng trống bên phải, rồi vẽ data mới nhất của mỗi sensor.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * last_values_buffer: buffer lưu giá trị cuối cùng của mỗi sensor lần trước.
 * shift_pixels: số pixel cần dịch.
 * Lưu ý: hàm này KHÔNG lock/unlock, KHÔNG gọi list_filter_graph_enabled.
 */
void graph_shift_and_append(cairo_surface_t *graph_surface,
                            const Psensor *const *graph_enabled_sensors,
                            GtkWidget *w_graph,
                            const struct config *config,
                            GtkWidget *window,
                            double *last_values_buffer,
                            size_t buffer_size,
                            int shift_pixels,
                            int plot_height,
                            double fixed_min, // TRUYỀN TỪ WD
                            double fixed_max)
{
    if (!graph_surface || !graph_enabled_sensors || !w_graph)
    {
        return;
    }

    int width = cairo_image_surface_get_width(graph_surface);
    int height = cairo_image_surface_get_height(graph_surface);

    if (width <= 1 || height <= 1)
    {
        return;
    }

    if (shift_pixels < 1)
    {
        shift_pixels = 1;
    }
    if (shift_pixels >= width)
    {
        shift_pixels = width - 1;
    }

    // === BƯỚC 1: Dịch surface sang trái ===
    cairo_surface_t *temp_surface = cairo_surface_create_similar(
        graph_surface,
        CAIRO_CONTENT_COLOR_ALPHA,
        width, height);

    cairo_t *temp_cr = cairo_create(temp_surface);

    cairo_set_source_surface(temp_cr, graph_surface, -shift_pixels, 0);
    cairo_paint(temp_cr);

    // === BƯỚC 2: Xóa vùng bên phải ===
    cairo_set_operator(temp_cr, CAIRO_OPERATOR_CLEAR);
    cairo_rectangle(temp_cr,
                    width - shift_pixels, 0,
                    shift_pixels, height);
    cairo_fill(temp_cr);
    cairo_set_operator(temp_cr, CAIRO_OPERATOR_OVER);

    // === BƯỚC 3: Vẽ data mới ===
    cairo_set_line_width(temp_cr, 1.0);

    // Vị trí cố định
    // double new_data_x = width - 1;
    // double old_x = new_data_x - shift_pixels;

    double new_data_x = width; // Không phải width - 1
    double old_x = new_data_x - shift_pixels;

    // // Tính fixed range
    // double fixed_min, fixed_max;
    // calculate_fixed_plot_range(graph_enabled_sensors, &fixed_min, &fixed_max);

    size_t sensor_idx = 0;
    const Psensor *const *sensor_ptr = graph_enabled_sensors;

    while (*sensor_ptr && sensor_idx < buffer_size)
    {
        const Psensor *s = *sensor_ptr;

        if (s->measures_count > 0)
        {
            double current_value = get_last_valid_value(s);

            gboolean is_temperature = (s->type & SENSOR_TYPE_TEMP) != 0;
            if (is_temperature && current_value <= 0.1)
            {
                sensor_ptr++;
                sensor_idx++;
                continue;
            }

            if (current_value != UNKNOWN_DOUBLE_VALUE)
            {
                // Với shift đơn giản, ta chỉ cần fixed_min/fixed_max cho temperature
                // RPM và percent vẫn cần all_minmax, nhưng có thể lấy từ ctx nếu cần
                double min_val = fixed_min;
                double max_val = fixed_max;

                // TODO: Nếu sensor là RPM hoặc percent, cần xử lý riêng
                // Nhưng hiện tại CPU là temperature nên tạm ổn

                double new_y = compute_y(current_value,
                                         min_val, max_val,
                                         plot_height,
                                         0);

                GdkRGBA *color = config_get_sensor_color(s->id);
                if (color)
                {
                    cairo_set_source_rgb(temp_cr,
                                         color->red,
                                         color->green,
                                         color->blue);
                    gdk_rgba_free(color);
                }

                // Vẽ điểm mới
                cairo_arc(temp_cr, new_data_x, new_y, 1.5, 0, 2 * M_PI);
                cairo_fill(temp_cr);

                // Nối với điểm cũ nếu có
                if (last_values_buffer[sensor_idx] != UNKNOWN_DOUBLE_VALUE)
                {
                    double old_y = compute_y(last_values_buffer[sensor_idx],
                                             min_val, max_val,
                                             plot_height,
                                             0);

                    if (old_x >= 0)
                    {
                        cairo_move_to(temp_cr, old_x, old_y);
                        cairo_line_to(temp_cr, new_data_x, new_y);
                        cairo_stroke(temp_cr);
                    }

                    g_print("|line:(%.0f,%.0f)=>(%.0f,%.0f) val=%.0f shift=%d fixed=(%.0f,%.0f)|",
                            old_x, old_y,
                            new_data_x, new_y,
                            current_value,
                            shift_pixels,
                            fixed_min, fixed_max);
                }

                last_values_buffer[sensor_idx] = current_value;
            }
        }

        sensor_ptr++;
        sensor_idx++;
    }

    cairo_destroy(temp_cr);

    // === BƯỚC 4: Copy kết quả ===
    cairo_t *cr = cairo_create(graph_surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_surface(cr, temp_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    cairo_surface_destroy(temp_surface);
}
