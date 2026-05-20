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
#ifndef PSENSOR_CONFIG_H
#define PSENSOR_CONFIG_H

#include <color.h>

#include <stdint.h>
#include <gdk/gdk.h>

#include <temperature.h>

enum sensorlist_position
{
    SENSORLIST_POSITION_RIGHT = 0,
    SENSORLIST_POSITION_LEFT = 1,
    SENSORLIST_POSITION_TOP = 2,
    SENSORLIST_POSITION_BOTTOM = 3
};
const char *config_get_sensorlist_position_str(enum sensorlist_position pos);

/*
 * Font metrics đã được cache, chỉ đo 1 lần.
 */
typedef struct
{
    double font_height;
    double digit_width;
    double colon_width;
    double degree_width;
    gboolean measured;
} FontMetrics;

typedef struct
{
    // Canvas lớn (chứa toàn bộ đồ thị)
    cairo_surface_t *canvas;
    cairo_surface_t *background;

    // Các surface chính
    cairo_surface_t *graph_surface;
    cairo_surface_t *plot_bg_surface;
    cairo_surface_t *left_labels_surface;
    cairo_surface_t *btm_labels_surface;

    // Viewport
    double viewport_x;
    double viewport_width;
    double viewport_height;

    // Kích thước canvas
    double canvas_width;
    double canvas_height;

    // Dự phòng cho min/max
    double global_min;
    double global_max;
    double min_temp_range;

    // Fixed min/max cho trục đứng
    double fixed_plot_min;
    double fixed_plot_max;
    gboolean plot_range_initialized;

    // Các field hiện có
    int last_width, last_height;
    gboolean cache_valid;
    gboolean background_valid;

    int shift_count;
    double *last_values;
    size_t last_sensors_count;
    gboolean last_values_initialized;
    size_t skipped_redraws;
    time_t last_shift_time;
    time_t last_drawn_timestamp;

    // Shift và pixels
    double pixels_per_point;
    double min_shift_pixels;

    // Cache labels
    char *cached_str_min;
    char *cached_str_max;
    char *cached_str_unit;
    char *cached_str_btime;
    char *cached_str_etime;
    gboolean labels_valid;

    // Font metrics cache
    FontMetrics font_metrics;
} MyWidgetData;

typedef struct
{
    // Canvas lớn (chứa toàn bộ đồ thị)
    cairo_surface_t *canvas;     // Canvas lớn hơn viewport
    cairo_surface_t *background; // Background cố định

    cairo_surface_t *graph_surface;      // Đường cong
    cairo_surface_t *background_surface; // Nền + trục

    // Viewport (phần hiển thị ra màn hình)
    double viewport_x;      // Vị trí x của viewport trên canvas
    double viewport_width;  // Chiều rộng viewport (bằng plot_width)
    double viewport_height; // Chiều cao viewport (bằng plot_height)

    // Kích thước canvas
    double canvas_width;  // = viewport_width * 2 (hoặc lớn hơn)
    double canvas_height; // = viewport_height + margin_top + margin_bottom

    // Dự phòng cho min/max
    double global_min;     // Min tuyệt đối (có dự phòng)
    double global_max;     // Max tuyệt đối (có dự phòng)
    double min_temp_range; // Biên độ tối thiểu (mặc định 20 độ)

    // Fixed min/max cho trục đứng (có dự phòng)
    double fixed_min;
    double fixed_max;
    gboolean minmax_initialized;

    // Các field hiện có
    int last_width, last_height;
    gboolean cache_valid;
    gboolean background_valid;

    int shift_count;
    double *last_values;
    size_t last_sensors_count;
    gboolean last_values_initialized;
    size_t skipped_redraws;
    time_t last_shift_time;
    time_t last_drawn_timestamp;
    double pixel_per_point;
    int total_points;

    double pixels_per_point; // Số pixel cho mỗi data point
    double min_shift_pixels; // Ngưỡng dịch tối thiểu (từ mm)
} MyWidgetData1;

