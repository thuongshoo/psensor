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
#include <pudisks2.h>

#include <locale.h>
#include <libintl.h>
#define _(str) gettext(str)

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <udisks/udisks.h>


#include <temperature.h>

static const char *PROVIDER_NAME = "udisks2";

static GDBusObjectManager *manager;

static const time_t SMART_UPDATE_INTERVAL = 30;

struct udisks_data {
	char *path;
	struct timeval last_smart_update;
};

static void udisks_data_free(void *data)
{
	struct udisks_data *u = (struct udisks_data *)data;
	free(u->path);
	free(u);
}

static void smart_update(Psensor *s, UDisksDriveAta *ata)
{
	struct udisks_data *data = s->provider_data;
	struct timeval t;

	if (gettimeofday(&t, nullptr) != 0) {
		log_err("%s: %s", PROVIDER_NAME, _("gettimeofday failed."));
		return;
	}

	if (data->last_smart_update.tv_sec
	    &&
	    (t.tv_sec - data->last_smart_update.tv_sec < SMART_UPDATE_INTERVAL))
		return;

	log_functionname("%s: update SMART data for %s", PROVIDER_NAME, data->path);

	GVariant *variant = g_variant_new_parsed("{'nowakeup': %v}",
						g_variant_new_boolean(TRUE));

	gboolean ret = udisks_drive_ata_call_smart_update_sync(ata,
								variant,
								nullptr,
								nullptr);

	if (!ret) {
		log_functionname("%s: SMART update failed for %s",
			PROVIDER_NAME,
			data->path);
	}
	data->last_smart_update = t;
}

void udisks2_psensor_list_update(Psensor **sensors)
{
	for (; *sensors; sensors++) {
		Psensor *s = *sensors;

		if (s->type & SENSOR_TYPE_REMOTE)
			continue;

		if (s->type & SENSOR_TYPE_UDISKS2) {
			const struct udisks_data *data = (struct udisks_data *)s->provider_data;

			GDBusObject *o = g_dbus_object_manager_get_object(manager,
									     data->path);

			if (!o)
				continue;

			UDisksDriveAta *drive_ata = nullptr;
			g_object_get(o, "drive-ata", &drive_ata, nullptr);

			smart_update(s, drive_ata);

			double v = udisks_drive_ata_get_smart_temperature(drive_ata);

			psensor_set_current_value(s, kelvin_to_celsius(v));

			g_object_unref(G_OBJECT(o));
			g_object_unref(G_OBJECT(drive_ata));
		}
	}
}

void udisks2_psensor_list_append(Psensor ***sensors, unsigned int values_length)
{
	log_functionname_enter();

	UDisksClient *client = udisks_client_new_sync(nullptr, nullptr);
	if (!client) {
		log_err(_("%s: cannot get the udisks2 client"), PROVIDER_NAME);
		log_functionname_exit();
		return;
	}

	manager = udisks_client_get_object_manager(client);

	GList *objects = g_dbus_object_manager_get_objects(manager);

	int i = 0;
	for (GList *cur = objects; cur; cur = cur->next) {
		const char *path = g_dbus_object_get_object_path(cur->data);

		UDisksDrive *drive = nullptr;
		UDisksDriveAta *drive_ata = nullptr;
		g_object_get(cur->data,
			     "drive", &drive,
			     "drive-ata", &drive_ata,
			     nullptr);

		if (!drive) {
			log_functionname("Not a drive: %s", path);
			continue;
		}

		if (!drive_ata) {
			log_functionname("Not an ATA drive: %s", path);
			continue;
		}

		if (!udisks_drive_ata_get_smart_enabled(drive_ata)) {
			log_functionname("SMART not enabled: %s", path);
			continue;
		}

		if (!udisks_drive_ata_get_smart_temperature(drive_ata)) {
			log_functionname("No temperature available: %s", path);
			continue;
		}

		const char *drive_id = udisks_drive_get_id(drive);
		char *id;
		if (drive_id) {
			id = g_strdup_printf("%s %s", PROVIDER_NAME, drive_id);
		} else {
			id = g_strdup_printf("%s %d", PROVIDER_NAME, i);
			i++;
		}

		const char *drive_model = udisks_drive_get_model(drive);
		char *name, *chip;
		if (drive_model) {
			name = strdup(drive_model);
			chip = strdup(drive_model);
		} else {
			name = strdup(_("Disk"));
			chip = strdup(_("Disk"));
		}

		unsigned int type = SENSOR_TYPE_TEMP | SENSOR_TYPE_UDISKS2 | SENSOR_TYPE_HDD;

		Psensor *s = psensor_create(id, name, chip, type, values_length);
		if (s == nullptr) {
			free(chip);
			free(name);
			free(id);
			continue;
		}
		struct udisks_data *data = malloc(sizeof(struct udisks_data));
		if (data == nullptr) {
			psensor_free(s);
			continue;
		}
		data->path = strdup(path);
		memset(&data->last_smart_update, 0, sizeof(struct timeval));

		s->provider_data = data;
		s->provider_data_free_fct = &udisks_data_free;

		psensor_list_append(sensors, s);

		g_object_unref(G_OBJECT(drive_ata));
		g_object_unref(G_OBJECT(drive));
		g_object_unref(G_OBJECT(cur->data));
	}

	g_list_free(objects);
	g_object_unref(client);

	log_functionname_exit();
}
