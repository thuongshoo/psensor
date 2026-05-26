#include <graph_context.h>
#include <glib.h>
/*
 * Initialize the GraphContext with DPI-aware values derived from physical
 * millimeters.  All magic numbers below are design‑choice constants expressed
 * in millimetres and then converted to pixels at runtime using the monitor's
 * actual DPI.  The fallback DPI (96) corresponds to a typical 24" 1920×1080
 * monitor.
 *
 * Physical design choices:
 *   - Horizontal / vertical padding:  0.8 mm  (enough whitespace around the
 *     plot area so that text labels and grid lines never touch the widget edge)
 *   - Font size for labels:           2.5 mm  (roughly 10 pt on a 96 DPI
 *     screen; legible without wasting space)
 *   - Minimum shift per new data:     1.0 mm  (guarantees the graph scrolls
 *     by a human‑noticeable amount even when the number of data points is
 *     much larger than the available pixels; avoids sub‑pixel shifts that
 *     would cause anti‑aliasing blur)
 *   - max_unit_chars = 4:  enough for a temperature value like "100°" (3 digits
 *     + degree sign) with a small safety margin.
 *   - min_temp_range = 40.0 °C:  the Y‑axis span never shrinks below 40 °C so
 *     that minor temperature fluctuations do not constantly rescale the graph,
 *     keeping the visual scale stable.
 *
 * Theme colors (fg / bg) and the GtkStyleContext are cached here so that
 * drawing functions can access them without touching global variables.
 */
#define VERTICAL_PADDING_IN_MILLIMETER 1.0
#define HORIZONTAL_PADDING_IN_MILLIMETER 0.8

void graph_context_init(GraphContext *ctx, GtkWidget *widget)
{
    memset(ctx, 0, sizeof(*ctx));

    /* Keep a weak reference to the widget for later theme lookups --------- */
    ctx->window = widget;

    /* Obtain screen DPI --------------------------------------------------- */
    GdkScreen *screen = gtk_widget_get_screen(widget);
    double dpi = gdk_screen_get_resolution(screen);
    if (dpi < 0)
        dpi = 96.0; /* sane fallback if DPI is unknown   */

    double px_per_mm = dpi / 25.4; /* 1 inch = 25.4 mm                 */

    /* Convert design constants from mm to integer pixel values ------------ */
    /* +0.5: round to nearest integer pixel                                */
    ctx->h_padding = (int)((HORIZONTAL_PADDING_IN_MILLIMETER * px_per_mm) + 0.5);
    ctx->v_padding = (int)((VERTICAL_PADDING_IN_MILLIMETER * px_per_mm) + 0.5);
    ctx->font_size = 2.5 * px_per_mm; /* kept as double for Cairo          */
    ctx->min_shift_pixels = (int)((1.0 * px_per_mm) + 0.5);
    if (ctx->min_shift_pixels < 1)
        ctx->min_shift_pixels = 1; /* never shift 0 pixels             */

    /* Other hard‑coded constants ------------------------------------------ */
    ctx->max_unit_chars = 4;                     /* e.g. "100°"                      */
    ctx->min_temp_range = MIN_TEMPERATURE_RANGE; /* minimum Y‑axis span in °C         */

    ctx->line_width = 1.0;
}

SKIPPED_REDRAWS_DATATYPE get_skipped_draw(const GraphContext *ctx)
{
    return g_atomic_int_get(&(ctx->skipped_redraws));
}

void increase_skipped_draw(GraphContext *ctx)
{
    // g_print("beforeIncreasing:skip=%d addr=%p\n", get_skipped_draw(ctx), &(ctx->skipped_redraws));
    g_atomic_int_inc(&(ctx->skipped_redraws));
}

void reset_skipped_draw(GraphContext *ctx)
{
    g_atomic_int_set(&ctx->skipped_redraws, 0);
    // g_print("ResetSKippedDraw addr=%p\n", &(ctx->skipped_redraws));
}

// SKIPPED_REDRAWS_DATATYPE get_skipped_draw(const GraphContext *ctx)
// {
//     return ctx->skipped_redraws;
// }

// void increase_skipped_draw(GraphContext *ctx)
// {
//     // g_print("beforeIncreasing:skip=%d addr=%p\n", get_skipped_draw(ctx), &(ctx->skipped_redraws));
//     ++ctx->skipped_redraws;
// }

// void reset_skipped_draw(GraphContext *ctx)
// {
//     ctx->skipped_redraws = 0;
//     // g_print("ResetSKippedDraw addr=%p\n", &(ctx->skipped_redraws));
// }
