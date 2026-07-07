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
#ifndef PSENSOR_UI_PREF_H
#define PSENSOR_UI_PREF_H

#include "ui.h"

void ui_pref_dialog_run(UI_psensor *);
GdkRGBA color_to_GdkRGBA(const Pcolor *color);

void ui_pref_decoration_toggled_cbk(GtkToggleButton *, gpointer);
void ui_pref_keep_below_toggled_cbk(GtkToggleButton *, gpointer);

void ui_pref_temperature_unit_changed_cbk(GtkComboBox *combo, gpointer data);
void ui_pref_menu_toggled_cbk(GtkToggleButton *btn, gpointer data);
void ui_pref_count_visible_toggled_cbk(GtkToggleButton *btn, gpointer data);
void ui_pref_sensorlist_position_changed_cbk(GtkComboBox *combo, gpointer data);
#endif
