// graph_context.h - Tất cả state của đồ thị
#ifndef PSENSOR_GRAPH_CONTEXT_H
#define PSENSOR_GRAPH_CONTEXT_H

#include <gtk/gtk.h>
#include "psensor.h"
#include "ptime.h"

#define MIN_TEMPERATURE_RANGE 40.0
#define HALF_RANGE (MIN_TEMPERATURE_RANGE / 2)
/*
 * Tính display range cho tất cả loại sensor từ all_minmax đã cache.
 * Temperature: mở rộng tối thiểu MIN_TEMPERATURE_RANGE (40°C).
 * RPM: mở rộng tối thiểu MIN_RPM_RANGE (500 RPM).
 * Percent: luôn 0-100.
 */
#define MIN_RPM_RANGE 500.0
#define HALF_RPM_RANGE (MIN_RPM_RANGE / 2)

/* Font metrics cache */
typedef struct
{
    double font_height;
    double digit_width;
    double colon_width;
    double degree_width;
    gboolean measured;
} FontMetrics;

/* Graph rendering dimensions */
typedef struct graph_info
{
    double plot_x;
    double plot_y;
    double plot_width;
    double plot_height;
    double canvas_width;
    double canvas_height;
} graph_info_st;

typedef struct
{
    double temp_min, temp_max;
    double rpm_min, rpm_max;
    double percent_min, percent_max;
    gboolean temp_initialized;
    gboolean rpm_initialized;
    gboolean percent_initialized;
} DisplayRange;

#define SKIPPED_REDRAWS_DATATYPE int

/* Master context: all graph state in one place */
typedef struct
{
    // === DPI-based constants ===
    int h_padding, v_padding;
    int max_unit_chars;
    double font_size;
    double min_shift_pixels;
    double min_temp_range;
    double line_width;
    // === Theme (cached from GTK) ===
    GtkWidget *window;
    GtkStyleContext *style;
    GdkRGBA theme_fg_color;
    GdkRGBA theme_bg_color;
    gboolean theme_valid;

    // === Font ===
    FontMetrics font_metrics;

    // === Layout cache ===
    int plot_x, plot_y, plot_width, plot_height;
    gboolean layout_valid;

    // === Surfaces + dimensions ===
    cairo_surface_t *graph_surface;
    int graph_surface_width, graph_surface_height;
    cairo_surface_t *grid_surface;
    int grid_surface_width, grid_surface_height;
    cairo_surface_t *minmax_labels_surface;
    int minmax_labels_surface_width, minmax_labels_surface_height;
    cairo_surface_t *time_labels_surface;
    int time_labels_surface_width, time_labels_surface_height;

    // === Data cache ===
    ALL_MINMAX all_minmax;
    DisplayRange display_range;      // Range hiển thị hiện tại
    DisplayRange last_display_range; // Range đã dùng để vẽ lần trước (cho shift khớp)
    time_t begin_time, end_time;

    // === Label state ===
    gboolean minmax_labels_valid, time_labels_valid;
    char str_min[PSENSOR_MAX_VALUE_LEN];
    char str_max[PSENSOR_MAX_VALUE_LEN];
    char str_unit[UNIT_STR_MAX_LEN];
    char str_btime[TIME_STR_MAX_LEN];
    char str_etime[TIME_STR_MAX_LEN];
    int minmax_labels_width;

    // === Shift state ===
    double pixels_per_point;
    size_t last_sensors_count;
    gboolean last_values_initialized;
    int shift_pixels;

    // === Cache ===
    gboolean cache_valid, background_valid;
    int last_width, last_height;
    SKIPPED_REDRAWS_DATATYPE skipped_redraws;
    size_t last_measures_count;

} GraphContext;

void graph_context_init(GraphContext *ctx, GtkWidget *widget);
void graph_context_cleanup(GraphContext *ctx);

void increase_skipped_draw(GraphContext *ctx);
void reset_skipped_draw(GraphContext *ctx);
SKIPPED_REDRAWS_DATATYPE get_skipped_draw(const GraphContext *ctx);

#endif
