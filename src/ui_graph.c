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
#include <pmutex.h>

#include "graph.h"
#include "ui_graph.h"
#include <ptime.h>

/*
 * Kiểm tra xem app có đang hiển thị để cập nhật UI hay không.
 * Trả về TRUE nếu cửa sổ đang visible, active, và không bị iconified.
 */
gboolean should_update_ui(struct ui_psensor *ui)
{
    GtkWindow *window = GTK_WINDOW(ui->main_window);

    if (!window || !gtk_widget_get_visible(GTK_WIDGET(window)))
        return FALSE;

    // KHÔNG kiểm tra active, để vẫn cập nhật background
    if (!gtk_window_is_active(window))
        return FALSE;

    GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
    if (gdk_window)
    {
        GdkWindowState state = gdk_window_get_state(gdk_window);
        if (state & GDK_WINDOW_STATE_ICONIFIED)
            return FALSE;
    }

    return TRUE;
}

/*
 * Xử lý sự kiện click chuột phải lên đồ thị để hiển thị popup menu.
 */
static int
on_graph_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    if (event->type != GDK_BUTTON_PRESS || event->button != 3)
        return FALSE;
#if ENABLE_DEBUG_PRINT
    time_t mytimenow = time(nullptr);
    char *mynow = time_to_str3(&mytimenow);
    char *mynow2 = time_to_str2(&mytimenow);
    DEBUG_PRINT("show popup menu %s|%s threadID=%ld\n", mynow, mynow2, gettid());
    free(mynow2);
    free(mynow);
#endif
    gtk_menu_popup_at_pointer(GTK_MENU(((struct ui_psensor *)data)->popup_menu),
                              (const GdkEvent *)event);

    return TRUE;
}

static int draw_callback_lock(pthread_mutex_t *m)
{
    return pmutex_lock(m);
}

static int draw_callback_unlock(pthread_mutex_t *m)
{
    return pmutex_unlock(m);
}

