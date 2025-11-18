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
#include <locale.h>
#include <libintl.h>
#define _(str) gettext(str)

#include <string.h>

#include <glibtop/cpu.h>
#include <glibtop/mem.h>

#include <pgtop2.h>

static float last_used;
static float last_total;

static const char *PROVIDER_NAME = "gtop2";

struct psensor *create_cpu_usage_sensor(unsigned int measures_len)
{
	char* id = g_strdup_printf("%s cpu usage", PROVIDER_NAME);
	char* label = strdup(_("CPU usage"));
	unsigned int type = SENSOR_TYPE_GTOP | SENSOR_TYPE_CPU_USAGE;

	char* chip = strdup(_("CPU"));
	struct psensor *psensor = psensor_create(id,
				 label,
				 chip,
				 type,
				 measures_len);
	if (psensor == NULL)
	{
		free(chip);
		free(label);
		free(id);
		return NULL;
	}
	return psensor;
}

static struct psensor *create_mem_free_sensor(unsigned int measures_len)
{
	char* id = g_strdup_printf("%s mem free", PROVIDER_NAME);
	char* name = strdup(_("free memory"));
	char* chip = strdup(_("memory"));
	unsigned int type = SENSOR_TYPE_GTOP | SENSOR_TYPE_MEMORY | SENSOR_TYPE_PERCENT;
	struct psensor* tmp = psensor_create(id,
			      name,
			      chip,
			      type,
			      measures_len);
	if (tmp == NULL)
	{
		free(chip);
		free(name);
		free(id);
		return NULL;
	}
	return tmp;
}

static double get_usage(void)
{
	glibtop_cpu cpu;
	unsigned long int used, dt;
	double cpu_rate;

	glibtop_get_cpu(&cpu);

	used = cpu.user + cpu.nice + cpu.sys;

	dt = cpu.total - last_total;

	if (dt)
		cpu_rate = 100.0 * (used - last_used) / dt;
	else
		cpu_rate = UNKNOWN_DOUBLE_VALUE;

	last_used = used;
	last_total = cpu.total;

	return cpu_rate;
}

static double get_mem_free(void)
{
	glibtop_mem mem;
	double v;

	glibtop_get_mem(&mem);
	v = ((double)mem.free) * 100.0 / mem.total;

	return v;
}

void gtop2_psensor_list_append(struct psensor ***sensors, unsigned int measures_len)
{
	psensor_list_append(sensors, create_cpu_usage_sensor(measures_len));
	psensor_list_append(sensors, create_mem_free_sensor(measures_len));
}

void cpu_usage_sensor_update(struct psensor *s)
{
	double v;

	v = get_usage();

	if (v != UNKNOWN_DOUBLE_VALUE)
		psensor_set_current_value(s, v);
}

static void mem_free_sensor_update(struct psensor *s)
{
	double v;

	v = get_mem_free();

	if (v != UNKNOWN_DOUBLE_VALUE)
		psensor_set_current_value(s, v);
}

void gtop2_psensor_list_update(struct psensor **sensors)
{
	struct psensor *s;

	while (*sensors) {
		s = *sensors;

		if (!(s->type & SENSOR_TYPE_REMOTE)
		    && s->type & SENSOR_TYPE_GTOP) {
			if (s->type & SENSOR_TYPE_CPU)
				cpu_usage_sensor_update(s);
			else if (s->type & SENSOR_TYPE_MEMORY)
				mem_free_sensor_update(s);
		}

		sensors++;
	}
}
