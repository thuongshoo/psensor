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
#include <ui_sensorpref.h>

#include <stdlib.h>

#include <cfg.h>
#include <temperature.h>
#include <ui_appindicator.h>
#include <ui_color.h>
#include <ui_pref.h>
#include <ui_sensorlist.h>
#include <ui_graph.h>

enum
{
	COL_NAME = 0,
	COL_SENSOR_PREF
};

static GtkTreeView *w_sensors_list;
static GtkDialog *w_dialog;
static GtkLabel *w_sensor_id;
static GtkLabel *w_sensor_type;
static GtkLabel *w_sensor_chipname;
static GtkLabel *w_sensor_min;
static GtkLabel *w_sensor_max;
static GtkLabel *w_sensor_low_threshold_unit;
static GtkLabel *w_sensor_high_threshold_unit;
static GtkEntry *w_sensor_name;
static GtkToggleButton *w_sensor_draw;
static GtkToggleButton *w_sensor_display;
static GtkToggleButton *w_sensor_alarm;
static GtkToggleButton *w_appindicator_enabled;
static GtkToggleButton *w_appindicator_label_enabled;
static GtkColorButton *w_sensor_color;
static GtkSpinButton *w_sensor_high_threshold;
static GtkSpinButton *w_sensor_low_threshold;
static GtkListStore *store;

/* 'true' when the notifications of field changes are due to the change
 * of the selected sensor.
 */
static bool ignore_changes;

static Psensor *get_selected_sensor(void)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	Psensor *s;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(w_sensors_list);

	s = nullptr;
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_tree_model_get(model, &iter, COL_SENSOR_PREF, &s, -1);

	return s;
}

static void apply_config(struct ui_psensor *ui)
{
	config_sync();

	ui_sensorlist_update(ui, true);
	ui_appindicator_update_menu(ui);
}

void ui_sensorpref_name_changed_cb(GtkEntry *entry, gpointer data)
{
	if (ignore_changes)
		return;

	Psensor *s = get_selected_sensor();
	if (!s)
		return;

	const gchar *str = gtk_entry_get_text(entry);

	if (0 != strcmp(str, s->name))
	{
		free(s->name);
		s->name = strdup(str);
		config_set_sensor_name(s->id, str);

		apply_config((struct ui_psensor *)data);
	}
}

void ui_sensorpref_draw_toggled_cb(GtkToggleButton *btn, gpointer data)
{
	if (ignore_changes)
		return;

	const Psensor *s = get_selected_sensor();

	if (!s)
		return;

	bool active = gtk_toggle_button_get_active(btn) == 1;
	config_set_sensor_graph_enabled(s->id, active);

	apply_config((UI_psensor *)data);

	graph_cache_invalidate((UI_psensor *)data);
}

void ui_sensorpref_display_toggled_cb(GtkToggleButton *btn, gpointer data)
{
	if (ignore_changes)
		return;

	const Psensor *s = get_selected_sensor();

	if (!s)
		return;

	gboolean active = gtk_toggle_button_get_active(btn);
	config_set_sensor_enabled(s->id, gboolean_to_bool(active));

	apply_config((struct ui_psensor *)data);
}

void ui_sensorpref_alarm_toggled_cb(GtkToggleButton *btn, gpointer data)
{
	if (ignore_changes)
		return;

	const Psensor *s = get_selected_sensor();

	if (!s)
		return;

	gboolean active = gtk_toggle_button_get_active(btn);
	config_set_sensor_alarm_enabled(s->id, gboolean_to_bool(active));

	apply_config((struct ui_psensor *)data);
}

void ui_sensorpref_appindicator_menu_toggled_cb(GtkToggleButton *btn, gpointer data)
{
	if (ignore_changes)
		return;

	const Psensor *s = get_selected_sensor();

	if (!s)
		return;

	gboolean active = gtk_toggle_button_get_active(btn);
	config_set_appindicator_enabled(s->id, gboolean_to_bool(active));

	apply_config((struct ui_psensor *)data);
}

void ui_sensorpref_appindicator_label_toggled_cb(GtkToggleButton *btn, gpointer data)
{
	if (ignore_changes)
		return;

	const Psensor *s = get_selected_sensor();

	if (!s)
		return;

	gboolean active = gtk_toggle_button_get_active(btn);
	config_set_appindicator_label_enabled(s->id, gboolean_to_bool(active));

	apply_config((struct ui_psensor *)data);
}

void ui_sensorpref_color_set_cb(GtkColorButton *widget, gpointer data)
{
	GdkRGBA color;

	if (ignore_changes)
		return;

	const Psensor *s = get_selected_sensor();
	if (!s)
		return;

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &color);
	config_set_sensor_color(s->id, &color);

	apply_config((struct ui_psensor *)data);
}

