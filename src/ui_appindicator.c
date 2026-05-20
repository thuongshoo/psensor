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
#include <ui_appindicator.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "appindicator_compat.h"
#include <cfg.h>
#include <psensor.h>
#include <ui.h>

#include <ui_sensorpref.h>
#include <ui_status.h>
#include <ui_pref.h>
#include <paths.h>

static const char *ICON = "psensor-fork_normal";
static const char *ATTENTION_ICON = "psensor-fork_hot";

/* Build GLADE path at runtime using get_data_path() so the installed
 * data directory is resolved the same way as the main UI. */

static const Psensor **sensors;
static GtkMenuItem **menu_items;
static bool appindicator_supported = true;
static AppIndicator *indicator;
static struct ui_psensor *ui_psensor;

void ui_appindicator_menu_show_cb(GtkMenuItem *mi, gpointer data)
{
	ui_window_show((struct ui_psensor *)data);
}

void ui_appindicator_cb_preferences(GtkMenuItem *mi, gpointer data)
{
	ui_pref_dialog_run((struct ui_psensor *)data);
}

void ui_appindicator_cb_sensor_preferences(GtkMenuItem *mi, gpointer data)
{
	struct ui_psensor *ui = data;

	if (ui->sensors && *ui->sensors)
		ui_sensorpref_dialog_run(*ui->sensors, ui);
}

static void
update_menu_item(GtkMenuItem *item, const Psensor *s, Temperature_Unit temperature_unit)
{
	gchar *str;
	char *v;

	v = psensor_current_value_to_str(s, temperature_unit);

	str = g_strdup_printf("%s: %s", s->name, v);

	gtk_menu_item_set_label(item, str);

	free(v);
	g_free(str);
}

static void update_menu_items(Temperature_Unit temperature_unit)
{
	const Psensor **s;
	GtkMenuItem **m;

	if (!sensors)
		return;

	for (s = sensors, m = menu_items; *s; s++, m++)
		update_menu_item(*m, *s, temperature_unit);
}

static void
create_sensor_menu_items(const struct ui_psensor *ui, GtkMenu *menu)
{
	const Psensor **sorted_sensors;

	sorted_sensors = ui_get_sensors_ordered_by_position((const Psensor*const*)ui->sensors);
	size_t n = psensor_list_size((const Psensor *const*)sorted_sensors);
	
	menu_items = (GtkMenuItem **)malloc((n + 1) * sizeof(GtkMenuItem *));
	if (menu_items == nullptr)
		return;

	sensors = (const Psensor **)calloc((n + 1), sizeof(Psensor *));
	if (sensors == nullptr)
	{
		free((void*)menu_items);
		return;
	}

	Temperature_Unit temperature_unit = config_get_temperature_unit();
	size_t i, j;
	for ( i = 0, j = 0; i < n; i++) {
		if (config_is_appindicator_enabled(sorted_sensors[i]->id)) {
			sensors[j] = sorted_sensors[i];
			const char *name = sensors[j]->name;

			menu_items[j] = GTK_MENU_ITEM
				(gtk_menu_item_new_with_label(name));

			gtk_menu_shell_insert(GTK_MENU_SHELL(menu),
					      GTK_WIDGET(menu_items[j]),
					      (gint) (j+2));

			update_menu_item(menu_items[j], sensors[j], temperature_unit);

			j++;
		}
	}

	sensors[j] = nullptr;
	menu_items[j] = nullptr;
	//only free the list, not the items
	free((void*)sorted_sensors);
}

static GtkMenu *load_menu(struct ui_psensor *ui)
{
	GError *error;
	GtkMenu *menu;
	guint ok;
	GtkBuilder *builder;

	log_functionname_enter();

	builder = gtk_builder_new();
	gtk_builder_set_translation_domain(builder, PACKAGE_NAME);

	error = nullptr;
	/* Build the glade file path at runtime so it matches get_data_path()
	 * used elsewhere (handles PSENSOR_DATA_DIR overrides, relative paths, etc.) */
	char *data_path = get_data_path();
	gchar *glade_file = g_strdup_printf("%s%s%s-appindicator.glade",
					    data_path, G_DIR_SEPARATOR_S, PACKAGE_NAME);
	ok = gtk_builder_add_from_file(builder, glade_file, &error);

	if (!ok) {
		log_err(_("Failed to load glade file %s: %s"),
			glade_file,
			error->message);
		g_error_free(error);
		g_free(glade_file);
		free(data_path);
		return nullptr;
	}

	g_free(glade_file);
	free(data_path);

	menu = GTK_MENU(gtk_builder_get_object(builder, "appindicator_menu"));
	create_sensor_menu_items(ui, menu);
	gtk_builder_connect_signals(builder, ui);

	g_object_ref(G_OBJECT(menu));
	g_object_unref(G_OBJECT(builder));

	log_functionname_exit();

	return menu;
}
	
