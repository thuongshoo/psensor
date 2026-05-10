/*
 * Copyright (C) 2010-2014 jeanfi@gmail.com
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
#define _GNU_SOURCE
#include "config.h"
#include <stdlib.h>
#include <string.h>

#include <locale.h>
#include <libintl.h>
#define _(str) gettext(str)

#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include <unistd.h>


#include <hdd.h>
#include <psensor.h>
#include <temperature.h>


char *
psensor_value_to_str(unsigned int type, double value, Temperature_Unit temperature_unit)
{
    /*
     * should not be possible to exceed 16 characters with temp or
     * rpm values the .x part is never displayed
     */
    const int MAX_STR_LEN = 16;
    char *str = malloc(MAX_STR_LEN);
    if (str == NULL)
        return NULL;

    if (is_temperature_type(type) && is_celsius(temperature_unit) == false)
        value = celsius_to_fahrenheit(value);

    snprintf(str, MAX_STR_LEN, "%.0f", value);

    return str;
}

static const char* CELSIUS_STRING = "\302\260C";
static const char* FAHRENHEIT_STRING = "\302\260F";
const char *psensor_type_to_unit_str(unsigned int type, Temperature_Unit temperature_unit)
{
    if (is_temperature_type(type))
    {
        if (temperature_unit == CELSIUS)
            return CELSIUS_STRING;

        return FAHRENHEIT_STRING;
    }
    
    if (type & SENSOR_TYPE_RPM)
    {
        return _("RPM");
    }
    
    if (type & SENSOR_TYPE_PERCENT)
    {
        return _("%");
    }

    return _("N/A");
}

char *
psensor_unit_to_str(unsigned int type, Temperature_Unit temperature_unit)
{
    /*
     * should not be possible to exceed 4 characters with temp or
     * rpm values the .x part is never displayed
     */
    const int MAX_STR_LEN = 4;

    char *str = malloc(MAX_STR_LEN);
    if(str == NULL)
        return NULL;

    const char *unit = psensor_type_to_unit_str(type, temperature_unit);

    snprintf(str, MAX_STR_LEN, "%s", unit);

    return str;
}

char *
psensor_measure_to_str(const struct measure *m,
                       unsigned int type,
                       Temperature_Unit temperature_unit)
{
    return psensor_value_to_str(type, m->value, temperature_unit);
}

Psensor*
ANALYZER_RETURNS_MALLOC
ANALYZER_HOLDS_MALLOC(1)
ANALYZER_HOLDS_MALLOC(2)
ANALYZER_HOLDS_MALLOC(3)
psensor_create(char *id,
                               char *name,
                               char *chip,
                               unsigned int type,
                               unsigned int values_max_length)
{
    Psensor *psensor = (Psensor *)malloc(sizeof(Psensor));
    if (psensor == NULL)
        return NULL;

    psensor->id = id;
    psensor->name = name;
    psensor->chip = chip;
    psensor->sess_lowest = UNKNOWN_DOUBLE_VALUE;
    psensor->sess_highest = UNKNOWN_DOUBLE_VALUE;

    if (type & SENSOR_TYPE_PERCENT)
    {
        psensor->min = 0;
        psensor->max = 100;
    }
    else
    {
        psensor->min = UNKNOWN_DOUBLE_VALUE;
        psensor->max = UNKNOWN_DOUBLE_VALUE;
    }

    psensor->type = type;

    psensor->measures = measures_double_create(values_max_length);

    psensor->alarm_high_threshold = 0;
    psensor->alarm_low_threshold = 0;

    psensor->cb_alarm_raised = NULL;
    psensor->cb_alarm_raised_data = NULL;
    psensor->alarm_raised = false;

    psensor->provider_data = NULL;
    psensor->provider_data_free_fct = &free;

    psensor->measures_size = values_max_length;
    psensor->measures_head = 0; 
    psensor->measures_count = 0;
    psensor->measures_full = false;
    
    return psensor;
}

void psensor_values_resize(Psensor *psensor, unsigned int new_size)
{
    struct measure *cur_ms = psensor->measures;
    struct measure *new_ms = measures_double_create(new_size);

    if (cur_ms)
    {
        size_t cur_size = psensor->measures_size;
        size_t i;
        // copy the old circle buffer to the new one		
        for (i = 0; i < new_size - 1 && i < cur_size - 1; i++)
            measure_copy(&cur_ms[cur_size - i - 1],
                         &new_ms[new_size - i - 1]);

        measures_free(psensor->measures);
    }

    psensor->measures = new_ms;

    psensor->measures_size = new_size;
    psensor->measures_head = 0; 
    psensor->measures_count = 0;
    psensor->measures_full = false;
}

void 
ANALYZER_TAKES_MALLOC(1)
psensor_free(Psensor *s)
{
    if (!s)
        return;

    log_debug("Cleanup %s", s->id);

    free(s->name);
    free(s->id);

    if (s->chip)
        free(s->chip);

    measures_free(s->measures);

    if (s->provider_data && s->provider_data_free_fct)
        s->provider_data_free_fct(s->provider_data);

    free(s);
}

