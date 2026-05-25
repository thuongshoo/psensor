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

/*
 * Kích thước và vị trí vùng vẽ đồ thị.
 */
typedef struct graph_info
{
    double plot_x;
    double plot_y;
    double plot_width;
    double plot_height;
    double canvas_width;
    double canvas_height;
} graph_info_st;

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

void graph_update(Psensor **sensors,
                  GtkWidget *w_graph,
                  struct config *config,
                  GtkWidget *window);

/*
 * Vẽ toàn bộ đồ thị lên surface.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 */
void redraw_graph(cairo_surface_t *graph_surface,
                  cairo_t *cr,
                  const Psensor *const *graph_enabled_sensors,
                  GtkWidget *w_graph,
                  const struct config *config,
                  GtkWidget *window);

/*
 * Vẽ plot background (nền + grid lines).
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 */
void draw_plot_background(cairo_surface_t *surface,
                          cairo_t *cr,
                          const Psensor *const *graph_enabled_sensors,
                          GtkWidget *w_graph,
                          const struct config *config,
                          GtkWidget *window);

/*
 * Vẽ curves lên surface (surface chỉ chứa vùng plot, không có labels).
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 */
void draw_curves_only(cairo_surface_t *surface,
                      cairo_t *cr,
                      const Psensor *const *graph_enabled_sensors,
                      GtkWidget *w_graph,
                      const struct config *config,
                      GtkWidget *window,
                      double fixed_min, // THÊM
                      double fixed_max);
void draw_curves_only1(cairo_surface_t *surface,
                       cairo_t *cr,
                       const Psensor *const *graph_enabled_sensors,
                       GtkWidget *w_graph,
                       const struct config *config,
                       GtkWidget *window);

/*
 * Vẽ left labels (min, max, unit) lên surface nhỏ.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * font_metrics: font metrics đã cache.
 */
void draw_left_labels(cairo_surface_t *surface,
                      cairo_t *cr,
                      const Psensor *const *graph_enabled_sensors,
                      const struct config *config,
                      GtkWidget *window,
                      const FontMetrics *font_metrics,
                      char **out_str_min,
                      char **out_str_max,
                      char **out_str_unit);

/*
 * Vẽ bottom labels (begin/end time) lên surface nhỏ.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * font_metrics: font metrics đã cache.
 */
void draw_bottom_labels(cairo_surface_t *surface,
                        cairo_t *cr,
                        const Psensor *const *graph_enabled_sensors,
                        const struct config *config,
                        GtkWidget *window,
                        const FontMetrics *font_metrics,
                        char **out_str_btime,
                        char **out_str_etime);

/*
 * Dịch graph_surface sang trái shift_pixels pixel, vẽ data mới.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
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
                            double fixed_max);

void graph_shift_and_append1(cairo_surface_t *graph_surface,
                             const Psensor *const *graph_enabled_sensors,
                             GtkWidget *w_graph,
                             const struct config *config,
                             GtkWidget *window,
                             double *last_values_buffer,
                             size_t buffer_size,
                             int shift_pixels);

/*
 * Tính số pixel mỗi data point chiếm.
 */
double calculate_pixels_per_point(const struct config *cfg, int plot_width);

/*
 * Tính ngưỡng dịch tối thiểu từ DPI (1mm).
 */
int calculate_min_shift_pixels(GtkWidget *widget);

/*
 * Đo và cache font metrics.
 */
void measure_font_metrics(cairo_t *cr, FontMetrics *fm);

/*
 * Lấy measure cuối cùng hợp lệ từ sensor (ring buffer safe).
 */
double get_last_valid_value(const Psensor *s);

/*
 * Lọc danh sách sensors, chỉ giữ sensor có graph_enabled = TRUE.
 */
const Psensor **list_filter_graph_enabled(const Psensor *const *sensors);

/*
 * Tính fixed range 20°C.
 */
void calculate_fixed_plot_range(const Psensor *const *graph_enabled_sensors,
                                double *out_min,
                                double *out_max);

unsigned int compute_values_max_length(const struct config *);

time_t get_graph_end_time_s(const Psensor *const *all_sensors);
time_t get_graph_begin_time_s(const struct config *cfg, time_t etime);

/* horizontal padding */
extern const int GRAPH_H_PADDING;
/* vertical padding */
extern const int GRAPH_V_PADDING;

/* Foreground color of the current desktop theme */
extern GdkRGBA theme_fg_color;
/* Background color of the current desktop theme */
extern GdkRGBA theme_bg_color;

#endif