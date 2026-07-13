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
#include <stdlib.h>
#include <string.h>

#include <cfg.h>
#include <ui.h>
#include <ui_color.h>
#include <ui_pref.h>
#include <ui_sensorlist.h>
#include <ui_sensorpref.h>
#include <ui_graph.h>

enum
{
    COL_NAME = 0,
    COL_TEMP,
    COL_TEMP_MIN,
    COL_TEMP_MAX,
    COL_COLOR,
    COL_COLOR_STR,
    COL_GRAPH_ENABLED,
    COL_EMPTY,
    COL_SENSOR,
    COL_DISPLAY_ENABLED
};

struct cb_data
{
    UI_psensor *ui;
    Psensor *sensor;
};

static int col_index_to_col(int idx)
{
    if (idx == 5)
        return COL_GRAPH_ENABLED;

    if (idx > 5)
        return -1;

    return idx;
}

static void populate(UI_psensor *ui)
{
    const Psensor **ordered_sensors = ui_get_sensors_ordered_by_position((const Psensor *const *)ui->sensors);
    GtkListStore *store = ui->sensors_store;

    gtk_list_store_clear(store);

    for (const Psensor **s_cur = ordered_sensors; *s_cur; s_cur++)
    {
        const Psensor *s = *s_cur;

        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);

        GdkRGBA color;
        config_get_sensor_color_into(s->id, &color);

        char *scolor = gdk_rgba_to_string(&color);

        gboolean enabled = bool_to_gboolean(config_is_sensor_enabled(s->id));
        gtk_list_store_set(store, &iter,
                           COL_NAME, s->name,
                           COL_COLOR_STR, scolor,
                           COL_GRAPH_ENABLED, bool_to_gboolean(config_is_sensor_graph_enabled(s->id)),
                           COL_SENSOR, s,
                           COL_DISPLAY_ENABLED, enabled,
                           -1);
        free(scolor);
    }
    free((void *)ordered_sensors);
}

void ui_sensorlist_update(UI_psensor *ui, bool complete)
{
    if (complete)
        populate(ui);

    GtkTreeModel *model = gtk_tree_view_get_model(ui->sensors_tree);
    model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));

    GtkListStore *store = ui->sensors_store;

    Temperature_Unit temperature_unit = config_get_temperature_unit();

    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid)
    {
        Psensor *s;
        gtk_tree_model_get(model, &iter, COL_SENSOR, &s, -1);

        char value[PSENSOR_MAX_VALUE_LEN];
        psensor_value_to_string_buffer(s->type,
                                       psensor_get_current_value(s),
                                       temperature_unit,
                                       value,
                                       sizeof(value));
        char min[PSENSOR_MAX_VALUE_LEN];
        psensor_value_to_string_buffer(s->type,
                                       s->sess_lowest,
                                       temperature_unit,
                                       min,
                                       sizeof(min));
        char max[PSENSOR_MAX_VALUE_LEN];
        psensor_value_to_string_buffer(s->type,
                                       s->sess_highest,
                                       temperature_unit,
                                       max,
                                       sizeof(max));

        gtk_list_store_set(store, &iter,
                           COL_TEMP, value,
                           COL_TEMP_MIN, min,
                           COL_TEMP_MAX, max,
                           -1);

        valid = gtk_tree_model_iter_next(model, &iter);
    }
}

/*
 * Returns the sensor corresponding to the x/y position
 * in the table.
 *
 * <null> if none.
 */
static Psensor *
get_sensor_at_pos(GtkTreeView *view, int x, int y, UI_psensor *ui)
{
    GtkTreePath *path;
    GtkTreeModel *model;
    GtkTreeIter iter;
    Psensor *s;

    gtk_tree_view_get_path_at_pos(view, x, y, &path, nullptr, nullptr, nullptr);
    model = gtk_tree_view_get_model(ui->sensors_tree);

    if (path)
    {
        if (gtk_tree_model_get_iter(model, &iter, path))
        {
            gtk_tree_model_get(model, &iter, COL_SENSOR, &s, -1);
            gtk_tree_path_free(path);
            return s;
        }
    }
    return nullptr;
}

/*
 * Returns the index of the column corresponding
 * to the x position in the table.
 *
 * -1 if none
 */
static int get_col_index_at_pos(GtkTreeView *view, int x)
{
    GList *cols = gtk_tree_view_get_columns(view);
    int colx = 0;
    int coli = 0;
    for (GList *node = cols; node; node = node->next)
    {
        GtkTreeViewColumn *checkcol = (GtkTreeViewColumn *)node->data;

        if (x >= colx && x < (colx + gtk_tree_view_column_get_width(checkcol)))
        {
            g_list_free(cols);
            return coli;
        }

        colx += gtk_tree_view_column_get_width(checkcol);

        coli++;
    }
    g_list_free(cols);
    return -1;
}

static void preferences_activated_cbk(GtkWidget *menu_item, gpointer data)
{
    struct cb_data *cb_data = data;
    ui_sensorpref_dialog_run(cb_data->sensor, cb_data->ui);
    free(cb_data);
}