void
ANALYZER_TAKES_MALLOC(1)
psensor_list_free(Psensor **sensors)
{
    if (sensors)
    {
        Psensor **sensor_cur = sensors;

        while (*sensor_cur)
        {
            psensor_free(*sensor_cur);

            sensor_cur++;
        }

        free((void*)sensors);
    }
}

size_t psensor_list_size(const Psensor * const *sensors)
{
    if (!sensors)
        return 0;

    size_t size = 0;
    const Psensor *const *sensor_cur = sensors;

    while (*sensor_cur)
    {
        size++;
        sensor_cur++;
    }
    return size;
}
/*
 * Return a new sensors list with 'sensor' added at the end of
 * 'sensors'. Layout: old items + new item + null termination
 */
static Psensor ** ANALYZER_RETURNS_MALLOC psensor_list_add(Psensor **all_sensors,
                                  Psensor *new_sensor)
{
    Psensor **new_sensor_list = NULL;

    if (all_sensors)
    {
        size_t size = psensor_list_size((const Psensor *const *)all_sensors);
        // allocate new list with one more entry
        new_sensor_list = (Psensor **)malloc((size + 1 + 1) * sizeof(Psensor *));
        if (new_sensor_list == NULL)
            return NULL;

        memcpy((void*)new_sensor_list, (void*)all_sensors, size * sizeof(Psensor *));

        new_sensor_list[size] = new_sensor;
        // null terminate the list
        new_sensor_list[size + 1] = NULL;
    }
    return new_sensor_list;
}

/// @brief append sensor to sensors list
/// @param list_sensors
/// @param new_sensor
void
ANALYZER_TAKES_MALLOC(1) 
ANALYZER_HOLDS_MALLOC(2)
psensor_list_append(Psensor ***list_sensors, Psensor *new_sensor )
{
    if (!new_sensor)
        return;

    Psensor **tmp = psensor_list_add(*list_sensors, new_sensor);
    if (tmp != NULL)
    {
        // free old sensors list
        free((void*)*list_sensors);
        // update sensors pointer
        *list_sensors = tmp;
    }
}

const Psensor *psensor_list_get_by_id(const Psensor *const *sensors, const char *id)
{
    const Psensor *const*sensors_cur = sensors;

    while (*sensors_cur)
    {
        if (!strcmp((*sensors_cur)->id, id))
            return *sensors_cur;

        sensors_cur++;
    }

    return NULL;
}

bool is_temperature_type(unsigned int type)
{
    return (type & SENSOR_TYPE_TEMP) != 0;
}

bool is_rpm_type(unsigned int type)
{
    return (type & SENSOR_TYPE_RPM) != 0;
}

static void check_if_call_alarm(Psensor *s, double v)
{
    if (v > s->alarm_high_threshold || v < s->alarm_low_threshold)
    {
        if (!s->alarm_raised && s->cb_alarm_raised)
        {
            s->alarm_raised = true;
            s->cb_alarm_raised(s, s->cb_alarm_raised_data);
        }
    }
    else
    {
        s->alarm_raised = false;
    }
}

static void update_lowest_highest(Psensor *s, double v) {
    if (s->sess_lowest == UNKNOWN_DOUBLE_VALUE || v < s->sess_lowest)
        s->sess_lowest = v;

    if (s->sess_highest == UNKNOWN_DOUBLE_VALUE || v > s->sess_highest)
        s->sess_highest = v;
}

void psensor_set_current_measure(Psensor *s, double v, struct timeval tv)
{
    if (s->measures_count == 0) {
        s->measures_head = 0;
        s->measures_tail = 0;
        s->measures_count = 1;
    } else {
        // TĂNG HEAD - conditional
        s->measures_head++;
        if (s->measures_head >= s->measures_size)
            s->measures_head = 0;
        
        if (s->measures_full) {
            // TĂNG TAIL - conditional  
            s->measures_tail++;
            if (s->measures_tail >= s->measures_size)
                s->measures_tail = 0;
        } else {
            s->measures_count++;
            if (s->measures_count == s->measures_size)
                s->measures_full = true;
            else
                s->measures_full = false;
        }
    }
    
    s->measures[s->measures_head].value = v;
    s->measures[s->measures_head].time = tv;
    
    update_lowest_highest(s, v);
    check_if_call_alarm(s, v);
}

void psensor_set_current_value(Psensor *sensor, double value)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0)
        timerclear(&tv);

    psensor_set_current_measure(sensor, value, tv);
}

void measure_iterator_init(struct measure_iterator *it, const Psensor *sensor)
{
    it->sensor = sensor;
    it->current_pos = sensor->measures_tail;  // 
    it->remaining = sensor->measures_count;
    it->direction = ITER_FORWARD; // 
}

void measure_iterator_init_reverse(struct measure_iterator *it, const Psensor *sensor)
{
    it->sensor = sensor;
    it->current_pos = sensor->measures_head;  // 
    it->remaining = sensor->measures_count;
    it->direction = ITER_REVERSE; // Reverse iteration
}