void ui_sensorpref_alarm_high_threshold_changed_cb(GtkSpinButton *btn, gpointer data)
{
	if (ignore_changes)
		return;

	Psensor *s = get_selected_sensor();
	if (!s)
		return;

	gdouble v = gtk_spin_button_get_value(btn);
	if (config_get_temperature_unit() == FAHRENHEIT)
		v = fahrenheit_to_celsius(v);

	config_set_sensor_alarm_high_threshold(s->id, (int)v);
	s->alarm_high_threshold = v;

	apply_config((struct ui_psensor *)data);
}

void ui_sensorpref_alarm_low_threshold_changed_cb(GtkSpinButton *btn, gpointer data)
{
	if (ignore_changes)
		return;

	Psensor *s = get_selected_sensor();
	if (!s)
		return;

	gdouble v = gtk_spin_button_get_value(btn);
	if (config_get_temperature_unit() == FAHRENHEIT)
		v = fahrenheit_to_celsius(v);

	config_set_sensor_alarm_low_threshold(s->id, (int)v);
	s->alarm_low_threshold = v;

	apply_config((struct ui_psensor *)data);
}

static void update_pref(Psensor *s)
{
	const char *chip;
	char *smin, *smax;

	if (!s)
		return;

	ignore_changes = true;

	gtk_label_set_text(w_sensor_id, s->id);
	gtk_label_set_text(w_sensor_type, psensor_type_to_str(s->type));
	gtk_entry_set_text(w_sensor_name, s->name);

	if (s->chip)
		chip = s->chip;
	else
		chip = _("Unknown");
	gtk_label_set_text(w_sensor_chipname, chip);

	Temperature_Unit temperature_unit = config_get_temperature_unit();

	if (s->min == UNKNOWN_DOUBLE_VALUE)
		smin = strdup(_("Unknown"));
	else
		smin = psensor_value_to_str(s->type, s->min, temperature_unit);

	gtk_label_set_text(w_sensor_min, smin);
	free(smin);

	if (s->max == UNKNOWN_DOUBLE_VALUE)
		smax = strdup(_("Unknown"));
	else
		smax = psensor_value_to_str(s->type, s->max, temperature_unit);
	gtk_label_set_text(w_sensor_max, smax);
	free(smax);

	gtk_toggle_button_set_active(w_sensor_draw,
								 bool_to_gboolean(config_is_sensor_graph_enabled(s->id)));

	gtk_toggle_button_set_active(w_sensor_display,
								 bool_to_gboolean(config_is_sensor_enabled(s->id)));

	GdkRGBA color;
	config_get_sensor_color_into(s->id, &color);
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w_sensor_color), &color);

	gtk_label_set_text(w_sensor_high_threshold_unit,
					   psensor_type_to_unit_str(s->type, temperature_unit));
	gtk_label_set_text(w_sensor_low_threshold_unit,
					   psensor_type_to_unit_str(s->type, temperature_unit));

	if (is_appindicator_supported())
	{
		gtk_widget_set_has_tooltip(GTK_WIDGET(w_appindicator_label_enabled), FALSE);
		gtk_widget_set_has_tooltip(GTK_WIDGET(w_appindicator_enabled), FALSE);
	}
	else
	{
		gtk_widget_set_sensitive(GTK_WIDGET(w_appindicator_label_enabled), FALSE);
		gtk_widget_set_has_tooltip(GTK_WIDGET(w_appindicator_label_enabled), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(w_appindicator_enabled), FALSE);
		gtk_widget_set_has_tooltip(GTK_WIDGET(w_appindicator_enabled), TRUE);
	}

	gtk_toggle_button_set_active(w_sensor_alarm,
								 bool_to_gboolean(config_get_sensor_alarm_enabled(s->id)));

	double threshold = s->alarm_high_threshold;
	if (!is_celsius(temperature_unit))
		threshold = celsius_to_fahrenheit(threshold);
	gtk_spin_button_set_value(w_sensor_high_threshold, threshold);

	threshold = s->alarm_low_threshold;
	if (!is_celsius(temperature_unit))
		threshold = celsius_to_fahrenheit(threshold);
	gtk_spin_button_set_value(w_sensor_low_threshold, threshold);

	gtk_toggle_button_set_active(w_appindicator_enabled,
								 bool_to_gboolean(config_is_appindicator_enabled(s->id)));

	gtk_toggle_button_set_active(w_appindicator_label_enabled,
								 bool_to_gboolean(config_is_appindicator_label_enabled(s->id)));

	ignore_changes = false;
}

void ui_sensorpref_tree_selection_changed_cb(GtkTreeSelection *sel, gpointer data)
{
	update_pref(get_selected_sensor());
}

