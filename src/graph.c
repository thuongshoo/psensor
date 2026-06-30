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

void update_theme(GraphContext *ctx)
{
    if (ctx->theme_valid)
        return;

    // Lấy style context của window (không phải graph widget)
    GtkWidget *toplevel = gtk_widget_get_toplevel(ctx->window);
    GtkStyleContext *style = gtk_widget_get_style_context(toplevel);

    gtk_style_context_get_background_color(style,
                                           GTK_STATE_FLAG_NORMAL,
                                           &ctx->theme_bg_color);
    gtk_style_context_get_color(style,
                                GTK_STATE_FLAG_NORMAL,
                                &ctx->theme_fg_color);

    // Fallback nếu vẫn đen
    if (ctx->theme_bg_color.red == 0.0 &&
        ctx->theme_bg_color.green == 0.0 &&
        ctx->theme_bg_color.blue == 0.0)
    {
        // Dùng màu mặc định GTK theme
        GdkRGBA default_bg = {0.960784, 0.960784, 0.960784, 1.0}; // #F5F5F5
        GdkRGBA default_fg = {0.172549, 0.172549, 0.180392, 1.0}; // #2C2C2E
        ctx->theme_bg_color = default_bg;
        ctx->theme_fg_color = default_fg;
    }

    ctx->theme_valid = TRUE;
}

unsigned int compute_values_max_length(const struct config *c)
{
    const unsigned int duration = c->graph_monitoring_duration * 60U; // minutes to seconds
    const unsigned int interval = c->sensor_update_interval;

    // ceil: (duration + interval/2) / interval
    unsigned int n = 6 + ((duration + (interval / 2)) / interval);

    return n;
}

time_t get_graph_end_time_s(const Psensor *const *all_sensors, bool is_smooth_curves_enabled)
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
        Pmeasure *m;
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

static double clamp_between_0_1(double normalized_x)
{
    if (normalized_x < 0.0)
        normalized_x = 0.0;
    if (normalized_x > 1.0)
        normalized_x = 1.0;

    return normalized_x;
}

static double compute_y(const double value, const double min, const double max, const double height, const double off)
{
    if (max <= min)
    {
        return (height / 2.0) + off;
    }

    const double range = max - min;
    double normalized = (value - min) / range;

    normalized = clamp_between_0_1(normalized);

    const double result = height - (height * normalized) + off;

    return result;
}

/* setup dash style */
static double dashes[] = {
    1.0, /* ink */
    2.0, /* skip */
};
static int ndash = ARRAY_SIZE(dashes);

static void draw_background_lines(cairo_t *cr, GraphContext *ctx,
                                  const struct config *config)
{
    const struct color *color = config->graph_fgcolor;
    int min = (int)ctx->display_range.temp_min;
    int max = (int)ctx->display_range.temp_max;

    cairo_set_line_width(cr, 1);
    cairo_set_dash(cr, dashes, ndash, 0);
    cairo_set_source_rgb(cr, color->red, color->green, color->blue);

    /* Vertical lines (time) — 5 lines */
    for (int i = 0; i <= 5; i++)
    {
        double x = (i * ctx->plot_width / 5.0);
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, ctx->plot_height);
    }

    /* Horizontal lines (value) — 5 lines */
    double range = (double)(max - min);

    if (range <= 0.1)
    {
        double y = ctx->plot_height / 2.0;
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, ctx->plot_width, y);
    }
    else
    {
        for (int i = 0; i <= 5; i++)
        {
            double fraction = i / 5.0;
            double value = min + (fraction * range);
            double y = compute_y(value, min, max, ctx->plot_height, 0);

            cairo_move_to(cr, 0, y);
            cairo_line_to(cr, ctx->plot_width, y);

            // /* Value label */
            // char label[32];
            // snprintf(label, sizeof(label), "%.0f", value);

            // cairo_text_extents_t extents;
            // cairo_text_extents(cr, label, &extents);

            // cairo_move_to(cr, -extents.width - 5, y + (extents.height / 2.0));
            // cairo_show_text(cr, label);
        }
    }

    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0);
}