bool measure_iterator_next(struct measure_iterator *it, struct measure **result)
{
    if (it->remaining <= 0)
        return false;
    
    *result = &it->sensor->measures[it->current_pos];
    
    it->current_pos++;
    if (it->current_pos >= it->sensor->measures_size) {
        it->current_pos = 0;
    }
    
    it->remaining--;
    return true;
}

bool measure_iterator_prev(struct measure_iterator *it, struct measure **result)
{
    if (it->remaining <= 0)
        return false;
    
    *result = &it->sensor->measures[it->current_pos];
    
    //
    if (it->current_pos == 0) {
        it->current_pos = it->sensor->measures_size - 1;
    }
    else
        it->current_pos--;

    it->remaining--;
    return true;
}

double psensor_get_current_value(const Psensor *sensor)
{
    if (sensor->measures_count == 0) 
        return UNKNOWN_DOUBLE_VALUE;
    return sensor->measures[sensor->measures_head].value;
}

struct measure *psensor_get_current_measure(const Psensor *sensor)
{
    return &sensor->measures[sensor->measures_head];
}

static void process_measure_values(const Psensor *sensor, MINMAX *minmax)
{
    double local_min = minmax->min;
    double local_max = minmax->max;
    
    size_t total_measures = sensor->measures_count;
    
    for (size_t measure_index = 0;  measure_index < total_measures; measure_index++)
    {
        struct measure measure = sensor->measures[measure_index];
        if (local_max < measure.value)
        {
            local_max = measure.value;
        }

        if (local_min > measure.value)
        {
            local_min = measure.value;
        }

    }

    minmax->min = local_min;
    minmax->max = local_max;
}

ALL_MINMAX get_all_minmax_value(const Psensor * const *all_sensors)
{
    ALL_MINMAX minmax = {
        {DBL_MAX, DBL_MIN},     // temp: min=MAX, max=MIN
        {DBL_MAX, DBL_MIN},     // rpm: min=MAX, max=MIN  
        {100.0, 0.0},           // percent: min=100%, max=0%
        0,                      // end_time
    };

    if (all_sensors == NULL)
        return minmax;
    // Iterate through all sensors in the system (array ends with NULL)
    for (size_t sensor_index = 0; all_sensors[sensor_index] != NULL; sensor_index++)
    {
        const Psensor *current_sensor = all_sensors[sensor_index];
        if (current_sensor->type & SENSOR_TYPE_TEMP)
        {
            process_measure_values(current_sensor, &minmax.temp);
        }
        else if (current_sensor->type & SENSOR_TYPE_FAN)
        {
            process_measure_values(current_sensor, &minmax.rpm);
        }
        else
        {
            process_measure_values(current_sensor, &minmax.percent);
        }
    }

    return minmax;
}

const char *psensor_type_to_str(unsigned int type)
{
    if (type & SENSOR_TYPE_NVCTRL)
    {
        if (type & SENSOR_TYPE_TEMP)
            return "Temperature";
        
        if (type & SENSOR_TYPE_GRAPHICS)
            return "Graphics usage";
        
        if (type & SENSOR_TYPE_VIDEO)
            return "Video usage";
        
        if (type & SENSOR_TYPE_MEMORY)
            return "Memory usage";
        
        if (type & SENSOR_TYPE_PCIE)
            return "PCIe usage";

        return "NVIDIA GPU";
    }

    if (type & SENSOR_TYPE_ATIADL)
    {
        if (type & SENSOR_TYPE_TEMP)
            return "AMD GPU Temperature";
        
        if (type & SENSOR_TYPE_RPM)
            return "AMD GPU Fan Speed";
        
        return "AMD GPU Usage";
    }

    if ((type & SENSOR_TYPE_HDD_TEMP) == SENSOR_TYPE_HDD_TEMP)
        return "HDD Temperature";

    if ((type & SENSOR_TYPE_CPU_USAGE) == SENSOR_TYPE_CPU_USAGE)
        return "CPU Usage";

    if (type & SENSOR_TYPE_TEMP)
        return "Temperature";

    if (type & SENSOR_TYPE_RPM)
        return "Fan";

    if (type & SENSOR_TYPE_CPU)
        return "CPU";

    if (type & SENSOR_TYPE_REMOTE)
        return "Remote";

    if (type & SENSOR_TYPE_MEMORY)
        return "Memory";

    return "N/A";
}



void psensor_log_measures(Psensor **sensors)
{
    if (log_level == LOG_DEBUG)
    {
        if (!sensors)
            return;

        while (*sensors)
        {
            log_debug("Measure: %s %.2f",
                      (*sensors)->name,
                      psensor_get_current_value(*sensors));

            sensors++;
        }
    }
}

const Psensor **psensor_list_copy(const Psensor *const *sensors)
{
    const Psensor * *result;

    size_t n = psensor_list_size(sensors);

    result = (const Psensor * *)malloc((n + 1) * sizeof(Psensor *));
    if (result != NULL)
    {
        for (size_t i = 0; i < n; i++)
            result[i] = sensors[i];
        result[n] = NULL;
    }

    return result;
}

char *
psensor_current_value_to_str(const Psensor *s, Temperature_Unit temperature_unit)
{
    return psensor_value_to_str(s->type,
                                psensor_get_current_value(s),
                                temperature_unit);
}
