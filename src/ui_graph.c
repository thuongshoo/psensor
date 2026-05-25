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

    gtk_menu_popup_at_pointer(GTK_MENU(((struct ui_psensor *)data)->popup_menu),
                              (const GdkEvent *)event);

    return TRUE;
}

// === HELPER FUNCTIONS ===

static int draw_callback_lock(pthread_mutex_t *m)
{
    return pmutex_lock(m);
}

static int draw_callback_unlock(pthread_mutex_t *m)
{
    return pmutex_unlock(m);
}

static void clear_surface_to_transparent(cairo_surface_t *surface)
{
    cairo_t *cr = cairo_create(surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_destroy(cr);
}

/*
 * Đảm bảo graph_surface tồn tại và đúng kích thước.
 * Kích thước = vùng plot (không bao gồm labels).
 */
static void ensure_graph_surface(MyWidgetData *wd,
                                 cairo_t *cr,
                                 int plot_width,
                                 int plot_height,
                                 gboolean size_changed)
{
    if (wd->graph_surface && !size_changed)
        return;

    if (wd->graph_surface)
        cairo_surface_destroy(wd->graph_surface);

    // Đảm bảo kích thước hợp lệ
    if (plot_width < 1)
        plot_width = 1;

    if (plot_height < 1)
        plot_height = 1;

    wd->graph_surface = cairo_surface_create_similar(
        cairo_get_target(cr),
        CAIRO_CONTENT_COLOR_ALPHA,
        plot_width, plot_height);
    clear_surface_to_transparent(wd->graph_surface);

    wd->cache_valid = FALSE;
    wd->pixels_per_point = 0;

    g_print("ensureGraphSurface:%dx%d ", plot_width, plot_height);
}

/*
 * Đảm bảo plot_bg_surface tồn tại và đúng kích thước.
 */
static void ensure_plot_bg_surface(MyWidgetData *wd,
                                   cairo_t *cr,
                                   int plot_width,
                                   int plot_height,
                                   gboolean size_changed)
{
    if (wd->plot_bg_surface && !size_changed)
        return;

    if (wd->plot_bg_surface)
        cairo_surface_destroy(wd->plot_bg_surface);

    wd->plot_bg_surface = cairo_surface_create_similar(
        cairo_get_target(cr),
        CAIRO_CONTENT_COLOR_ALPHA,
        plot_width, plot_height);
    clear_surface_to_transparent(wd->plot_bg_surface);

    wd->background_valid = FALSE;
}

/*
 * Đảm bảo left_labels_surface tồn tại.
 * Kích thước: đủ cho text max/min/unit.
 */
static void ensure_left_labels_surface(MyWidgetData *wd,
                                       cairo_t *cr,
                                       const FontMetrics *font_metrics,
                                       int plot_height,
                                       gboolean size_changed)
{
    // Chiều rộng: đủ cho 5 ký tự (ví dụ "100°C")
    int lbl_width = (int)(font_metrics->digit_width * 6 + 10);
    int lbl_height = plot_height;

    if (wd->left_labels_surface && !size_changed)
        return;

    if (wd->left_labels_surface)
        cairo_surface_destroy(wd->left_labels_surface);

    wd->left_labels_surface = cairo_surface_create_similar(
        cairo_get_target(cr),
        CAIRO_CONTENT_COLOR_ALPHA,
        lbl_width, lbl_height);
    clear_surface_to_transparent(wd->left_labels_surface);

    wd->labels_valid = FALSE;
}

/*
 * Đảm bảo btm_labels_surface tồn tại.
 * Kích thước: đủ cho 1 dòng text time.
 */
static void ensure_btm_labels_surface(MyWidgetData *wd,
                                      cairo_t *cr,
                                      const FontMetrics *font_metrics,
                                      int full_width,
                                      gboolean size_changed)
{
    int lbl_height = (int)(font_metrics->font_height + 2 * GRAPH_V_PADDING);

    if (wd->btm_labels_surface && !size_changed)
        return;

    if (wd->btm_labels_surface)
        cairo_surface_destroy(wd->btm_labels_surface);

    wd->btm_labels_surface = cairo_surface_create_similar(
        cairo_get_target(cr),
        CAIRO_CONTENT_COLOR_ALPHA,
        full_width, lbl_height);
    clear_surface_to_transparent(wd->btm_labels_surface);

    wd->labels_valid = FALSE;
}

/*
 * Khởi tạo last_values buffer.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static void init_last_values(MyWidgetData *wd,
                             const Psensor *const *graph_enabled_sensors)
{
    if (wd->last_values_initialized)
        return;

    if (!graph_enabled_sensors || !graph_enabled_sensors[0])
    {
        wd->last_values_initialized = TRUE;
        return;
    }

    size_t count = 0;
    while (graph_enabled_sensors[count])
        count++;

    wd->last_values = (double *)calloc(count + 1, sizeof(double));
    if (wd->last_values)
    {
        wd->last_sensors_count = count;
        for (size_t i = 0; i < count; i++)
            wd->last_values[i] = UNKNOWN_DOUBLE_VALUE;
    }

    wd->last_values_initialized = TRUE;
}

/*
 * Cập nhật last_values buffer.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static void update_last_values(MyWidgetData *wd,
                               const Psensor *const *graph_enabled_sensors)
{
    if (!wd->last_values || !graph_enabled_sensors)
        return;

    for (size_t i = 0;
         i < wd->last_sensors_count && graph_enabled_sensors[i];
         i++)
    {
        wd->last_values[i] = get_last_valid_value(graph_enabled_sensors[i]);
    }
}

/*
 * Vẽ lại toàn bộ curves từ đầu.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static void redraw_graph_full(struct ui_psensor *ui,
                              MyWidgetData *wd,
                              const Psensor *const *graph_enabled_sensors)
{
    // Tính fixed range
    double fixed_min, fixed_max;
    calculate_fixed_plot_range(graph_enabled_sensors, &fixed_min, &fixed_max);

    // LUÔN cập nhật vào wd
    wd->fixed_plot_min = fixed_min;
    wd->fixed_plot_max = fixed_max;
    wd->plot_range_initialized = TRUE;

    // Vẽ curves với fixed range
    cairo_t *graph_cr = cairo_create(wd->graph_surface);
    cairo_set_operator(graph_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(graph_cr);
    cairo_set_operator(graph_cr, CAIRO_OPERATOR_OVER);

    draw_curves_only(wd->graph_surface, graph_cr,
                     graph_enabled_sensors,
                     ui_get_graph_widget(),
                     ui->config,
                     ui->main_window,
                     fixed_min, fixed_max);

    cairo_destroy(graph_cr);
    wd->cache_valid = TRUE;
    wd->shift_count = 0;
    wd->pixels_per_point = 0;
}

/*
 * Kiểm tra xem fixed range có thay đổi đáng kể không (>2°C).
 * Nếu có thì cần vẽ lại left labels và plot background.
 */
static gboolean has_plot_range_changed(MyWidgetData *wd,
                                       const Psensor *const *graph_enabled_sensors)
{
    double new_min, new_max;
    calculate_fixed_plot_range(graph_enabled_sensors, &new_min, &new_max);

    if (!wd->plot_range_initialized)
    {
        wd->fixed_plot_min = new_min;
        wd->fixed_plot_max = new_max;
        wd->plot_range_initialized = TRUE;
        return FALSE; // Lần đầu không coi là changed
    }

    if (fabs(new_min - wd->fixed_plot_min) > 2.0 || fabs(new_max - wd->fixed_plot_max) > 2.0)
    {
        wd->fixed_plot_min = new_min;
        wd->fixed_plot_max = new_max;
        return TRUE; // Changed → sẽ full redraw
    }

    return FALSE; // Không changed → dùng cache
}

/*
 * Vẽ lại left labels nếu cần.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static void update_left_labels_if_needed(struct ui_psensor *ui,
                                         MyWidgetData *wd,
                                         const Psensor *const *graph_enabled_sensors)
{
    if (!wd->left_labels_surface)
        return;

    if (wd->labels_valid) //&& !has_plot_range_changed(wd, graph_enabled_sensors))
        return;

    cairo_t *lbl_cr = cairo_create(wd->left_labels_surface);

    cairo_set_operator(lbl_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(lbl_cr);
    cairo_set_operator(lbl_cr, CAIRO_OPERATOR_OVER);

    draw_left_labels(wd->left_labels_surface,
                     lbl_cr,
                     graph_enabled_sensors,
                     ui->config,
                     ui->main_window,
                     &wd->font_metrics,
                     &wd->cached_str_min,
                     &wd->cached_str_max,
                     &wd->cached_str_unit);

    cairo_destroy(lbl_cr);
    wd->labels_valid = TRUE;
}

/*
 * Vẽ lại bottom labels nếu cần (mỗi phút).
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static void update_bottom_labels_if_needed(struct ui_psensor *ui,
                                           MyWidgetData *wd,
                                           const Psensor *const *graph_enabled_sensors)
{
    if (!wd->btm_labels_surface)
        return;

    // Kiểm tra xem có cần cập nhật không (mỗi phút)
    time_t now = time(NULL);
    static time_t last_bottom_update = 0;

    if (wd->labels_valid && (now - last_bottom_update) < 60)
        return;

    last_bottom_update = now;

    cairo_t *lbl_cr = cairo_create(wd->btm_labels_surface);

    cairo_set_operator(lbl_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(lbl_cr);
    cairo_set_operator(lbl_cr, CAIRO_OPERATOR_OVER);

    draw_bottom_labels(wd->btm_labels_surface,
                       lbl_cr,
                       graph_enabled_sensors,
                       ui->config,
                       ui->main_window,
                       &wd->font_metrics,
                       &wd->cached_str_btime,
                       &wd->cached_str_etime);

    cairo_destroy(lbl_cr);
}

/*
 * Thử dịch đồ thị và vẽ data mới.
 * graph_enabled_sensors: danh sách sensor đã lọc, chỉ đọc.
 * Lưu ý: không lock/unlock, caller phải tự lock.
 */
static gboolean try_shift_graph(struct ui_psensor *ui,
                                MyWidgetData *wd,
                                GtkWidget *widget,
                                const Psensor *const *graph_enabled_sensors)
{
    if (!wd->cache_valid || !wd->graph_surface)
    {
        g_print("exit 1 cache_valid=%d graph_surface=%d ", wd->cache_valid, !!wd->graph_surface);
        return FALSE;
    }

    int width = cairo_image_surface_get_width(wd->graph_surface);
    int height = cairo_image_surface_get_height(wd->graph_surface);

    if (width <= 1 || height <= 1)
    {
        wd->cache_valid = FALSE;
        ui->config->is_new_data = false;
        g_print("exit 2 w/d=%d/%d ", width, height);
        return FALSE;
    }

    if (wd->min_shift_pixels <= 0)
        wd->min_shift_pixels = (double)calculate_min_shift_pixels(widget);

    if (wd->pixels_per_point <= 0)
        wd->pixels_per_point = calculate_pixels_per_point(ui->config, width);

    int shift_pixels = (int)(wd->pixels_per_point + 0.5);
    if (shift_pixels < (int)wd->min_shift_pixels)
        shift_pixels = (int)wd->min_shift_pixels;

    if (shift_pixels < 1)
        shift_pixels = 1;

    if (shift_pixels >= width)
        shift_pixels = width - 1;

    g_print("|try_shift: pixelsPerPoint=%.2f minShift=%.0f shift=%d width/height=%d/%d|",
            wd->pixels_per_point,
            wd->min_shift_pixels,
            shift_pixels,
            width, height);

    // Trong try_shift_graph, lấy plot_height từ surface
    int plot_height = height; // graph_surface có height = plot_height
    // Dùng fixed range đã cache, không tính lại
    double fixed_min = wd->fixed_plot_min;
    double fixed_max = wd->fixed_plot_max;

    graph_shift_and_append(wd->graph_surface,
                           graph_enabled_sensors,
                           ui_get_graph_widget(),
                           ui->config,
                           ui->main_window,
                           wd->last_values,
                           wd->last_sensors_count,
                           shift_pixels,
                           plot_height,
                           fixed_min, // TRUYỀN TỪ WD
                           fixed_max);

    ui->config->is_new_data = false;

    wd->shift_count++;

    return TRUE;
}

/*
 * Composite tất cả các layer lên cr của widget.
 */
static void composite_layers(cairo_t *cr,
                             MyWidgetData *wd,
                             int plot_x,
                             int plot_y,
                             int plot_width,
                             int plot_height,
                             int full_width,
                             int full_height)
{
    // 1. Vẽ left region (màu nền theme)
    cairo_set_source_rgb(cr,
                         theme_bg_color.red,
                         theme_bg_color.green,
                         theme_bg_color.blue);
    cairo_rectangle(cr, 0, 0, plot_x, full_height);
    cairo_fill(cr);

    // 2. Vẽ right region
    cairo_rectangle(cr,
                    plot_x + plot_width, 0,
                    full_width - plot_x - plot_width, full_height);
    cairo_fill(cr);

    // 3. Vẽ plot background
    if (wd->plot_bg_surface)
    {
        cairo_set_source_surface(cr,
                                 wd->plot_bg_surface,
                                 plot_x, plot_y);
        cairo_paint(cr);
    }

    // 4. Vẽ graph (curves)
    if (wd->graph_surface)
    {
        cairo_set_source_surface(cr,
                                 wd->graph_surface,
                                 plot_x, plot_y);
        cairo_paint(cr);
    }

    // 5. Vẽ left labels
    if (wd->left_labels_surface)
    {
        cairo_set_source_surface(cr,
                                 wd->left_labels_surface,
                                 GRAPH_H_PADDING,
                                 plot_y);
        cairo_paint(cr);
    }

    // 6. Vẽ bottom labels
    if (wd->btm_labels_surface)
    {
        cairo_set_source_surface(cr,
                                 wd->btm_labels_surface,
                                 0,
                                 plot_y + plot_height);
        cairo_paint(cr);
    }
}

/*
 * Tính layout (plot_x, plot_y, plot_width, plot_height).
 * Dùng font metrics đã cache để ước lượng extents.
 */
#define MAX_UNIT_CHARS 5
static void calculate_layout_from_cache(const FontMetrics *font_metrics,
                                        int full_width,
                                        int full_height,
                                        const char *str_btime,
                                        const char *str_etime,
                                        int *out_plot_x,
                                        int *out_plot_y,
                                        int *out_plot_width,
                                        int *out_plot_height)
{
    // Ước lượng max value label width (5 ký tự)
    double max_label_width = font_metrics->digit_width * MAX_UNIT_CHARS + 10;

    // Ước lượng time label height
    double time_label_height = font_metrics->font_height + 2 * GRAPH_V_PADDING;

    *out_plot_x = (int)(2 * GRAPH_H_PADDING + max_label_width);
    *out_plot_y = GRAPH_V_PADDING;
    *out_plot_width = full_width - *out_plot_x - GRAPH_H_PADDING;
    *out_plot_height = full_height - *out_plot_y - time_label_height;

    if (*out_plot_width < 1)
        *out_plot_width = 1;

    if (*out_plot_height < 1)
        *out_plot_height = 1;
}

// === MAIN CALLBACK ===
gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    static uint64_t count = 0;
    g_print("%3lu ", count);
    ++count;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    // Bỏ qua nếu widget chưa được cấp phát kích thước
    if (allocation.width <= 0 || allocation.height <= 0)
        return TRUE;

    struct ui_psensor *ui = (struct ui_psensor *)user_data;
    struct config *cfg = ui->config;
    MyWidgetData *wd = &cfg->widget_data;

    gboolean size_changed = (wd->last_width != allocation.width || wd->last_height != allocation.height);

    // Đo font metrics 1 lần
    measure_font_metrics(cr, &wd->font_metrics);

    // Lấy danh sách enabled sensors - LOCK 1 LẦN DUY NHẤT
    draw_callback_lock(&ui->sensors_mutex);
    const Psensor **graph_enabled_sensors =
        list_filter_graph_enabled((const Psensor *const *)ui->sensors);

    if (!graph_enabled_sensors || !graph_enabled_sensors[0])
    {
        draw_callback_unlock(&ui->sensors_mutex);
        free((void *)graph_enabled_sensors);

        // Vẽ background trống
        cairo_set_source_rgb(cr,
                             theme_bg_color.red,
                             theme_bg_color.green,
                             theme_bg_color.blue);
        cairo_paint(cr);
        return TRUE;
    }

    // Lấy time labels để tính layout
    // time_t end_time = get_graph_end_time_s(graph_enabled_sensors);
    // time_t begin_time = get_graph_begin_time_s(cfg, end_time);
    char *str_btime = nullptr; // time_to_str(begin_time);
    char *str_etime = nullptr; // time_to_str(end_time);

    // Tính layout
    int plot_x, plot_y, plot_width, plot_height;
    calculate_layout_from_cache(&wd->font_metrics,
                                allocation.width,
                                allocation.height,
                                str_btime, str_etime,
                                &plot_x, &plot_y,
                                &plot_width, &plot_height);

    // free(str_btime);
    // free(str_etime);

    // Đảm bảo các surface
    ensure_plot_bg_surface(wd, cr, plot_width, plot_height, size_changed);
    ensure_graph_surface(wd, cr, plot_width, plot_height, size_changed);
    ensure_left_labels_surface(wd, cr, &wd->font_metrics, plot_height, size_changed);
    ensure_btm_labels_surface(wd, cr, &wd->font_metrics, allocation.width, size_changed);

    // Khởi tạo last_values
    init_last_values(wd, graph_enabled_sensors);

    // Vẽ plot background nếu cần
    if (!wd->background_valid)
    {
        cairo_t *bg_cr = cairo_create(wd->plot_bg_surface);
        cairo_set_operator(bg_cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(bg_cr);
        cairo_set_operator(bg_cr, CAIRO_OPERATOR_OVER);

        draw_plot_background(wd->plot_bg_surface,
                             bg_cr,
                             graph_enabled_sensors,
                             ui_get_graph_widget(),
                             cfg,
                             ui->main_window);

        cairo_destroy(bg_cr);
        wd->background_valid = TRUE;
    }

    // Vẽ curves
    if (size_changed || !wd->cache_valid)
    {
        g_print("|fulldraw|");
        redraw_graph_full(ui, wd, graph_enabled_sensors);
        update_last_values(wd, graph_enabled_sensors);
        wd->labels_valid = FALSE; // Buộc vẽ lại labels
    }
    else if (cfg->is_new_data)
    {
        try_shift_graph(ui, wd, widget, graph_enabled_sensors);
    }

    // Cập nhật labels
    update_left_labels_if_needed(ui, wd, graph_enabled_sensors);
    update_bottom_labels_if_needed(ui, wd, graph_enabled_sensors);

    draw_callback_unlock(&ui->sensors_mutex);
    free((void *)graph_enabled_sensors);

    // Cập nhật last_width/height
    if (size_changed)
    {
        g_print("|size_changed lastW/H=%d/%d allocW/H=%d/%d|",
                wd->last_width, wd->last_height,
                allocation.width, allocation.height);
        wd->last_width = allocation.width;
        wd->last_height = allocation.height;
    }

    // Composite
    composite_layers(cr, wd,
                     plot_x, plot_y,
                     plot_width, plot_height,
                     allocation.width, allocation.height);
    g_print("\n");
    return TRUE;
}

static void smooth_curves_enabled_changed_cbk(void *data)
{
    is_smooth_curves_enabled = config_is_smooth_curves_enabled();
}

void ui_graph_create(struct ui_psensor *sensor_context)
{
    log_debug("ui_graph_create()");

    GtkWidget *w_graph = ui_get_graph_widget();

    is_smooth_curves_enabled = config_is_smooth_curves_enabled();
    g_signal_connect_after(config_get_GSettings(),
                           "changed::graph-smooth-curves-enabled",
                           G_CALLBACK(smooth_curves_enabled_changed_cbk),
                           NULL);

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

void ui_graph_cleanup(struct ui_psensor *ui_psensor)
{
    struct config *cfg = ui_psensor->config;
    MyWidgetData *widget_data = &cfg->widget_data;

    if (widget_data->last_values)
    {
        free(widget_data->last_values);
        widget_data->last_values = NULL;
    }

    if (widget_data->plot_bg_surface)
    {
        cairo_surface_destroy(widget_data->plot_bg_surface);
        widget_data->plot_bg_surface = NULL;
    }

    if (widget_data->graph_surface)
    {
        cairo_surface_destroy(widget_data->graph_surface);
        widget_data->graph_surface = NULL;
    }

    if (widget_data->left_labels_surface)
    {
        cairo_surface_destroy(widget_data->left_labels_surface);
        widget_data->left_labels_surface = NULL;
    }

    if (widget_data->btm_labels_surface)
    {
        cairo_surface_destroy(widget_data->btm_labels_surface);
        widget_data->btm_labels_surface = NULL;
    }

    free(widget_data->cached_str_min);
    free(widget_data->cached_str_max);
    free(widget_data->cached_str_unit);
    free(widget_data->cached_str_btime);
    free(widget_data->cached_str_etime);
}