/* ==================== SIMPLER VERSION: EVERY 4 POINTS BEZIER ==================== */
static void draw_sensor_segmented_bezier(GraphContext *ctx, const Psensor *sensor, cairo_t *cr,
                                         double min, double max,
                                         time_t begin_time, time_t end_time,
                                         double plot_x, double plot_y,
                                         double plot_width, double plot_height)
{
    /* This version draws Bezier curve for EVERY 4 points, ensuring all points are used */
    if (/*!sensor ||*/ sensor->measures_count < 4 || begin_time >= end_time)
        return;

    GdkRGBA color;
    config_get_sensor_color_into(sensor->id, &color);

    cairo_set_source_rgb(cr, color.red, color.green, color.blue);

    const double time_scale = plot_width / (double)(end_time - begin_time);
    const double value_range = (max > min) ? (max - min) : 1.0;
    // printf("%s minmax=%.1f:%.1f time=%ld:%ld:%ld \n", sensor->name,min,max, begin_time, end_time, end_time - begin_time);
    /* We'll draw as we iterate - no need to store all points */
    double segment_x[4], segment_y[4];
    int segment_idx = 0;
    bool has_started = false;

    struct measure_iterator it;
    measure_iterator_init(&it, sensor);

    Pmeasure *m;
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
        segment_x[segment_idx] = plot_x + ((double)(t - begin_time) * time_scale);

        double normalized = (v - min) / value_range;
        if (normalized < 0.0)
            normalized = 0.0;
        if (normalized > 1.0)
            normalized = 1.0;
        segment_y[segment_idx] = plot_y + ((1.0 - normalized) * plot_height);

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
            cairo_move_to(cr, segment_x[0], segment_y[0]);

        for (int i = 1; i < segment_idx; i++)
        {
            cairo_line_to(cr, segment_x[i], segment_y[i]);
        }
    }

    if (true == has_started || (segment_idx > 1))
        cairo_stroke(cr);
}

static void draw_sensor_linear_curve(GraphContext *ctx, const Psensor *s, cairo_t *cr,
                                     double min, double max,
                                     time_t begin_time, time_t ending_time,
                                     double plot_x, double plot_y,
                                     double plot_width, double plot_height)
{
    GdkRGBA color;
    config_get_sensor_color_into(s->id, &color);
    cairo_set_source_rgb(cr, color.red, color.green, color.blue);

    struct measure_iterator it;
    measure_iterator_init(&it, s);

    Pmeasure *m;
    bool first = true;
    time_t time_range = ending_time - begin_time;
    if (time_range <= 0)
        time_range = 1;

    while (measure_iterator_next(&it, &m))
    {
        if (m->value == UNKNOWN_DOUBLE_VALUE || !(m->time.tv_sec))
            continue;

        double normalized_x = (double)(m->time.tv_sec - begin_time) / (double)time_range;
        normalized_x = clamp_between_0_1(normalized_x);
        double x = (normalized_x * plot_width) + plot_x;
        double y = compute_y(m->value, min, max, plot_height, plot_y);

        // g_print("DEBUG:time=%ld|bt=%ld|et=%ld|vdt=%ld|width=%.1f|xoff=%.1f|xy=%05.2f:%05.2f minmax=%.2f:%.2f v=%.2f|\n",
        //         m->time.tv_sec,
        //         begin_time, ending_time, vdt, info->plot_width,
        //         info->plot_x,
        //         x, y, min, max, m->value);

        DEBUG_PRINT("|xy=%g:%g minmax=%g:%g v=%g|\n", x, y, min, max, m->value);

        // printf("%ld:%.2f=%05.2f:%05.2f minmax=%.2f:%.2f\n", m->time.tv_sec,m->value, x,y, min, max);

        if (!first)
        {
#if ENABLE_DEBUG_PRINT
            cairo_arc(cr,
                      x,                           // Tâm X
                      y,                           // Tâm Y
                      ctx->pixels_per_point / 3.0, // Bán kính (bằng nửa cạnh ngắn nhất)
                      0, 2 * G_PI);                // Góc từ 0 đến 2PI để có hình tròn đầy đủ
#else
            cairo_line_to(cr, x, y);
#endif
        }
        else
        {
#if ENABLE_DEBUG_PRINT
            cairo_arc(cr,
                      x,                           // Tâm X
                      y,                           // Tâm Y
                      ctx->pixels_per_point / 3.0, // Bán kính (bằng nửa cạnh ngắn nhất)
                      0, 2 * G_PI);
#else
            cairo_move_to(cr, x, y);
#endif
            first = false;
        }
    }
#if ENABLE_DEBUG_PRINT
    cairo_fill(cr);
#else
    cairo_stroke(cr);
#endif
}

