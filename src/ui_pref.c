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
#include <ui_pref.h>

#include <stdlib.h>
#include <string.h>
#include <pmutex.h>

#include <amd.h>
#include <cfg.h>
#include <graph.h>
#include <hdd.h>
#include <lmsensor.h>
#include <nvidia.h>
#include <pgtop2.h>
#include <pudisks2.h>
#include <pxdg.h>
#include <ui.h>
#include <ui_color.h>

#include <ui_unity.h>

void ui_pref_decoration_toggled_cbk(GtkToggleButton *btn, gpointer data)
{
    config_set_window_decoration_enabled(0 == gtk_toggle_button_get_active(btn));
}

void ui_pref_keep_below_toggled_cbk(GtkToggleButton *btn, gpointer data)
{
    config_set_window_keep_below_enabled(
        gboolean_to_bool(
            gtk_toggle_button_get_active(btn)));
}

void ui_pref_temperature_unit_changed_cbk(GtkComboBox *combo, gpointer data)
{
    config_set_temperature_unit(
        to_Temperature_Unit(
            gtk_combo_box_get_active(combo)));
}

void ui_pref_menu_toggled_cbk(GtkToggleButton *btn, gpointer data)
{
    config_set_menu_bar_enabled(
        gboolean_to_bool(
            gtk_toggle_button_get_active(btn)));
}

void ui_pref_count_visible_toggled_cbk(GtkToggleButton *btn, gpointer data)
{
    config_set_count_visible(
        gboolean_to_bool(
            gtk_toggle_button_get_active(btn)));
}

void ui_pref_sensorlist_position_changed_cbk(GtkComboBox *combo, gpointer data)
{
    config_set_sensorlist_position(
        to_sensorlist_position(
            gtk_combo_box_get_active(combo)));
}

GdkRGBA color_to_GdkRGBA(const struct color *color)
{
    GdkRGBA c;

    c.red = color->red;
    c.green = color->green;
    c.blue = color->blue;
    c.alpha = 1.0;

    return c;
}
static int ui_pref_dialog_run_lock(pthread_mutex_t *m)
{
    return pmutex_lock(m);
}
static int ui_pref_dialog_run_unlock(pthread_mutex_t *m)
{
    return pmutex_unlock(m);
}

// === STRUCT: Provider config ===
typedef struct
{
    const char *widget_name;
    bool (*is_supported)(void); // Hàm check support trả về gboolean
    bool (*is_enabled)(void);   // Hàm check enabled trả về char
    void (*set_enabled)(bool);  // Hàm set enabled nhận char
} ProviderConfig;
// Providers - using pattern
static const ProviderConfig providers[] = {
    {"lmsensors", lmsensor_is_supported, config_is_lmsensor_enabled, config_set_lmsensor_enable},
    {"nvctrl", nvidia_is_supported, config_is_nvctrl_enabled, config_set_nvctrl_enable},
    {"atiadlsdk", amd_is_supported, config_is_atiadlsdk_enabled, config_set_atiadlsdk_enable},
    {"gtop2", gtop2_is_supported, config_is_gtop2_enabled, config_set_gtop2_enable},
    {"libatasmart", atasmart_is_supported, config_is_libatasmart_enabled, config_set_libatasmart_enable},
    {"udisks2", udisks2_is_supported, config_is_udisks2_enabled, config_set_udisks2_enable},
    {nullptr, nullptr, nullptr, nullptr}};
// === HELPER: Setup toggle button với support check ===
static void setup_provider_toggle(GtkBuilder *builder, const ProviderConfig *p)
{
    GtkToggleButton *toggle = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, p->widget_name));
    gtk_toggle_button_set_active(toggle, bool_to_gboolean(p->is_enabled()));

    if (p->is_supported())
    {
        gtk_widget_set_has_tooltip(GTK_WIDGET(toggle), FALSE);
    }
    else
    {
        gtk_widget_set_sensitive(GTK_WIDGET(toggle), 0);
        gtk_widget_set_has_tooltip(GTK_WIDGET(toggle), TRUE);
    }
}

// === HELPER: Setup toggle button đơn giản ===
static void setup_toggle(GtkBuilder *builder, const char *name, gboolean active)
{
    GtkToggleButton *toggle = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, name));
    gtk_toggle_button_set_active(toggle, active);
}

// === HELPER: Setup spin button ===
static void setup_spin_button(GtkBuilder *builder, const char *name, gdouble value)
{
    GtkSpinButton *spin = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, name));
    gtk_spin_button_set_value(spin, value);
}