static void hide_activated_cbk(GtkWidget *menu_item, gpointer data)
{
    log_functionname_enter();

    struct cb_data *cb_data = data;
    Psensor *s = cb_data->sensor;
    config_set_sensor_enabled(s->id, false);
    config_sync();

    GtkTreeModel *fmodel = gtk_tree_view_get_model(cb_data->ui->sensors_tree);
    GtkTreeModel *model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(fmodel));
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid)
    {
        Psensor *s2;
        gtk_tree_model_get(model, &iter, COL_SENSOR, &s2, -1);

        if (s == s2)
            gtk_list_store_set(cb_data->ui->sensors_store,
                               &iter,
                               COL_DISPLAY_ENABLED,
                               FALSE,
                               -1);
        valid = gtk_tree_model_iter_next(model, &iter);
    }

    free(cb_data);

    log_functionname_exit();
}

static GtkWidget *
create_sensor_popup(UI_psensor *ui, Psensor *sensor)
{
    GtkWidget *menu, *item, *separator;
    struct cb_data *data;

    menu = gtk_menu_new();

    item = gtk_menu_item_new_with_label(sensor->name);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);

    item = gtk_menu_item_new_with_label(_("Hide"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    data = malloc(sizeof(struct cb_data));
    if (data == nullptr)
    {
        g_object_unref(separator);
        g_object_unref(menu);
        return nullptr;
    }
    data->ui = ui;
    data->sensor = sensor;
    g_signal_connect(item,
                     "activate",
                     G_CALLBACK(hide_activated_cbk), data);

    item = gtk_menu_item_new_with_label(_("Preferences"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    data = malloc(sizeof(struct cb_data));
    if (data == nullptr)
    {
        g_object_unref(separator);
        g_object_unref(menu);
        return nullptr;
    }
    data->ui = ui;
    data->sensor = sensor;
    g_signal_connect(item,
                     "activate",
                     G_CALLBACK(preferences_activated_cbk), data);

    gtk_widget_show_all(menu);

    return menu;
}

static int clicked_cbk(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    UI_psensor *ui;
    GtkTreeView *view;
    Psensor *s;

    ui = (UI_psensor *)data;
    view = ui->sensors_tree;

    s = get_sensor_at_pos(view, (int)event->x, (int)event->y, ui);

    if (s)
    {
        int coli = col_index_to_col(get_col_index_at_pos(view, (int)event->x));

        if (coli == COL_COLOR)
        {
            GdkRGBA color;
            config_get_sensor_color_into(s->id, &color);
            if (ui_change_color(_("Select sensor color"),
                                &color,
                                GTK_WINDOW(ui->main_window)))
            {
                config_set_sensor_color(s->id, &color);
                ui_sensorlist_update(ui, true);
                config_sync();
            }

            return TRUE;
        }

        if (coli >= 0 && coli != COL_GRAPH_ENABLED)
        {
            GtkWidget *menu = create_sensor_popup(ui, s);
            if (menu == nullptr)
                return FALSE;

            gtk_menu_popup_at_pointer(GTK_MENU(menu), (const GdkEvent *)event);
            return TRUE;
        }
    }
    return FALSE;
}

void ui_sensorlist_cb_graph_toggled(GtkCellRendererToggle *cell,
                                    const gchar *path_str,
                                    gpointer data)
{

    UI_psensor *ui = (UI_psensor *)data;
    GtkTreeModel *fmodel = gtk_tree_view_get_model(ui->sensors_tree);

    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);

    GtkTreeIter iter;

    gtk_tree_model_get_iter(fmodel, &iter, path);

    Psensor *fmodel_sensor;
    gtk_tree_model_get(fmodel, &iter, COL_SENSOR, &fmodel_sensor, -1);

    bool b = !config_is_sensor_graph_enabled(fmodel_sensor->id);
    config_set_sensor_graph_enabled(fmodel_sensor->id, b);

    config_sync();

    graph_cache_invalidate(ui);

    gtk_tree_path_free(path);

    GtkTreeModel *model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(fmodel));
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid)
    {
        Psensor *filtered_model_sensor;
        gtk_tree_model_get(model, &iter, COL_SENSOR, &filtered_model_sensor, -1);

        if (fmodel_sensor == filtered_model_sensor)
            gtk_list_store_set(ui->sensors_store,
                               &iter,
                               COL_GRAPH_ENABLED,
                               b,
                               -1);
        valid = gtk_tree_model_iter_next(model, &iter);
    }
}

void ui_sensorlist_create(UI_psensor *ui)
{
    GtkTreeModel *fmodel, *model;

    log_functionname_enter();

    model = gtk_tree_view_get_model(ui->sensors_tree);
    fmodel = gtk_tree_model_filter_new(model, nullptr);
    gtk_tree_model_filter_set_visible_column(GTK_TREE_MODEL_FILTER(fmodel),
                                             COL_DISPLAY_ENABLED);

    gtk_tree_view_set_model(ui->sensors_tree, fmodel);

    g_signal_connect(ui->sensors_tree,
                     "button-press-event", (GCallback)clicked_cbk, ui);

    ui_sensorlist_update(ui, true);

    log_functionname_exit();
}
