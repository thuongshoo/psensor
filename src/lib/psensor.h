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
#ifndef PSENSOR_PSENSOR_H
#define PSENSOR_PSENSOR_H

#include <bool.h>

#include <sensors/sensors.h>

#include <measure.h>
#include <plog.h>
#include <temperature.h>

typedef enum psensor_type
{
    /* type of sensor values */
    SENSOR_TYPE_TEMP = 0x00001U,
    SENSOR_TYPE_RPM = 0x00002U,
    SENSOR_TYPE_PERCENT = 0x00004U,

    /* Whether the sensor is remote */
    SENSOR_TYPE_REMOTE = 0x00008U,

    /* Libraries used for retrieving sensor information */
    SENSOR_TYPE_LMSENSOR = 0x00100U,

    SENSOR_TYPE_NVCTRL = 0x00200U,
    SENSOR_TYPE_GTOP = 0x00400U,
    SENSOR_TYPE_ATIADL = 0x00800U,
    SENSOR_TYPE_ATASMART = 0x01000U,
    SENSOR_TYPE_HDDTEMP = 0x02000U,
    SENSOR_TYPE_UDISKS2 = 0x800000U,

    /* Type of HW component */
    SENSOR_TYPE_HDD = 0x04000U,
    SENSOR_TYPE_CPU = 0x08000U,
    SENSOR_TYPE_GPU = 0x10000U,
    SENSOR_TYPE_FAN = 0x20000U,

    SENSOR_TYPE_GRAPHICS = 0x40000U,
    SENSOR_TYPE_VIDEO = 0x80000U,
    SENSOR_TYPE_PCIE = 0x100000U,
    SENSOR_TYPE_MEMORY = 0x200000U,
    SENSOR_TYPE_AMBIENT = 0x400000U,

    /* Combinations */
    SENSOR_TYPE_HDD_TEMP = (SENSOR_TYPE_HDD | SENSOR_TYPE_TEMP),
    SENSOR_TYPE_CPU_USAGE = (SENSOR_TYPE_CPU | SENSOR_TYPE_PERCENT),

    SENSOR_TYPE_LMSENSOR_TEMP = SENSOR_TYPE_LMSENSOR | SENSOR_TYPE_TEMP,
    SENSOR_TYPE_LMSENSOR_FAN_RPM = SENSOR_TYPE_LMSENSOR | SENSOR_TYPE_RPM | SENSOR_TYPE_FAN,

    SENSOR_TYPE_NVCTRL_GPU_TEMP = SENSOR_TYPE_NVCTRL | SENSOR_TYPE_GPU | SENSOR_TYPE_TEMP,
    SENSOR_TYPE_NVCTRL_GPU_PERCENT_AMBIENT = SENSOR_TYPE_NVCTRL | SENSOR_TYPE_GPU | SENSOR_TYPE_AMBIENT,
    SENSOR_TYPE_NVCTRL_GPU_PERCENT_GRAPHICS = SENSOR_TYPE_NVCTRL | SENSOR_TYPE_GPU | SENSOR_TYPE_GRAPHICS,
    SENSOR_TYPE_NVCTRL_GPU_PERCENT_VIDEO = SENSOR_TYPE_NVCTRL | SENSOR_TYPE_GPU | SENSOR_TYPE_VIDEO,
    SENSOR_TYPE_NVCTRL_GPU_PERCENT_MEMORY = SENSOR_TYPE_NVCTRL | SENSOR_TYPE_GPU | SENSOR_TYPE_MEMORY,
    SENSOR_TYPE_NVCTRL_GPU_RPM_FAN = SENSOR_TYPE_NVCTRL | SENSOR_TYPE_FAN | SENSOR_TYPE_RPM,
    SENSOR_TYPE_NVCTRL_GPU_PERCENT_FAN = SENSOR_TYPE_NVCTRL | SENSOR_TYPE_FAN | SENSOR_TYPE_PERCENT,

    SENSOR_TYPE_HDD_HDDTEMP_TEMP = SENSOR_TYPE_HDD | SENSOR_TYPE_HDDTEMP | SENSOR_TYPE_TEMP,
    SENSOR_TYPE_GTOP_MEMORY_PERCENT = SENSOR_TYPE_GTOP | SENSOR_TYPE_MEMORY | SENSOR_TYPE_PERCENT,
    SENSOR_TYPE_ATASMART_HDD_TEMP = SENSOR_TYPE_ATASMART | SENSOR_TYPE_HDD | SENSOR_TYPE_TEMP,
    SENSOR_TYPE_HDD_UDISKS2_TEMP = SENSOR_TYPE_TEMP | SENSOR_TYPE_UDISKS2 | SENSOR_TYPE_HDD,

    SENSOR_TYPE_GTOP_CPU_USAGE = SENSOR_TYPE_GTOP | SENSOR_TYPE_CPU_USAGE
} PsensorType;

