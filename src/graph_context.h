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
    int last_width, last_height;
    gboolean cache_valid, background_valid;

    gboolean minmax_labels_valid, time_labels_valid;
    GtkWidget *window;
    int shift_pixels;
    gboolean layout_valid;
    cairo_surface_t *graph_surface;
    cairo_surface_t *grid_surface;
    cairo_surface_t *minmax_labels_surface;
    cairo_surface_t *time_labels_surface;
    // === DPI-based constants ===
    int v_padding;
    double line_width;

    // === Layout cache ===
    int plot_x, plot_y, plot_width, plot_height;

    // === Surfaces + dimensions ===
    int graph_surface_width, graph_surface_height;
    int grid_surface_width, grid_surface_height;
    int minmax_labels_surface_width, minmax_labels_surface_height;
    int time_labels_surface_width, time_labels_surface_height;

    // === Data cache ===
    ALL_MINMAX all_minmax;
    DisplayRange display_range;      // Range hiển thị hiện tại
    DisplayRange last_display_range; // Range đã dùng để vẽ lần trước (cho shift khớp)
    time_t begin_time, end_time;
    size_t last_sensors_count;

    double font_size;
    GdkRGBA theme_fg_color;
    GdkRGBA theme_bg_color;
    int h_padding;
    // === Label state ===
    char str_min[PSENSOR_MAX_VALUE_LEN];
    char str_max[PSENSOR_MAX_VALUE_LEN];
    char str_unit[UNIT_STR_MAX_LEN];
    char str_btime[TIME_STR_MAX_LEN];
    char str_etime[TIME_STR_MAX_LEN];
    int minmax_labels_width;

    GtkStyleContext *style;
    // === Shift state ===
    int pixels_per_point;

    gboolean last_values_initialized;

    int min_shift_pixels;

    // === Cache ===

    SKIPPED_REDRAWS_DATATYPE skipped_redraws;
    size_t last_measures_count;

    int max_unit_chars;
    gboolean theme_valid;
    // === Font ===
    FontMetrics font_metrics;

    double min_temp_range;
} GraphContext;

void graph_context_init(GraphContext *ctx, GtkWidget *widget);
void graph_context_cleanup(GraphContext *ctx);

void increase_skipped_draw(GraphContext *ctx);
void reset_skipped_draw(GraphContext *ctx);
SKIPPED_REDRAWS_DATATYPE get_skipped_draw(const GraphContext *ctx);

#endif