static const char* get_unit_format_string(unsigned int sensor_type)
{
	if (is_temperature_type(sensor_type) || (is_rpm_type(sensor_type)))
		return  "999UUU";
	/* percent */
	return "999%";
}

static int append_sensor_to_strings(char **label, char **guide, 
                                     const char *value_string,
                                     const char *unit_format)
{
    char *new_label = nullptr;
    char *new_guide = nullptr;
    
    // Cập nhật label
    if (*label) {
        if (asprintf(&new_label, "%s %s", *label, value_string) == -1) {
            return -1;
        }
    } else {
        new_label = strdup(value_string);
        if (!new_label) return -1;
    }
    
    // Cập nhật guide
    if (*guide) {
        if (asprintf(&new_guide, "%sW%s", *guide, unit_format) == -1) {
            free(new_label);
            return -1;
        }
    } else {
        new_guide = strdup(value_string);
        if (!new_guide) {
            free(new_label);
            return -1;
        }
    }
    
    free(*label);
    free(*guide);
    *label = new_label;
    *guide = new_guide;
    
    return 0;
}

static void update_label(const struct ui_psensor *ui)
{
    const Psensor **sensorList = ui_get_sensors_ordered_by_position((const Psensor *const *)ui->sensors);
    const Psensor **original_sensors = sensorList;
    
    char *label = nullptr;
    char *guide = nullptr;
    Temperature_Unit temperature_unit = config_get_temperature_unit();

    for (; *sensorList; sensorList++) {
        if (!config_is_appindicator_label_enabled((*sensorList)->id)) {
            continue;
        }

        char *value_string = psensor_current_value_to_str(*sensorList, temperature_unit);
        if (!value_string) {
            continue;
        }

        const char* unit_format = get_unit_format_string((*sensorList)->type);
        
        if (append_sensor_to_strings(&label, &guide, value_string, unit_format) != 0) {
            free(value_string);
            // Có thể log lỗi ở đây
            continue;
        }
        
        free(value_string);
    }

    if (label && guide) {
        app_indicator_set_label(indicator, label, guide);
    }
    
    free(label);
    free(guide);
	//only free the list, not the items
    free((void*)original_sensors);
}

void ui_appindicator_update(const struct ui_psensor *ui, bool is_attention)
{
	AppIndicatorStatus status;

	if (!indicator)
		return;

	update_label(ui);

	status = app_indicator_get_status(indicator);

	if (!is_attention && status == APP_INDICATOR_STATUS_ATTENTION)
		app_indicator_set_status(indicator,
					 APP_INDICATOR_STATUS_ACTIVE);

	if (is_attention && status == APP_INDICATOR_STATUS_ACTIVE)
		app_indicator_set_status(indicator,
		APP_INDICATOR_STATUS_ATTENTION);

	update_menu_items(config_get_temperature_unit());
}

static void remove_sensor_menu_items(GtkMenu *menu)
{
	GtkMenuItem **items;

	if (!menu_items)
		return;

	items = menu_items;
	while (*items) {
		gtk_container_remove(GTK_CONTAINER(menu), GTK_WIDGET(*items));

		items++;
	}

	free((void*)menu_items);
	free((void*)sensors);
}

void ui_appindicator_update_menu(struct ui_psensor *ui)
{
	GtkMenu *menu;

	menu = GTK_MENU(app_indicator_get_menu(indicator));

	if (menu) {
		remove_sensor_menu_items(menu);
		create_sensor_menu_items(ui, menu);
	} else {
		menu = load_menu(ui);

		if (menu) {
			app_indicator_set_menu(indicator, menu);
			g_object_unref(G_OBJECT(menu));
		}
	}

	if (menu)
		gtk_widget_show_all(GTK_WIDGET(menu));
}

void ui_appindicator_init(struct ui_psensor *ui)
{
	ui_psensor = ui;
	log_debug("ui_appindicator_init()");
	indicator = app_indicator_new
		(PACKAGE_NAME,
		 ICON,
		 APP_INDICATOR_CATEGORY_APPLICATION_STATUS);

	app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
	app_indicator_set_attention_icon(indicator, ATTENTION_ICON);

	ui_appindicator_update_menu(ui);
}

bool is_appindicator_supported(void)
{
	return appindicator_supported;
}

void ui_appindicator_cleanup(void)
{
	free((void*)sensors);
}