// === HELPER: Setup color chooser ===
static void setup_color_chooser(GtkBuilder *builder, const char *name, const Pcolor *color)
{
    GdkRGBA rgba = color_to_GdkRGBA(color);
    GtkColorChooser *chooser = GTK_COLOR_CHOOSER(gtk_builder_get_object(builder, name));
    gtk_color_chooser_set_rgba(chooser, &rgba);
}

// === HELPER: Save color from chooser ===
static void save_color_from_chooser(GtkBuilder *builder, const char *name, struct color *dest)
{
    GdkRGBA color;
    GtkColorChooser *chooser = GTK_COLOR_CHOOSER(gtk_builder_get_object(builder, name));
    gtk_color_chooser_get_rgba(chooser, &color);
    color_set(dest, color.red, color.green, color.blue);
}

// // === HELPER: Save toggle state ===
// static void save_toggle_state(GtkBuilder *builder, const char *name,
//                                void (*setter)(char)) {  // ĐÚNG - nhận char
//     GtkToggleButton *toggle = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, name));
//     setter(gboolean_to_bool(gtk_toggle_button_get_active(toggle)));
// }

// === HELPER: Save spin button value with min ===
static gint save_spin_button_value(GtkBuilder *builder, const char *name, gint min_val)
{
    GtkSpinButton *spin = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, name));
    gint value = gtk_spin_button_get_value_as_int(spin);
    return (value > min_val) ? value : min_val;
}

static void save_provider_toggle(GtkBuilder *builder, const ProviderConfig *p)
{
    GtkToggleButton *toggle = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, p->widget_name));
    p->set_enabled(gboolean_to_bool(gtk_toggle_button_get_active(toggle)));
}

// === SETUP FUNCTION ===
static void setup_pref_dialog_widgets(GtkBuilder *builder, const struct config *cfg)
{
    // Notification script
    GtkEntry *w_notif_script = GTK_ENTRY(gtk_builder_get_object(builder, "notif_script"));
    char *notif_script = config_get_notif_script();
    if (notif_script)
    {
        gtk_entry_set_text(w_notif_script, notif_script);
        free(notif_script);
    }

    // Colors
    setup_color_chooser(builder, "color_fg", &cfg->graph_fgcolor);
    setup_color_chooser(builder, "color_bg", &cfg->graph_bgcolor);

    // Opacity
    GtkScale *w_bg_opacity = GTK_SCALE(gtk_builder_get_object(builder, "bg_opacity"));
    gtk_range_set_value(GTK_RANGE(w_bg_opacity), cfg->graph_bg_alpha);

    // Spin buttons
    setup_spin_button(builder, "update_interval", cfg->graph_update_interval);
    setup_spin_button(builder, "sensor_update_interval", cfg->sensor_update_interval);
    setup_spin_button(builder, "monitoring_duration", cfg->graph_monitoring_duration);
    setup_spin_button(builder, "slog_interval", cfg->slog_interval);

    // Sensor list position
    GtkComboBox *w_sensorlist_pos = GTK_COMBO_BOX(gtk_builder_get_object(builder, "sensors_list_position"));
    gtk_combo_box_set_active(w_sensorlist_pos, sensorlist_position_to_int(config_get_sensorlist_position()));

    // Temperature unit
    GtkComboBoxText *w_temp_unit = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "temperature_unit"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(w_temp_unit),
                             Temperature_Unit_to_int(config_get_temperature_unit()));

    // Simple toggles
    setup_toggle(builder, "autostart", pxdg_is_autostarted());
    setup_toggle(builder, "enable_menu", bool_to_gboolean(config_is_menu_bar_enabled()));
    setup_toggle(builder, "graph_smooth_curves", bool_to_gboolean(config_is_smooth_curves_enabled()));
    setup_toggle(builder, "enable_slog", bool_to_gboolean(cfg->slog_enabled));
    setup_toggle(builder, "hide_on_startup", bool_to_gboolean(cfg->hide_on_startup));
    setup_toggle(builder, "restore_window", bool_to_gboolean(cfg->window_restore_enabled));
    setup_toggle(builder, "hide_window_decoration", !config_is_window_decoration_enabled());
    setup_toggle(builder, "keep_window_below", bool_to_gboolean(config_is_window_keep_below_enabled()));
    setup_toggle(builder, "hddtemp", bool_to_gboolean(config_is_hddtemp_enabled()));

    // Unity-specific toggle
    GtkToggleButton *w_enable_launcher_counter = GTK_TOGGLE_BUTTON(
        gtk_builder_get_object(builder, "enable_launcher_counter"));
    gtk_toggle_button_set_active(w_enable_launcher_counter,
                                 bool_to_gboolean(config_is_count_visible()));

    if (ui_unity_is_supported())
    {
        gtk_widget_set_has_tooltip(GTK_WIDGET(w_enable_launcher_counter), FALSE);
    }
    else
    {
        gtk_widget_set_sensitive(GTK_WIDGET(w_enable_launcher_counter), FALSE);
        gtk_widget_set_has_tooltip(GTK_WIDGET(w_enable_launcher_counter), TRUE);
    }

    for (const ProviderConfig *p = providers; p->widget_name; p++)
    {
        setup_provider_toggle(builder, p);
    }
}