static void draw_background(GraphContext *ctx, const Pconfig *cfg)
{
    if (!ctx->background_valid)
    {
        cairo_t *bg_cr = cairo_create(ctx->grid_surface);
        cairo_set_operator(bg_cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(bg_cr);
        cairo_set_operator(bg_cr, CAIRO_OPERATOR_OVER);
        draw_plot_background(ctx, bg_cr, cfg);
        cairo_destroy(bg_cr);
        ctx->background_valid = TRUE;
    }
}

static void draw_empty_graph(UI_psensor *ui, cairo_t *cr)
{
    draw_callback_unlock(&ui->sensors_mutex);

    Pconfig *cfg = ui->config;
    const GraphContext *ctx = &cfg->graph_ctx;
    cairo_set_source_rgb(cr, ctx->theme_bg_color.red, ctx->theme_bg_color.green, ctx->theme_bg_color.blue);

    cairo_paint(cr);
}

// === HELPER FUNCTIONS ===
static const Psensor **list_filter_graph_enabled(UI_psensor *ui)
{
    if (!ui || !ui->sensors)
        return nullptr;

    static guint local_version = 0;

    pthread_mutex_lock(&ui->graph_mutex);

    if (ui->graph_cache == nullptr || local_version != ui->graph_version)
    {
        if (ui->graph_cache)
        {
            free((void *)ui->graph_cache);
            ui->graph_cache = nullptr;
        }

        const size_t n = psensor_list_size((const Psensor *const *)ui->sensors);
        ui->graph_cache = (const Psensor **)calloc(n + 1, sizeof(Psensor *));
        if (!ui->graph_cache)
        {
            pthread_mutex_unlock(&ui->graph_mutex);
            return nullptr;
        }

        const Psensor *const *cur = (const Psensor *const *)ui->sensors;
        size_t i = 0;
        for (; i < n && *cur; ++cur)
        {
            const Psensor *s = *cur;
            if (config_is_sensor_graph_enabled(s->id))
            {
                ui->graph_cache[i] = s;
                ++i;
            }
        }
        ui->graph_cache[i] = nullptr;
        local_version = ui->graph_version;
    }

    pthread_mutex_unlock(&ui->graph_mutex);
    return ui->graph_cache;
}

void graph_cache_invalidate(UI_psensor *ui)
{
    pthread_mutex_lock(&ui->graph_mutex);
    ui->graph_version++;
    pthread_mutex_unlock(&ui->graph_mutex);
}

static void clear_surface_to_transparent(cairo_surface_t *surface)
{
    cairo_t *cr = cairo_create(surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_destroy(cr);
}

static gboolean ensure_grid_surface(GraphContext *ctx, cairo_t *cr,
                                    gboolean size_changed)
{
    if (ctx->grid_surface && !size_changed)
        return TRUE;

    if (ctx->grid_surface)
        cairo_surface_destroy(ctx->grid_surface);

    ctx->grid_surface = cairo_surface_create_similar(
        cairo_get_target(cr), CAIRO_CONTENT_COLOR_ALPHA,
        ctx->plot_width, ctx->plot_height);
    clear_surface_to_transparent(ctx->grid_surface);

    ctx->grid_surface_width = ctx->plot_width;
    ctx->grid_surface_height = ctx->plot_height;

    ctx->background_valid = FALSE;

    return (ctx->grid_surface != nullptr);
}

static gboolean ensure_graph_surface(GraphContext *ctx, cairo_t *cr,
                                     gboolean size_changed)
{
    if (ctx->graph_surface && !size_changed)
        return TRUE;

    if (ctx->graph_surface)
        cairo_surface_destroy(ctx->graph_surface);

    int w = ctx->plot_width;
    int h = ctx->plot_height - (2 * ctx->v_padding);

    ctx->graph_surface = cairo_surface_create_similar(
        cairo_get_target(cr), CAIRO_CONTENT_COLOR_ALPHA, w, h);
    clear_surface_to_transparent(ctx->graph_surface);

    /* Cache dimensions to avoid cairo_image_surface_get_width/height
     * which may return 0 on non‑image backends (e.g. Docker, Xlib) */
    ctx->graph_surface_width = w;
    ctx->graph_surface_height = h;

    ctx->cache_valid = FALSE;
    // ctx->pixels_per_point = 0;

    return (ctx->graph_surface != nullptr);
}

static gboolean ensure_minmax_labels_surface(GraphContext *ctx, cairo_t *cr,
                                             gboolean size_changed)
{
    if (ctx->minmax_labels_surface && !size_changed)
        return TRUE;

    int w = (int)((ctx->font_metrics.digit_width * ctx->max_unit_chars) + ctx->h_padding);
    ctx->minmax_labels_width = w;
    int h = ctx->plot_height;

    if (ctx->minmax_labels_surface)
        cairo_surface_destroy(ctx->minmax_labels_surface);

    ctx->minmax_labels_surface = cairo_surface_create_similar(
        cairo_get_target(cr), CAIRO_CONTENT_COLOR_ALPHA, w, h);
    clear_surface_to_transparent(ctx->minmax_labels_surface);

    ctx->minmax_labels_surface_width = w;
    ctx->minmax_labels_surface_height = h;

    ctx->minmax_labels_valid = FALSE;

    return (ctx->minmax_labels_surface != nullptr);
}

static gboolean ensure_time_labels_surface(GraphContext *ctx, cairo_t *cr,
                                           int full_width, gboolean size_changed)
{
    if (ctx->time_labels_surface && !size_changed)
        return TRUE;

    int w = full_width;
    int h = (int)(ctx->font_metrics.font_height + (2 * ctx->v_padding));

    if (ctx->time_labels_surface)
        cairo_surface_destroy(ctx->time_labels_surface);

    ctx->time_labels_surface = cairo_surface_create_similar(
        cairo_get_target(cr), CAIRO_CONTENT_COLOR_ALPHA, w, h);
    clear_surface_to_transparent(ctx->time_labels_surface);

    ctx->time_labels_surface_width = w;
    ctx->time_labels_surface_height = h;

    ctx->time_labels_valid = FALSE;

    return (ctx->time_labels_surface != nullptr);
}

/*
 * Vẽ lại bottom labels nếu cần (mỗi phút).
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static void update_time_labels_if_needed(struct ui_psensor *ui,
                                         GraphContext *graph_ctx,
                                         const Psensor *const *graph_enabled_sensors)
{
    if (!graph_ctx->time_labels_surface)
    {
        // g_print("update_bottom_labels_if_needed exit 1\n");
        return;
    }

    Pconfig *cfg = ui->config;

    graph_ctx->end_time = get_graph_end_time_s(graph_enabled_sensors, cfg->is_smooth_curves_enabled);
    graph_ctx->begin_time = get_graph_begin_time_s(ui->config, graph_ctx->end_time);

    // Kiểm tra xem có cần cập nhật không (mỗi phút)
    time_t now = time(nullptr);
    static time_t last_bottom_update = 0;

    if (!graph_ctx->time_labels_valid)
    {
        // g_print("update_bottom_labels_if_needed exit 2\n");
    }
    else if ((now - last_bottom_update) < 60)
    {
        // g_print("update_bottom_labels_if_needed exit 3\n");
        return;
    }

    last_bottom_update = now;

    cairo_t *beginend_time_cairo = cairo_create(graph_ctx->time_labels_surface);

    cairo_set_operator(beginend_time_cairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(beginend_time_cairo);
    cairo_set_operator(beginend_time_cairo, CAIRO_OPERATOR_OVER);

    draw_bottom_labels(graph_ctx,
                       beginend_time_cairo,
                       ui->config,
                       ui->main_window);

    cairo_destroy(beginend_time_cairo);
}

/*
 * Khởi tạo last_values buffer.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static void init_last_values_once(GraphContext *graph_ctx,
                                  const Psensor *const *graph_enabled_sensors)
{
    if (graph_ctx->last_values_initialized)
        return;

    size_t count = 0;
    while (graph_enabled_sensors[count])
        count++;

    graph_ctx->last_sensors_count = count;

    graph_ctx->last_values_initialized = TRUE;
}

/*
 * Vẽ lại toàn bộ curves từ đầu.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static void redraw_graph_full(const UI_psensor *ui,
                              GraphContext *ctx,
                              const Psensor *const *graph_enabled_sensors)
{
    cairo_t *graph_cr = cairo_create(ctx->graph_surface);
    cairo_set_operator(graph_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(graph_cr);
    cairo_set_operator(graph_cr, CAIRO_OPERATOR_OVER);

    const Pconfig *cfg = ui->config;
    draw_curves_only(ctx, graph_cr, graph_enabled_sensors, cfg->is_smooth_curves_enabled);

    cairo_destroy(graph_cr);
}

/*
 * Thông tin cho mỗi loại sensor: con trỏ đến min/max trong ALL_MINMAX,
 * con trỏ đến min/max/initialized trong DisplayRange, margin, tên.
 */
typedef struct
{
    double *all_min;
    double *all_max;
    double *display_min;
    double *display_max;
    gboolean *initialized;
    double margin;
    char *name;
} RangeChecker;

static gboolean check_single_range(RangeChecker *rc, gboolean *changed)
{
    if (!rc->all_min || !rc->all_max)
        return FALSE;

    double current_min = *rc->all_min;
    double current_max = *rc->all_max;

    if (!*rc->initialized)
    {
        DEBUG_PRINT("|%sRangeInitialized %g:%g|\n", rc->name,
                    *rc->display_min, *rc->display_max);
        return FALSE;
    }

    if (current_min < *rc->display_min + rc->margin ||
        current_max > *rc->display_max - rc->margin)
    {
        DEBUG_PRINT("|%sRangeChanged old=%g:%g current=%g:%g|\n", rc->name,
                    *rc->display_min, *rc->display_max,
                    current_min, current_max);
        *changed = TRUE;
        return TRUE;
    }

    return FALSE;
}

typedef struct
{
    double all_min, all_max;
    double *display_min;
    double *display_max;
    gboolean *initialized;
    double min_range;
    double fallback_min, fallback_max;
} RangeCalculator;

static void calc_single_range(RangeCalculator *rc)
{
    if (rc->all_min > rc->all_max)
    {
        *rc->display_min = rc->fallback_min;
        *rc->display_max = rc->fallback_max;
    }
    else
    {
        double range = rc->all_max - rc->all_min;
        if (range < rc->min_range)
        {
            double half = rc->min_range / 2.0;
            *rc->display_min = rc->all_min - half;
            *rc->display_max = rc->all_min + half;
        }
        else
        {
            *rc->display_min = rc->all_min;
            *rc->display_max = rc->all_max;
        }
    }
    *rc->initialized = TRUE;
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static void calculate_display_range(GraphContext *ctx)
{
    RangeCalculator calcs[] = {
        {ctx->all_minmax.temperature.min, ctx->all_minmax.temperature.max,
         &ctx->display_range.temp_min, &ctx->display_range.temp_max,
         &ctx->display_range.temp_initialized, MIN_TEMPERATURE_RANGE, 0.0, 100.0},

        {ctx->all_minmax.revolutions_per_minute.min, ctx->all_minmax.revolutions_per_minute.max,
         &ctx->display_range.rpm_min, &ctx->display_range.rpm_max,
         &ctx->display_range.rpm_initialized, MIN_RPM_RANGE, 0.0, 3000.0},

        {ctx->all_minmax.percent.min, ctx->all_minmax.percent.max,
         &ctx->display_range.percent_min, &ctx->display_range.percent_max,
         &ctx->display_range.percent_initialized, 0.0, 0.0, 100.0},
    };

    for (size_t i = 0; i < ARRAY_SIZE(calcs); i++)
        calc_single_range(&calcs[i]);
}

/*
 * Quét sensor types đang hiển thị, kiểm tra từng loại.
 */
static gboolean has_plot_range_changed(GraphContext *ctx,
                                       const Psensor *const *graph_enabled_sensors)
{
    /* Quét sensor types */
    gboolean has_temp = FALSE, has_rpm = FALSE, has_percent = FALSE;
    for (const Psensor *const *s = graph_enabled_sensors; *s; s++)
    {
        PsensorType t = (*s)->type;
        if (t & SENSOR_TYPE_TEMP)
            has_temp = TRUE;
        if (t & SENSOR_TYPE_RPM)
            has_rpm = TRUE;
        if (t & SENSOR_TYPE_PERCENT)
            has_percent = TRUE;
    }

    gboolean changed = FALSE;
    gboolean any_uninitialized = FALSE;

    RangeChecker checkers[] = {
        {has_temp ? &ctx->all_minmax.temperature.min : nullptr,
         has_temp ? &ctx->all_minmax.temperature.max : nullptr,
         &ctx->display_range.temp_min, &ctx->display_range.temp_max,
         &ctx->display_range.temp_initialized, 0.0, "temperature"},

        {has_rpm ? &ctx->all_minmax.revolutions_per_minute.min : nullptr,
         has_rpm ? &ctx->all_minmax.revolutions_per_minute.max : nullptr,
         &ctx->display_range.rpm_min, &ctx->display_range.rpm_max,
         &ctx->display_range.rpm_initialized, 0.0, "rpm"},

        {has_percent ? &ctx->all_minmax.percent.min : nullptr,
         has_percent ? &ctx->all_minmax.percent.max : nullptr,
         &ctx->display_range.percent_min, &ctx->display_range.percent_max,
         &ctx->display_range.percent_initialized, 0.0, "percent"},
    };

    for (size_t i = 0; i < ARRAY_SIZE(checkers); i++)
    {
        if (checkers[i].all_min == nullptr)
            continue; // Loại sensor này không có trong danh sách

        if (!*checkers[i].initialized)
        {
            any_uninitialized = TRUE;
            continue;
        }

        check_single_range(&checkers[i], &changed);
    }

    /* Nếu có thay đổi hoặc có loại chưa khởi tạo → tính lại */
    if (changed || any_uninitialized)
    {
        calculate_display_range(ctx);
        /* Sau khi tính, gán last_display_range để shift dùng */
        ctx->last_display_range = ctx->display_range;
    }

    return changed;
}

/*
 * Vẽ lại left labels nếu cần.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static void update_minmax_labels_if_needed(struct ui_psensor *ui,
                                           GraphContext *graph_ctx)
{
    if (!graph_ctx->minmax_labels_surface)
        return;

    if (graph_ctx->minmax_labels_valid)
        return;

    cairo_t *minmax_value_cairo = cairo_create(graph_ctx->minmax_labels_surface);

    cairo_set_operator(minmax_value_cairo, CAIRO_OPERATOR_CLEAR);
    cairo_paint(minmax_value_cairo);
    cairo_set_operator(minmax_value_cairo, CAIRO_OPERATOR_OVER);

    draw_left_labels(graph_ctx,
                     minmax_value_cairo,
                     ui->config,
                     ui->main_window);

    cairo_destroy(minmax_value_cairo);
    graph_ctx->minmax_labels_valid = TRUE;
}

/*
 * Thử dịch đồ thị và vẽ data mới.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static gboolean try_shift_graph(const UI_psensor *ui,
                                GraphContext *ctx,
                                GtkWidget *widget,
                                const Psensor *const *graph_enabled_sensors)
{
    graph_shift_and_append(ctx,
                           graph_enabled_sensors,
                           ui->config,
                           ctx->shift_pixels);

    return TRUE;
}

/*
 * Composite tất cả các layer lên cr của widget.
 */
static void composite_layers(cairo_t *cr, GraphContext *ctx,
                             int full_width, int full_height)
{
    // 1. Left region
    cairo_set_source_rgb(cr,
                         ctx->theme_bg_color.red,
                         ctx->theme_bg_color.green,
                         ctx->theme_bg_color.blue);

    cairo_rectangle(cr, 0, 0, ctx->plot_x, full_height);
    cairo_fill(cr);

    // 2. Right region
    cairo_rectangle(cr,
                    ctx->plot_x + ctx->plot_width, 0,
                    full_width - ctx->plot_x - ctx->plot_width, full_height);
    cairo_fill(cr);

    // 3. Grid
    // if (ctx->grid_surface)
    // {
    cairo_set_source_surface(cr, ctx->grid_surface,
                             ctx->plot_x, ctx->plot_y);
    cairo_paint(cr);
    // }

    // 4. Graph
    // if (ctx->graph_surface)
    // {
    cairo_set_source_surface(cr, ctx->graph_surface,
                             ctx->plot_x, ctx->plot_y + ctx->v_padding);
    cairo_paint(cr);
    //}

    // 5. Minmax labels
    // if (ctx->minmax_labels_surface)
    // {
    cairo_set_source_surface(cr, ctx->minmax_labels_surface,
                             ctx->h_padding, ctx->plot_y);
    cairo_paint(cr);
    // }

    // 6. Time labels
    // if (ctx->time_labels_surface)
    // {
    cairo_set_source_surface(cr, ctx->time_labels_surface,
                             0, ctx->plot_y + ctx->plot_height + ctx->v_padding);
    cairo_paint(cr);
    // }
}

/*
 * Tính layout (plot_x, plot_y, plot_width, plot_height).
 * Dùng font metrics đã cache để ước lượng extents.
 */
static void calculate_plot_area_layout(
    const UI_psensor *ui,
    GraphContext *ctx,
    GtkWidget *widget,
    int full_width,
    int full_height)
{
    const FontMetrics *fm = &ctx->font_metrics;

    double max_label_width = (fm->digit_width * ctx->max_unit_chars) + ctx->h_padding;
    double time_label_height = fm->font_height + ctx->v_padding;

    ctx->plot_x = (int)((2 * ctx->h_padding) + max_label_width);
    ctx->plot_y = ctx->v_padding;
    ctx->plot_width = full_width - ctx->plot_x - ctx->h_padding;
    ctx->plot_height = full_height - ctx->plot_y - time_label_height;

    if (ctx->plot_width < 1)
        ctx->plot_width = 1;
    if (ctx->plot_height < 1)
        ctx->plot_height = 1;

    ctx->layout_valid = TRUE;

    //
    ctx->graph_surface_width = ctx->plot_width;
    ctx->graph_surface_height = ctx->plot_height - (2 * ctx->v_padding);
    //
    int width = ctx->graph_surface_width;

    if (ctx->min_shift_pixels <= 0)
        ctx->min_shift_pixels = (double)calculate_min_shift_pixels(widget);

    if (ctx->pixels_per_point <= 0)
        ctx->pixels_per_point = calculate_pixels_per_point(ui->config, width);

    int shift_pixels = (int)(ctx->pixels_per_point + 0.5);
    if (shift_pixels < (int)ctx->min_shift_pixels)
        shift_pixels = (int)ctx->min_shift_pixels;

    if (shift_pixels < 1)
        shift_pixels = 1;

    if (shift_pixels >= width)
        shift_pixels = width - 1;
    ctx->shift_pixels = shift_pixels;
}

// === MAIN CALLBACK ===
static gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
#if ENABLE_DEBUG_PRINT
    static uint64_t count = 0;
    time_t mytimenow = time(nullptr);
    char *mynow = time_to_str3(&mytimenow);
    char *mynow2 = time_to_str2(&mytimenow);
    DEBUG_PRINT("%3lu|%s|%s theadID=%ld ", count, mynow, mynow2, gettid());
    free(mynow2);
    free(mynow);
    ++count;
#endif

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    if (allocation.width <= 0 || allocation.height <= 0)
    {
        DEBUG_PRINT("allocate = 0\n");
        return TRUE;
    }

    UI_psensor *ui = (UI_psensor *)user_data;
    Pconfig *cfg = ui->config;
    GraphContext *ctx = &cfg->graph_ctx;

    gboolean size_changed = (ctx->last_width != allocation.width || ctx->last_height != allocation.height);

    if (size_changed)
    {
        DEBUG_PRINT("RESIZE=%d old=%dx%d new=%dx%d ",
                    size_changed,
                    ctx->last_width, ctx->last_height,
                    allocation.width, allocation.height);
    }

    // if (!size_changed && !cfg->is_new_data)
    // {
    //     DEBUG_PRINT("no change size and no new data\n");
    //     return TRUE;
    // }

    draw_callback_lock(&ui->sensors_mutex);

    DEBUG_PRINT("cacheValid=%d newData=%d skipdraw=%d ",
                ctx->cache_valid,
                cfg->is_new_data, get_skipped_draw(ctx));

    const Psensor **graph_enabled_sensors =
        list_filter_graph_enabled(ui);

    if (!graph_enabled_sensors || !graph_enabled_sensors[0])
    {
        draw_empty_graph(ui, cr);
        // DEBUG_PRINT("no graph enabled sensor\n");
        return TRUE;
    }

    if (ctx->last_width == 0)
    {
        graph_context_init(ctx, widget);
        update_theme(ctx);
        measure_font_metrics_once(cr, ctx);
        init_last_values_once(ctx, graph_enabled_sensors);
    }

    if (size_changed || !ctx->layout_valid)
    {
        calculate_plot_area_layout(ui, ctx, widget, allocation.width, allocation.height);
        if (ctx->plot_width <= 1 || ctx->plot_height <= 1)
        {
            draw_empty_graph(ui, cr);
            DEBUG_PRINT("plot width or height <= 1\n");
            return TRUE;
        }
    }

    if (!ensure_grid_surface(ctx, cr, size_changed))
        return TRUE;

    if (!ensure_graph_surface(ctx, cr, size_changed))
        return TRUE;

    if (!ensure_minmax_labels_surface(ctx, cr, size_changed))
        return TRUE;

    if (!ensure_time_labels_surface(ctx, cr, allocation.width, size_changed))
        return TRUE;

    ctx->all_minmax = get_all_minmax_values(graph_enabled_sensors);

    if (has_plot_range_changed(ctx, graph_enabled_sensors))
    {
        ctx->cache_valid = FALSE;
        ctx->background_valid = FALSE;
        ctx->minmax_labels_valid = FALSE;
    }

    update_minmax_labels_if_needed(ui, ctx);
    update_time_labels_if_needed(ui, ctx, graph_enabled_sensors);
    draw_background(ctx, cfg);
    DEBUG_PRINT("|PPP=%g min=%g shift=%d WxH=%dx%d|",
                // ctx->cache_valid, cfg->is_new_data,
                ctx->pixels_per_point,
                ctx->min_shift_pixels,
                ctx->shift_pixels,
                ctx->graph_surface_width, ctx->graph_surface_height);
    if (size_changed || !ctx->cache_valid)
    {
        DEBUG_PRINT(">>REDRAWFULL ");
        redraw_graph_full(ui, ctx, graph_enabled_sensors);
        ctx->last_display_range = ctx->display_range;
        ui->config->is_new_data = false;
        ctx->cache_valid = TRUE;

        ctx->minmax_labels_valid = FALSE;
        ctx->time_labels_valid = FALSE;
        reset_skipped_draw(ctx);
    }
    else if (cfg->is_new_data)
    {
        DEBUG_PRINT(">>SHIFTGRAPH ");
        // if (get_skipped_draw(ctx) == 0)
        //     increase_skipped_draw(ctx);

        try_shift_graph(ui, ctx, widget, graph_enabled_sensors);
        ctx->last_display_range = ctx->display_range;
        ui->config->is_new_data = false;
        reset_skipped_draw(ctx);
    }

    draw_callback_unlock(&ui->sensors_mutex);

    if (size_changed)
    {
        ctx->last_width = allocation.width;
        ctx->last_height = allocation.height;
    }

    composite_layers(cr, ctx, allocation.width, allocation.height);
    return TRUE;
}

static void smooth_curves_enabled_changed_cbk(void *user_data)
{
    UI_psensor *ui = (UI_psensor *)user_data;
    Pconfig *cfg = ui->config;
    cfg->is_smooth_curves_enabled = config_is_smooth_curves_enabled();
}

void ui_graph_create(UI_psensor *sensor_context)
{
    log_debug("ui_graph_create()");

    GtkWidget *w_graph = ui_get_graph_widget();

    UI_psensor *ui = sensor_context;
    Pconfig *cfg = ui->config;
    cfg->is_smooth_curves_enabled = config_is_smooth_curves_enabled();
    g_signal_connect_after(config_get_GSettings(),
                           "changed::graph-smooth-curves-enabled",
                           G_CALLBACK(smooth_curves_enabled_changed_cbk),
                           sensor_context);

    g_signal_connect(GTK_WIDGET(w_graph),
                     "draw",
                     G_CALLBACK(draw_callback),
                     sensor_context);

    gtk_widget_add_events(w_graph, GDK_BUTTON_PRESS_MASK);

    g_signal_connect(GTK_WIDGET(w_graph),
                     "button_press_event",
                     (GCallback)on_graph_clicked, sensor_context);

    log_debug("ui_graph_create() ends");
}

static void free_resource(void **pointer_to_resource)
{
    void *resource = *pointer_to_resource;
    if (resource)
    {
        free(resource);
        resource = nullptr;
    }
}

static void free_cairo_resource(void **pointer_to_resource)
{
    void *resource = *pointer_to_resource;
    if (resource)
    {
        cairo_surface_destroy(resource);
        resource = nullptr;
    }
}

void ui_graph_cleanup(struct ui_psensor *ui_psensor)
{
    GraphContext *ctx = &ui_psensor->config->graph_ctx;

    free_cairo_resource((void **)&ctx->time_labels_surface);
    free_cairo_resource((void **)&ctx->minmax_labels_surface);
    free_cairo_resource((void **)&ctx->graph_surface);
    free_cairo_resource((void **)&ctx->grid_surface);

    free_resource((void **)&ctx->cached_str_min);
    free_resource((void **)&ctx->cached_str_max);
    free_resource((void **)&ctx->cached_str_unit);
    free_resource((void **)&ctx->cached_str_btime);
    free_resource((void **)&ctx->cached_str_etime);
    free_resource((void **)&ui_psensor->graph_cache);
}
