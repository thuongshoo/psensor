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
#ifndef PSENSOR_UI_H
#define PSENSOR_UI_H

#include <paths.h>

#include <pthread.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#if defined(HAVE_APPINDICATOR)
#include "appindicator_compat.h"
#endif

#include "psensor.h"
#include "cfg.h"

#define PSENSOR_ICON PACKAGE_NAME

#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)

typedef struct ui_psensor
{
    Psensor **sensors;
    /* mutex which MUST be used for accessing sensors.*/
    pthread_mutex_t sensors_mutex;
    pthread_mutex_t graph_mutex;

    // Cache cho danh sách graph
    Pconfig *config;
    const Psensor **graph_cache;

    GtkWidget *main_window;

    GtkListStore *sensors_store;
    GtkTreeView *sensors_tree;

    uint32_t graph_version;
    uint32_t graph_update_interval;
    GtkWidget *popup_menu;
    bool should_exit;
} UI_psensor;

/*
 * Update the window according to the configuration.
 *
 * Creates or re-creates the sensor_box according to the position of
 * the list of sensors in the configuration.
 *
 * Show or hide the menu bar.
 */
void ui_window_update(UI_psensor *);

/* Show the main psensor window. */
void ui_window_show(UI_psensor *);

/* Must be called to terminate Psensor UI. */
void ui_psensor_quit(UI_psensor *ui);

/* Creates the main GTK window */
void ui_window_create(UI_psensor *ui);

void ui_menu_bar_show(unsigned int show, UI_psensor *ui);

void ui_enable_alpha_channel(UI_psensor *ui);

void ui_cb_preferences(GtkMenuItem *mi, gpointer data);
void ui_cb_menu_quit(GtkMenuItem *mi, gpointer data);
void ui_cb_sensor_preferences(GtkMenuItem *mi, gpointer data);

GtkWidget *ui_get_graph_widget(void);

const Psensor **ui_get_sensors_ordered_by_position(const Psensor *const *sensors);

void ui_cb_about(GtkAction *a, gpointer data);
#endif