typedef struct config
{
    struct color *graph_bgcolor;
    struct color *graph_fgcolor;
    /* Last saved position of the window. */
    int window_x;
    int window_y;
    /* Last saved size of the window. */
    int window_w;
    int window_h;
    /* Last saved position of the window divider. */
    int window_vertical_divider_pos;
    int window_horizontal_divider_pos;
    uint32_t graph_update_interval;
    uint32_t graph_monitoring_duration;
    unsigned int sensor_values_max_length;
    uint32_t sensor_update_interval;
    uint32_t slog_interval;
    double graph_bg_alpha;

    MyWidgetData widget_data;
    bool alpha_channel_enabled;
    bool window_restore_enabled;
    bool slog_enabled;
    bool is_new_data;
    bool hide_on_startup;

    pthread_mutex_t graph_enabled_mutex;

} Pconfig;

/* Loads psensor configuration */
struct config *config_load(void);

void config_save_to_g_file(const struct config *);

void config_cleanup(void);

GdkRGBA *config_get_sensor_color(const char *);
void config_set_sensor_color(const char *, const GdkRGBA *);

bool config_get_sensor_alarm_high_threshold(const char *, double *);
void config_set_sensor_alarm_high_threshold(const char *, int);

bool config_get_sensor_alarm_low_threshold(const char *, double *);
void config_set_sensor_alarm_low_threshold(const char *, int);

bool config_get_sensor_alarm_enabled(const char *);
void config_set_sensor_alarm_enabled(const char *, bool);

bool config_is_sensor_graph_enabled(const char *);
void config_set_sensor_graph_enabled(const char *, bool);

char *config_get_sensor_name(const char *);
void config_set_sensor_name(const char *, const char *);

bool config_is_appindicator_enabled(const char *);
void config_set_appindicator_enabled(const char *, bool);

bool config_is_appindicator_label_enabled(const char *);
void config_set_appindicator_label_enabled(const char *, bool);

gboolean bool_to_gboolean(bool b);
char gboolean_to_char(gboolean b);
bool gboolean_to_bool(gboolean b);
enum sensorlist_position to_sensorlist_position(gint i);
int sensorlist_position_to_int(enum sensorlist_position pos);

bool is_slog_enabled(void);
void config_set_slog_enabled_changed_cbk(void (*)(void *), void *);

unsigned int config_get_slog_interval(void);

bool config_is_smooth_curves_enabled(void);
void config_set_smooth_curves_enabled(bool);

int config_get_sensor_position(const char *);
void config_set_sensor_position(const char *, int);

char *config_get_notif_script(void);
void config_set_notif_script(const char *);

bool config_is_sensor_enabled(const char *sid);
void config_set_sensor_enabled(const char *sid, bool enabled);

bool config_is_lmsensor_enabled(void);
void config_set_lmsensor_enable(bool);

bool config_is_gtop2_enabled(void);
void config_set_gtop2_enable(bool);

bool config_is_udisks2_enabled(void);
void config_set_udisks2_enable(bool);

bool config_is_hddtemp_enabled(void);
void config_set_hddtemp_enable(bool);

bool config_is_libatasmart_enabled(void);
void config_set_libatasmart_enable(bool);

bool config_is_nvctrl_enabled(void);
void config_set_nvctrl_enable(bool);

bool config_is_atiadlsdk_enabled(void);
void config_set_atiadlsdk_enable(bool);

Temperature_Unit config_get_temperature_unit(void);
void config_set_temperature_unit(enum temperature_unit);

double config_get_default_high_threshold_temperature(void);

bool config_is_window_decoration_enabled(void);
void config_set_window_decoration_enabled(bool);

bool config_is_window_keep_below_enabled(void);
void config_set_window_keep_below_enabled(bool);

bool config_is_menu_bar_enabled(void);
void config_set_menu_bar_enabled(bool);

bool config_is_count_visible(void);
void config_set_count_visible(bool);

enum sensorlist_position config_get_sensorlist_position(void);
void config_set_sensorlist_position(enum sensorlist_position pos);

/*
 * Returns the user directory containing psensor data (configuration
 * and log).
 * Corresponds to $HOME/.psensor-fork/
 * Creates the directory if it does not exist;
 * Returns NULL if it cannot be determined.
 */
const char *get_psensor_user_dir(void);

void config_sync(void);

GSettings *config_get_GSettings(void);

#endif