// Forward declaration
typedef struct psensor Psensor;
typedef struct psensor
{
    /* Human readable name of the sensor.  It may not be uniq. */
    char *name;
    /* Uniq id of the sensor */
    char *id;
    /* Name of the chip. */
    char *chip;

    /* see psensor_type */
    PsensorType type;
    /*
     * Last registered measures of the sensor.  Index 0 for the
     * oldest measure.
     */
    Pmeasure *measures;
    // callback handler
    void (*cb_alarm_raised)(Psensor *, void *);
    void *cb_alarm_raised_data;

    void *provider_data;
    void (*provider_data_free_fct)(void *);
#ifdef HAVE_LIBATIADL
    /* AMD id for the aticonfig */
    int amd_id;
#endif

    /* maximium value in duration */
    double max;
    /* minimium value in duration */
    double min;
    /* The highest value detected during this session. */
    double sess_highest;
    /* The lowest value detected during this session. */
    double sess_lowest;
    double alarm_high_threshold;
    double alarm_low_threshold;

    size_t measures_size;  // Total size of array
    size_t measures_count; // current number of items
    size_t measures_head;  // Index of newest measurement
    size_t measures_tail;  // oldest measurement
    bool measures_full;

    /* Whether an alarm is raised for this sensor */
    bool alarm_raised;
} Psensor;

typedef struct
{
    Psensor **sensors;
    size_t length;
} PsensorList;

//
typedef enum
{
    ITER_FORWARD = 0,
    ITER_REVERSE = 1
} iter_direction_t;

// Iterator để vẽ từ OLDEST đến NEWEST
struct measure_iterator
{
    const Psensor *sensor;
    size_t current_pos;
    size_t remaining;
    iter_direction_t direction;
};

void measure_iterator_init(struct measure_iterator *it, const Psensor *s);
void measure_iterator_init_reverse(struct measure_iterator *it, const Psensor *s);
bool measure_iterator_next(struct measure_iterator *it, Pmeasure **result);
bool measure_iterator_prev(struct measure_iterator *it, Pmeasure **result);

Psensor *psensor_create(char *id,
                        char *name,
                        char *chip,
                        unsigned int type,
                        unsigned int values_max_length);

void psensor_values_resize(Psensor *psensor, unsigned int new_size);

void psensor_free(Psensor *sensor);

void psensor_list_free(Psensor **sensors);
size_t psensor_list_size(const Psensor *const *sensors);

const Psensor *psensor_list_get_by_id(const Psensor *const *sensors,
                                      const char *id);

bool is_temperature_type(unsigned int type);
bool is_rpm_type(unsigned int type);

/*
 * Converts the value of a sensor to a string.
 *
 * parameter 'type' is SENSOR_TYPE_LMSENSOR_TEMP, SENSOR_TYPE_NVIDIA,
 * or SENSOR_TYPE_LMSENSOR_FAN
 */
char *psensor_value_to_str(unsigned int type,
                           double value,
                           Temperature_Unit temperature_unit);

char *psensor_unit_to_str(unsigned int type,
                          Temperature_Unit temperature_unit);

char *psensor_measure_to_str(const Pmeasure *m,
                             unsigned int type,
                             Temperature_Unit temperature_unit);

void psensor_list_append(Psensor ***sensors, Psensor *sensor);

const Psensor **psensor_list_copy(const Psensor *const *);

void psensor_set_current_value(Psensor *sensor, double value);
void psensor_set_current_measure(Psensor *sensor, double value,
                                 struct timeval tv);

double psensor_get_current_value(const Psensor *);

Pmeasure *psensor_get_current_measure(const Psensor *sensor);

/* Returns a string representation of a psensor type. */
const char *psensor_type_to_str(unsigned int type);

const char *psensor_type_to_unit_str(unsigned int type, Temperature_Unit temperature_unit);

typedef struct minmax_st
{
    double min;
    double max;
} MINMAX;

typedef struct all_minmax_st
{
    MINMAX temperature;
    MINMAX revolutions_per_minute;
    MINMAX percent;
    time_t end_time;
} ALL_MINMAX;

ALL_MINMAX get_all_minmax_values(const Psensor *const *all_sensors);

char *psensor_current_value_to_str(const Psensor *, Temperature_Unit temperature_unit);

void psensor_log_measures(Psensor **sensors);

#endif