// === SAVE FUNCTION ===
static void save_pref_dialog_settings(GtkBuilder *builder, struct config *cfg,
                                      struct ui_psensor *ui)
{
    double value;

    ui_pref_dialog_run_lock(&ui->sensors_mutex);

    // Notification script
    GtkEntry *w_notif_script = GTK_ENTRY(gtk_builder_get_object(builder, "notif_script"));
    config_set_notif_script(gtk_entry_get_text(w_notif_script));

    // Colors
    save_color_from_chooser(builder, "color_fg", &cfg->graph_fgcolor);
    save_color_from_chooser(builder, "color_bg", &cfg->graph_bgcolor);

    // Background opacity
    GtkScale *w_bg_opacity = GTK_SCALE(gtk_builder_get_object(builder, "bg_opacity"));
    value = gtk_range_get_value(GTK_RANGE(w_bg_opacity));
    cfg->graph_bg_alpha = value;
    cfg->alpha_channel_enabled = (value == 1.0) ? false : true;

    // Spin buttons with validation
    gint sensor_update_interval = save_spin_button_value(builder, "sensor_update_interval", 0);
    if (sensor_update_interval > 0)
        cfg->sensor_update_interval = (u_int32_t)sensor_update_interval;
    else
        cfg->sensor_update_interval = 1;

    gint graph_update_interval = save_spin_button_value(builder, "update_interval", 0);
    if (graph_update_interval > 0)
        cfg->graph_update_interval = (uint32_t)graph_update_interval;
    else
        cfg->graph_update_interval = 1;

    gint graph_monitoring_duration = save_spin_button_value(builder, "monitoring_duration", 0);
    if (graph_monitoring_duration > 0)
        cfg->graph_monitoring_duration = (u_int32_t)graph_monitoring_duration;
    else
        cfg->graph_monitoring_duration = 10U;

    gint slog_interval = save_spin_button_value(builder, "slog_interval", 0);
    if (slog_interval > 0)
        cfg->slog_interval = (u_int32_t)slog_interval;
    else
        cfg->slog_interval = 1;

    // Simple toggles
    cfg->hide_on_startup = gboolean_to_bool(gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "hide_on_startup"))));
    cfg->window_restore_enabled = gboolean_to_bool(gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "restore_window"))));
    cfg->slog_enabled = gboolean_to_bool(gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "enable_slog"))));

    cfg->sensor_values_max_length = compute_values_max_length(cfg);
    config_save_to_g_file(cfg);

    // Provider toggles
    for (const ProviderConfig *p = providers; p->widget_name; p++)
    {
        save_provider_toggle(builder, p);
    }

    // Autostart
    GtkToggleButton *w_autostart = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "autostart"));
    pxdg_set_autostart(gtk_toggle_button_get_active(w_autostart));

    ui_pref_dialog_run_unlock(&ui->sensors_mutex);
    ui_window_update(ui);
}

void ui_pref_dialog_run(struct ui_psensor *ui)
{
    GtkBuilder *builder;
    GError *error = nullptr;
    struct config *cfg = ui->config;

    // Build dialog
    builder = gtk_builder_new();
    gtk_builder_set_translation_domain(builder, PACKAGE_NAME);

    if (!gtk_builder_add_from_file(builder,
                                   PACKAGE_DATA_DIR G_DIR_SEPARATOR_S G_STRINGIFY(PACKAGE_NAME_WITHOUT_QUOTE) "-pref.glade",
                                   &error))
    {
        log_printf(LOG_ERR, error->message);
        g_error_free(error);
        return;
    }

    GtkDialog *diag = GTK_DIALOG(gtk_builder_get_object(builder, "dialog1"));

    // Setup UI widgets from config
    setup_pref_dialog_widgets(builder, cfg);

    // Run dialog
    gtk_builder_connect_signals(builder, nullptr);
    gint result = gtk_dialog_run(diag);

    // Save if accepted
    if (result == GTK_RESPONSE_ACCEPT)
    {
        save_pref_dialog_settings(builder, cfg, ui);
    }

    // Cleanup
    g_object_unref(G_OBJECT(builder));
    gtk_widget_destroy(GTK_WIDGET(diag));
}