static void select_sensor(Psensor *s, const Psensor **sensors)
{
	const Psensor **s_cur;
	int i;
	GtkTreePath *p;
	GtkTreeSelection *sel;

	for (s_cur = sensors, i = 0; *s_cur; s_cur++, i++)
		if (s == *s_cur)
		{
			p = gtk_tree_path_new_from_indices(i, -1);
			sel = gtk_tree_view_get_selection(w_sensors_list);
			gtk_tree_selection_select_path(sel, p);

			gtk_tree_path_free(p);

			update_pref(s);
			break;
		}
}

static void quit(void)
{
	gtk_widget_destroy(GTK_WIDGET(w_dialog));
	w_dialog = nullptr;
}

static gboolean
on_delete_event_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	quit();
	return TRUE;
}

void ui_sensorpref_close_clicked_cb(GtkButton *btn, gpointer data)
{
	quit();
}

static GtkBuilder *load_ui(struct ui_psensor *ui)
{
	GtkBuilder *builder;
	GError *error;
	guint ok;

	error = nullptr;

	builder = gtk_builder_new();
	gtk_builder_set_translation_domain(builder, PACKAGE_NAME);

	ok = gtk_builder_add_from_file(builder,
								   PACKAGE_DATA_DIR G_DIR_SEPARATOR_S G_STRINGIFY(PACKAGE_NAME_WITHOUT_QUOTE) "-edit.glade",
								   &error);

	if (!ok)
	{
		log_printf(LOG_ERR, error->message);
		g_error_free(error);
		return nullptr;
	}

	w_sensors_list = GTK_TREE_VIEW(gtk_builder_get_object(builder, "sensors_list"));
	w_dialog = GTK_DIALOG(gtk_builder_get_object(builder, "dialog1"));
	w_sensor_id = GTK_LABEL(gtk_builder_get_object(builder, "sensor_id"));
	w_sensor_type = GTK_LABEL(gtk_builder_get_object(builder, "sensor_type"));
	w_sensor_name = GTK_ENTRY(gtk_builder_get_object(builder, "sensor_name"));
	w_sensor_chipname = GTK_LABEL(gtk_builder_get_object(builder, "chip_name"));
	w_sensor_min = GTK_LABEL(gtk_builder_get_object(builder, "sensor_min"));
	w_sensor_max = GTK_LABEL(gtk_builder_get_object(builder, "sensor_max"));
	w_sensor_draw = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sensor_draw"));
	w_sensor_display = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sensor_enable_checkbox"));
	w_sensor_color = GTK_COLOR_BUTTON(gtk_builder_get_object(builder, "sensor_color"));
	w_sensor_alarm = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "sensor_alarm"));
	w_sensor_high_threshold = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sensor_alarm_high_threshold"));
	w_sensor_low_threshold = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "sensor_alarm_low_threshold"));
	w_sensor_high_threshold_unit = GTK_LABEL(gtk_builder_get_object(builder, "sensor_alarm_high_threshold_unit"));
	w_sensor_low_threshold_unit = GTK_LABEL(gtk_builder_get_object(builder, "sensor_alarm_low_threshold_unit"));
	w_appindicator_enabled = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "indicator_checkbox"));
	w_appindicator_label_enabled = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "indicator_label_checkbox"));

	store = GTK_LIST_STORE(gtk_builder_get_object(builder,
												  "sensors_liststore"));

	gtk_builder_connect_signals(builder, ui);

	g_signal_connect(w_dialog,
					 "delete_event",
					 G_CALLBACK(on_delete_event_cb),
					 w_dialog);

	return builder;
}

static void populate(Psensor *sensor, Psensor **sensors)
{
	GtkTreeIter iter;
	const Psensor **s_cur;
	const Psensor *s;

	gtk_list_store_clear(store);

	const Psensor **ordered_sensors = ui_get_sensors_ordered_by_position((const Psensor *const *)sensors);
	for (s_cur = ordered_sensors; *s_cur; s_cur++)
	{
		s = *s_cur;
		gtk_list_store_append(store, &iter);

		gtk_list_store_set(store, &iter,
						   COL_NAME, s->name,
						   COL_SENSOR_PREF, s,
						   -1);
	}

	select_sensor(sensor, ordered_sensors);

	free((void *)ordered_sensors);
}

void ui_sensorpref_dialog_run(Psensor *sensor, struct ui_psensor *ui)
{
	if (w_dialog == nullptr)
	{
		GtkBuilder *builder = load_ui(ui);

		if (!builder)
			return;

		g_object_unref(G_OBJECT(builder));
	}

	populate(sensor, ui->sensors);

	gtk_window_present(GTK_WINDOW(w_dialog));
}
