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
#include <pthread.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <libnotify/notify.h>

/* Macro defined since libnotify 0.5.2 */
#ifndef NOTIFY_CHECK_VERSION
#define NOTIFY_CHECK_VERSION(x, y, z) 0
#endif

#include "cfg.h"
#include "ui.h"
#include "ui_notify.h"

/* Time of the last notification. */
static struct timeval last_notification_tv;

void ui_notify(Psensor *sensor, struct ui_psensor *ui)
{
	log_debug("last_notification %d", last_notification_tv.tv_sec);

	struct timeval time;
	if (gettimeofday(&time, nullptr) != 0)
	{
		log_err(_("gettimeofday failed."));
		return;
	}

	if (!last_notification_tv.tv_sec || time.tv_sec - last_notification_tv.tv_sec >= 60)
		last_notification_tv = time;
	else
		return;

	if (notify_is_initted() == FALSE)
		notify_init(PACKAGE_NAME);

	if (notify_is_initted() == TRUE)
	{
		Temperature_Unit temperature_unit = config_get_temperature_unit();

		const Pmeasure *measure = psensor_get_current_measure(sensor);
		char svalue[PSENSOR_MAX_VALUE_LEN];
		psensor_value_to_string_buffer(sensor->type, measure->value, temperature_unit, svalue, sizeof(svalue));

		char *body;
		if (-1 == asprintf(&body, "%s : %s", sensor->name, svalue))
			return;

		const char *summary;
		if (is_temperature_type(sensor->type))
			summary = _("Temperature alert");
		else if (sensor->type & SENSOR_TYPE_RPM)
			summary = _("Fan speed alert");
		else
			summary = _("N/A");

		NotifyNotification *notif;
		/*
		 * Since libnotify 0.7 notify_notification_new has
		 * only 3 parameters.
		 */
#if NOTIFY_CHECK_VERSION(0, 7, 0)
		notif = notify_notification_new(summary, body, PSENSOR_ICON);
#else
		notif = notify_notification_new(summary,
										body,
										PSENSOR_ICON,
										GTK_WIDGET(ui->main_window));
#endif
		log_debug("notif_notification_new %s", body);

		notify_notification_show(notif, nullptr);

		free(body);
		g_object_unref(notif);
	}
	else
	{
		log_err("notify not initialized");
	}
}