static void display_no_graphs_warning(cairo_t *cr, int x, int y)
{
    const char *msg = _("No graphs enabled");

    cairo_select_font_face(cr,
                           "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 18.0);

    cairo_move_to(cr, x, y);
    cairo_show_text(cr, msg);
}

static void draw_sensor_curves(cairo_t *cr,
                               const Psensor *const *enabled_sensors,
                               GraphContext *ctx,
                               bool is_smooth_curves_enabled)
{
    if (ctx->begin_time == 0 || ctx->end_time == 0)
        return;

    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_width(cr, ctx->line_width);

    draw_sensor_curve_function_type draw_curve = is_smooth_curves_enabled
                                                     ? &draw_sensor_segmented_bezier
                                                     : &draw_sensor_linear_curve;

    const Psensor *const *sensor = enabled_sensors;
    while (*sensor)
    {
        const Psensor *s = *sensor;

        double min_val, max_val;
        if (s->type & SENSOR_TYPE_RPM)
        {
            min_val = ctx->display_range.rpm_min;
            max_val = ctx->display_range.rpm_max;
        }
        else if (s->type & SENSOR_TYPE_PERCENT)
        {
            min_val = ctx->display_range.percent_min;
            max_val = ctx->display_range.percent_max;
        }
        else
        {
            min_val = ctx->display_range.temp_min;
            max_val = ctx->display_range.temp_max;
        }

        /* Surface has no margin, plot origin is (0,0) */
        draw_curve(ctx, s, cr, min_val, max_val,
                   ctx->begin_time, ctx->end_time,
                   0, 0,
                   ctx->graph_surface_width, ctx->graph_surface_height);

        sensor++;
    }

    bool has_graphs = false;
    if (*enabled_sensors)
        has_graphs = true;

    if (!has_graphs)
        display_no_graphs_warning(cr,
                                  12,
                                  ctx->graph_surface_height / 2);
}

/* ==================== MAIN FUNCTION ==================== */

/*
 * Đo và cache font metrics.
 * Chỉ cần gọi 1 lần, sau đó dùng FontMetrics để ước lượng extents.
 */
void measure_font_metrics_once(cairo_t *cr, GraphContext *ctx)
{
    FontMetrics *fm = &ctx->font_metrics;

    if (fm->measured)
        return;

    cairo_select_font_face(cr, BEGIN_END_TIME_FONT,
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, ctx->font_size);

    cairo_font_extents_t font_extents;
    cairo_font_extents(cr, &font_extents);
    fm->font_height = font_extents.height;

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
double get_last_valid_value(const Psensor *s, bool is_smooth_curves_enabled)
{
    if /*(!s ||*/ (s->measures_count == 0)
        return UNKNOWN_DOUBLE_VALUE;

    int skip = is_smooth_curves_enabled ? 2 : 0;
    struct measure_iterator it_rev;
    measure_iterator_init_reverse(&it_rev, s);
    Pmeasure *m;

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

void calculate_fixed_plot_range(GraphContext *ctx,
                                const Psensor *const *graph_enabled_sensors,
                                double *out_min,
                                double *out_max)
{
    double data_min = ctx->all_minmax.temperature.min;
    double data_max = ctx->all_minmax.temperature.max;
    double range = data_max - data_min;

    if (range < MIN_TEMPERATURE_RANGE)
    {
        // Mở rộng range lên 20 độ, giữ nguyên min
        *out_min = data_min - HALF_RANGE;
        *out_max = data_min + HALF_RANGE;
    }
    else
    {
        *out_min = data_min;
        *out_max = data_max;
    }
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
    int min_pixels = (int)((pixels_per_mm * 1.0) + 0.5);

    if (min_pixels < 1)
        min_pixels = 1;

    return min_pixels;
}

/*
 * Vẽ plot background lên surface.
 * Surface này chỉ chứa vùng plot (nền + grid lines).
 * KHÔNG chứa labels.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 */
void draw_plot_background(GraphContext *ctx, cairo_t *cr,
                          const Pconfig *config)
{
    const struct color *bgcolor = config->graph_bgcolor;

    if (config->alpha_channel_enabled)
        cairo_set_source_rgba(cr, ctx->theme_bg_color.red,
                              ctx->theme_bg_color.green,
                              ctx->theme_bg_color.blue,
                              config->graph_bg_alpha);
    else
        cairo_set_source_rgb(cr, ctx->theme_bg_color.red,
                             ctx->theme_bg_color.green,
                             ctx->theme_bg_color.blue);

    cairo_rectangle(cr, 0, 0,
                    ctx->grid_surface_width + 1,
                    ctx->grid_surface_height + 1);
    cairo_fill(cr);

    if (config->alpha_channel_enabled)
        cairo_set_source_rgba(cr, bgcolor->red, bgcolor->green, bgcolor->blue,
                              config->graph_bg_alpha);
    else
        cairo_set_source_rgb(cr, bgcolor->red, bgcolor->green, bgcolor->blue);

    cairo_rectangle(cr, 0, 0,
                    ctx->grid_surface_width,
                    ctx->grid_surface_height);
    cairo_fill(cr);

    draw_background_lines(cr, ctx, config);
}

/*
 * Vẽ curves lên surface (chỉ vùng plot, không có labels, không có nền).
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 */

void draw_curves_only(GraphContext *ctx, cairo_t *cr,
                      const Psensor *const *graph_enabled_sensors,
                      bool is_smooth_curves_enabled)
{
    draw_sensor_curves(cr, graph_enabled_sensors, ctx, is_smooth_curves_enabled);
}
/*
 * Vẽ nhãn thời gian (begin / end time) lên surface đã cache.
 * Mọi kích thước được lấy từ GraphContext, không gọi cairo_get_width/height.
 */
void draw_bottom_labels(GraphContext *ctx,
                        cairo_t *cr,
                        const struct config *config,
                        GtkWidget *window)
{
    char *str_btime = time_to_str(ctx->begin_time);
    char *str_etime = time_to_str(ctx->end_time);

    if (ctx->cached_str_btime)
        free(ctx->cached_str_btime);
    ctx->cached_str_btime = str_btime;

    if (ctx->cached_str_etime)
        free(ctx->cached_str_etime);
    ctx->cached_str_etime = str_etime;

    cairo_select_font_face(cr, BEGIN_END_TIME_FONT,
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, ctx->font_size);

    cairo_set_source_rgb(cr, ctx->theme_fg_color.red,
                         ctx->theme_fg_color.green,
                         ctx->theme_fg_color.blue);

    cairo_text_extents_t extents_etime;
    estimate_text_extents(&ctx->font_metrics, str_etime, &extents_etime);

    int surf_width = ctx->time_labels_surface_width;
    int surf_height = ctx->time_labels_surface_height;

    double text_y = (surf_height) / 2.0;

    cairo_move_to(cr, ctx->minmax_labels_surface_width + ctx->h_padding, text_y);
    cairo_show_text(cr, str_btime);

    cairo_move_to(cr, surf_width - extents_etime.width - ctx->h_padding, text_y);
    cairo_show_text(cr, str_etime);
}

/*
 * Vẽ nhãn min / max / unit lên surface đã cache.
 * Dùng font metrics và kích thước từ GraphContext.
 */
void draw_left_labels(GraphContext *ctx,
                      cairo_t *cr,
                      const struct config *config,
                      GtkWidget *window)
{
    Temperature_Unit temperature_unit = config_get_temperature_unit();

    char *str_max = psensor_value_to_str(SENSOR_TYPE_TEMP, ctx->display_range.temp_max, temperature_unit);
    char *str_min = psensor_value_to_str(SENSOR_TYPE_TEMP, ctx->display_range.temp_min, temperature_unit);
    char *str_unit = psensor_unit_to_str(SENSOR_TYPE_TEMP, temperature_unit);

    if (ctx->cached_str_max)
        free(ctx->cached_str_max);
    ctx->cached_str_max = str_max;

    if (ctx->cached_str_min)
        free(ctx->cached_str_min);
    ctx->cached_str_min = str_min;

    if (ctx->cached_str_unit)
        free(ctx->cached_str_unit);
    ctx->cached_str_unit = str_unit;

    cairo_select_font_face(cr, BEGIN_END_TIME_FONT,
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, ctx->font_size);

    cairo_set_source_rgb(cr, ctx->theme_fg_color.red,
                         ctx->theme_fg_color.green,
                         ctx->theme_fg_color.blue);

    cairo_text_extents_t extents_max, extents_min, extents_unit;
    estimate_text_extents(&ctx->font_metrics, str_max, &extents_max);
    estimate_text_extents(&ctx->font_metrics, str_min, &extents_min);
    estimate_text_extents(&ctx->font_metrics, str_unit, &extents_unit);

    double max_width = extents_max.width;
    if (extents_min.width > max_width)
        max_width = extents_min.width;
    if (extents_unit.width > max_width)
        max_width = extents_unit.width;

    double line_height = ctx->font_metrics.font_height;
    int surf_height = ctx->minmax_labels_surface_height;

    /* Draw top‑aligned: max value + unit */
    cairo_move_to(cr, max_width / 2.0, line_height);
    cairo_show_text(cr, str_max);

    cairo_move_to(cr, max_width / 2.0, 2.0 * line_height);
    cairo_show_text(cr, str_unit);

    /* Draw bottom‑aligned: min value + unit */
    cairo_move_to(cr, max_width / 2.0, surf_height - (2.0 * line_height));
    cairo_show_text(cr, str_min);

    cairo_move_to(cr, max_width / 2.0, surf_height - line_height);
    cairo_show_text(cr, str_unit);
}

/* ── Getter: trả về min/max theo loại sensor ── */
static void display_range_get(const DisplayRange *range,
                              PsensorType type,
                              double *out_min, double *out_max)
{
    if (type & SENSOR_TYPE_RPM)
    {
        *out_min = range->rpm_min;
        *out_max = range->rpm_max;
    }
    else if (type & SENSOR_TYPE_PERCENT)
    {
        *out_min = range->percent_min;
        *out_max = range->percent_max;
    }
    else // Temperature (mặc định)
    {
        *out_min = range->temp_min;
        *out_max = range->temp_max;
    }
}

/* ── Getter tiện lợi: ctx->display_range ── */
void ctx_display_range_get(const GraphContext *ctx,
                           PsensorType type,
                           double *out_min, double *out_max)
{
    display_range_get(&ctx->display_range, type, out_min, out_max);
}

/* ── Getter tiện lợi: ctx->last_display_range ── */
void ctx_last_display_range_get(const GraphContext *ctx,
                                PsensorType type,
                                double *out_min, double *out_max)
{
    display_range_get(&ctx->last_display_range, type, out_min, out_max);
}

/*
 * Vẽ data mới nhất của mỗi sensor lên cột cuối bên phải của surface.
 * Surface đã được dịch trước đó (bởi memmove hoặc surface tạm).
 * Hàm này không cần biết surface backend là gì, chỉ dùng cairo_t.
 */
/*
 * Vẽ data mới nhất của mỗi sensor lên cột cuối bên phải của surface.
 * Surface đã được dịch trước đó (bởi memmove hoặc surface tạm).
 * Hàm này không cần biết surface backend là gì, chỉ dùng cairo_t.
 */
static void draw_new_data(GraphContext *ctx,
                          const Psensor *const *graph_enabled_sensors,
                          int shift_pixels,
                          bool is_smooth_curves_enabled,
                          int actual_skipped)
{
    cairo_surface_t *surface = ctx->graph_surface;
    int width = ctx->graph_surface_width;
    int height = ctx->graph_surface_height;

    cairo_t *cr = cairo_create(surface);
    cairo_set_line_width(cr, ctx->line_width);

    size_t sensor_idx = 0;
    const Psensor *const *sensor_ptr = graph_enabled_sensors;

    int skipped_count = actual_skipped;
    while (*sensor_ptr && sensor_idx < ctx->last_sensors_count)
    {
        const Psensor *s = *sensor_ptr;

        if (s->measures_count == 0)
        {
            DEBUG_PRINT(" noMeasure=%lu\n", sensor_idx);
            sensor_ptr++;
            sensor_idx++;
            continue;
        }
        DEBUG_PRINT("|measures_count=%zu ", s->measures_count);
#if ENABLE_DEBUG_PRINT
        for (int i = 0; i < s->measures_size; i++)
            DEBUG_PRINT("%ld ", s->measures[i].time);
#endif
        double new_min, new_max, old_min, old_max;
        ctx_display_range_get(ctx, s->type, &new_min, &new_max);
        ctx_last_display_range_get(ctx, s->type, &old_min, &old_max);

        GdkRGBA color;
        config_get_sensor_color_into(s->id, &color);
        cairo_set_source_rgb(cr, color.red, color.green, color.blue);

        int skip = is_smooth_curves_enabled ? 2 : 0;
        struct measure_iterator it_rev;
        measure_iterator_init_reverse(&it_rev, s);
        Pmeasure *m;

        /*
         * Duyệt ngược skipped_count + 1 lần:
         *   Lần 0..skipped_count-1: các điểm mới (vẽ từ phải sang trái)
         *   Lần skipped_count: điểm cũ để nối
         */
        int count = 0;
        double prev_x = 0, prev_y = 0;
        // static time_t last_time = 0;
        while (measure_iterator_prev(&it_rev, &m) && count < skipped_count + 1)
        {
            if (m->value == UNKNOWN_DOUBLE_VALUE || !(m->time.tv_sec))
            {
                DEBUG_PRINT(" skip1 ");
                continue;
            }
            if (skip > 0)
            {
                skip--;
                DEBUG_PRINT(" skip2 ");
                continue;
            }

            DEBUG_PRINT("time=%ld ", m->time.tv_sec);
            // if (m->time.tv_sec < last_time)
            //     DEBUG_PRINT("wrongTime ");
            // last_time = m->time.tv_sec;
            double x, y;
            if (count < skipped_count)
            {
                /* Điểm mới */
                x = width - (count * shift_pixels);
                y = compute_y(m->value, new_min, new_max, height, 0);
            }
            else
            {
                /* Điểm cũ để nối */
                x = width - skipped_count * shift_pixels;
                y = compute_y(m->value, old_min, old_max, height, 0);
            }

            if (count > 0)
            {

#if ENABLE_DEBUG_PRINT
                // Màu đỏ: (1.0, 0.0, 0.0)
                cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);

                cairo_arc(cr,
                          prev_x,                      // Tâm X
                          prev_y,                      // Tâm Y
                          ctx->pixels_per_point / 3.0, // Bán kính (bằng nửa cạnh ngắn nhất)
                          0, 2 * G_PI);                // Góc từ 0 đến 2PI để có hình tròn đầy đủ
                cairo_fill(cr);
                // Màu xanh lá: (0.0, 1.0, 0.0)
                cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
                cairo_arc(cr,
                          x,                           // Tâm X
                          y,                           // Tâm Y
                          ctx->pixels_per_point / 3.0, // Bán kính (bằng nửa cạnh ngắn nhất)
                          0, 2 * G_PI);                // Góc từ 0 đến 2PI để có hình tròn đầy đủ
                cairo_fill(cr);
#else
                cairo_move_to(cr, prev_x, prev_y);
                cairo_line_to(cr, x, y);
                cairo_stroke(cr);
#endif
                DEBUG_PRINT("|prev=%g:%g cur=%g:%g value=%g c=%d|\n", prev_x, prev_y, x, y, m->value, count);
            }
            else
            {
                DEBUG_PRINT(" xy=%g:%g Count=%d\n", x, y, count);
            }

            prev_x = x;
            prev_y = y;
            count++;
        }

        sensor_ptr++;
        sensor_idx++;
    }

    cairo_destroy(cr);
}

/*
 * Dịch toàn bộ graph_surface sang trái shift_pixels pixel,
 * xóa vùng trống bên phải, rồi vẽ data mới nhất của mỗi sensor.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * last_values_buffer: buffer lưu giá trị cuối cùng của mỗi sensor lần trước.
 * shift_pixels: số pixel cần dịch.
 * Lưu ý: hàm này KHÔNG lock/unlock, KHÔNG gọi list_filter_graph_enabled.
 */
void graph_shift_and_append(GraphContext *ctx,
                            const Psensor *const *graph_enabled_sensors,
                            const Pconfig *config,
                            int shift_pixels)
{
    cairo_surface_t *surface = ctx->graph_surface;
    int width = ctx->graph_surface_width;
    int height = ctx->graph_surface_height;

    int skipped_count = (int)get_skipped_draw(ctx);
    int total_columns_to_shift = shift_pixels * skipped_count;

    // CLAMP total_shift
    if (total_columns_to_shift >= width)
        total_columns_to_shift = width - 1;

    // QUAN TRỌNG: Tính lại skipped_count thực tế dựa trên total_shift đã clamp
    int actual_skipped = total_columns_to_shift / shift_pixels;
    if (actual_skipped < 1)
        actual_skipped = 1;

    DEBUG_PRINT("skippedCount/actual=%d/%d totalShift=%d  ",
                skipped_count, actual_skipped, total_columns_to_shift);

    // Shift surface với total_shift
    if (cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE)
    {
        DEBUG_PRINT("sur1\n");

        cairo_surface_flush(surface);
        unsigned char *pixels = cairo_image_surface_get_data(surface); // Con trỏ đến buffer pixel
        int row_bytes = cairo_image_surface_get_stride(surface);
        int bytes_per_pixel = 4;                                             // Bytes per pixel (ARGB32)
        int shift_bytes = total_columns_to_shift * bytes_per_pixel;          // Số byte cần dịch
        int keep_bytes = (width - total_columns_to_shift) * bytes_per_pixel; // Số byte giữ lại sau dịch
        if (pixels && row_bytes > 0)
        {
            for (int y = 0; y < height; y++)
            {
                unsigned char *row = pixels + (y * row_bytes); // Đầu hàng y
                memmove(row,                                   // Đích: đầu hàng
                        row + shift_bytes,                     // Nguồn: bỏ qua total_columns_to_shift pixel đầu
                        keep_bytes);                           // Số byte giữ lại

                memset(row + keep_bytes, // Ngay sau cột giữ lại
                       0,                // Giá trị 0 = trong suốt
                       shift_bytes);     // Số byte cần xóa
            }
            cairo_surface_mark_dirty(surface);
        }
    }
    else
    {
        DEBUG_PRINT("sur2\n");
        // fallback surface tạm
        cairo_surface_t *temp = cairo_surface_create_similar(
            surface, CAIRO_CONTENT_COLOR_ALPHA, width, height);
        cairo_t *temp_cr = cairo_create(temp);
        cairo_set_source_surface(temp_cr, surface, -total_columns_to_shift, 0);
        cairo_paint(temp_cr);
        cairo_destroy(temp_cr);

        cairo_t *cr = cairo_create(surface);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_set_source_surface(cr, temp, 0, 0);
        cairo_paint(cr);
        cairo_destroy(cr);
        cairo_surface_destroy(temp);
    }

    // Vẽ actual_skipped điểm mới (KHÔNG phải skipped_count gốc)
    draw_new_data(ctx, graph_enabled_sensors, shift_pixels,
                  config->is_smooth_curves_enabled, actual_skipped);
}
