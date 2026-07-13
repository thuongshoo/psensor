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
#include <graph_context.h>

#define BEGIN_END_TIME_FONT "sans-serif"

/*
 * Context chứa tất cả dữ liệu cần để vẽ 1 lần đồ thị.
 */
typedef struct
{
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

typedef void (*draw_sensor_curve_function_type)(
    GraphContext *ctx,
    const Psensor *sensor, cairo_t *cr,
    double min, double max,
    time_t begin_time, time_t end_time,
    double plot_x, double plot_y,
    double plot_width, double plot_height);

void graph_update(Psensor **sensors,
                  GtkWidget *w_graph,
                  Pconfig *config,
                  GtkWidget *window);

/*
 * Vẽ toàn bộ đồ thị lên surface.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 */
void redraw_graph(cairo_surface_t *graph_surface,
                  cairo_t *cr,
                  const Psensor *const *graph_enabled_sensors,
                  GtkWidget *w_graph,
                  const Pconfig *config,
                  GtkWidget *window);

/*
 * Vẽ left labels (min, max, unit) lên surface nhỏ.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * font_metrics: font metrics đã cache.
 */
void draw_left_labels(GraphContext *ctx,
                      cairo_t *cr,
                      // const Psensor *const *graph_enabled_sensors,
                      const Pconfig *config,
                      GtkWidget *window);

/*
 * Vẽ nhãn thời gian (begin / end time) lên surface đã cache.
 * Mọi kích thước được lấy từ GraphContext, không gọi cairo_get_width/height.
 */
void draw_bottom_labels(GraphContext *ctx,
                        cairo_t *cr,
                        // const Psensor *const *graph_enabled_sensors,
                        const Pconfig *config,
                        GtkWidget *window);

/*
 * Tính số pixel mỗi data point chiếm.
 */
int calculate_pixels_per_point(const Pconfig *cfg, int plot_width);

/*
 * Tính ngưỡng dịch tối thiểu từ DPI (1mm).
 */
int calculate_min_shift_pixels(GtkWidget *widget);

/*
 * Đo và cache font metrics.
 */
void measure_font_metrics_once(cairo_t *cr, GraphContext *ctx);

/*
 * Lấy measure cuối cùng hợp lệ từ sensor (ring buffer safe).
 */
double get_last_valid_value(const Psensor *s, bool is_smooth_curves_enabled);

/*
 * Lọc danh sách sensors, chỉ giữ sensor có graph_enabled = TRUE.
 */
const Psensor **list_filter_graph_enabled1(const Psensor *const *sensors);

/*
 * Tính fixed range 20°C.
 */
void calculate_fixed_plot_range(GraphContext *ctx,
                                const Psensor *const *graph_enabled_sensors,
                                double *out_min,
                                double *out_max);

unsigned int compute_values_max_length(const Pconfig *);

time_t get_graph_end_time_s(const Psensor *const *all_sensors, bool is_smooth_curves_enabled);
time_t get_graph_begin_time_s(const Pconfig *cfg, time_t etime);

/* Foreground color of the current desktop theme */
extern GdkRGBA theme_fg_color;
/* Background color of the current desktop theme */
extern GdkRGBA theme_bg_color;

void draw_plot_background(GraphContext *ctx, cairo_t *cr,
                          const Pconfig *config);

void draw_curves_only(GraphContext *ctx, cairo_t *cr,
                      const Psensor *const *graph_enabled_sensors,
                      bool is_smooth_curves_enabled);
// void draw_sensor_linear_curve(GraphContext *ctx, const Psensor *s, cairo_t *cr,
//                               double min, double max,
//                               time_t begin_time, time_t ending_time,
//                               double plot_x, double plot_y,
//                               double plot_width, double plot_height);

/*
 * Dịch graph_surface sang trái shift_pixels pixel, vẽ data mới.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 */
void graph_shift_and_append(GraphContext *ctx,
                            const Psensor *const *graph_enabled_sensors,
                            const Pconfig *config,
                            int shift_pixels);
void update_theme(GraphContext *ctx);

void ctx_display_range_get(const GraphContext *ctx,
                           PsensorType type,
                           double *out_min, double *out_max);
void ctx_last_display_range_get(const GraphContext *ctx,
                                PsensorType type,
                                double *out_min, double *out_max);

#endif
