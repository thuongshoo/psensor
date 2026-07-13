/*
 * slog.c - logging functions for sensors
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

#include "slog.h"
#include <pthread.h>
#include <locale.h>
#include <libintl.h>
#define _(str) gettext(str)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <plog.h>
#include <pmutex.h>
#include "ptime.h"

static FILE *s_file;
static double *s_last_values;
static unsigned int s_period;
static const Psensor *const *s_sensors;
static pthread_mutex_t *s_sensors_mutex;
static pthread_t s_thread;
static time_t s_start_time;
static volatile int s_slog_thread_running = 1;

static const char *DEFAULT_FILENAME = "sensors.log";

static char *get_default_path(void)
{
    char *home, *path, *dir;

    home = getenv("HOME");

    if (home)
    {
        int result = asprintf(&dir, "%s/%s", home, PACKAGE_USER_FOLDER);
        if (result == -1)
            return nullptr;
        mkdir(dir, 0777);

        result = asprintf(&path, "%s/%s", dir, DEFAULT_FILENAME);
        free(dir);
        if (result == -1)
            return nullptr;

        return path;
    }

    log_warn(_("HOME variable not set."));
    return strdup(DEFAULT_FILENAME);
}

static bool slog_open(const char *path, const Psensor *const *sensors)
{
    if (s_file)
    {
        log_err(_("Sensor log s_file already open."));
        return false;
    }

    char *lpath = nullptr;

    if (path)
    {
        lpath = strdup(path);
        if (!lpath)
        {
            log_err(_("Memory allocation failed."));
            return false;
        }
    }
    else
    {
        lpath = get_default_path();
        if (!lpath)
        {
            log_err(_("Failed to get default path."));
            return false;
        }
    }

    s_file = fopen(lpath, "a");

    if (!s_file)
        log_err(_("Cannot open sensor log s_file: %s."), lpath);

    if (!s_file)
    {
        free(lpath);
        return false;
    }

    s_start_time = time(nullptr);
    char time_string[NUMBER_OF_SECONDS_SINCE_THE_EPOCH_MAX_LENGTH];
    time_to_str3(&s_start_time, time_string, sizeof(time_string));
    fprintf(s_file, "I,%s,%s\n", time_string, VERSION);

    while (*sensors)
    {
        fprintf(s_file, "S,%s,%x\n", (*sensors)->id, (*sensors)->type);
        sensors++;
    }

    fflush(s_file);

    free(lpath);
    return true;
}

static void slog_write_sensors(const Psensor *const *sensors)
{
    struct timeval tv;
    bool is_first_call;

    if (!s_file)
    {
        log_debug(_("Sensor log s_file not open."));
        return;
    }

    gettimeofday(&tv, nullptr);

    size_t count = psensor_list_size(sensors);

    if (s_last_values)
    {
        is_first_call = false;
    }
    else
    {
        is_first_call = true;
        // to avoid malloc 0 byte in case of no sensor,
        s_last_values = malloc((count + 1) * sizeof(double));
    }

    fprintf(s_file, "%ld", (long int)(tv.tv_sec - s_start_time));
    for (size_t i = 0; i < count; i++)
    {
        double v = psensor_get_current_value(sensors[i]);

        if (!is_first_call && s_last_values[i] == v)
            fputc(',', s_file);
        else
            fprintf(s_file, ",%.1f", v);

        s_last_values[i] = v;
    }

    fputc('\n', s_file);

    fflush(s_file);
}

static int slog_activate_lock(pthread_mutex_t *m)
{
    return pmutex_lock(m);
}
static int slog_activate_unlock(pthread_mutex_t *m)
{
    return pmutex_unlock(m);
}

static void *slog_routine(void *data)
{
    pthread_setname_np(pthread_self(), "slog_routine");
    while (s_slog_thread_running)
    {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);

        slog_activate_lock(s_sensors_mutex);
        slog_write_sensors(s_sensors);
        slog_activate_unlock(s_sensors_mutex);

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);

        sleep(s_period);
    }
    return nullptr;
}

void slog_close(void)
{
    if (s_file)
    {
        s_slog_thread_running = 0;       // signal to stop s_thread
        pthread_join(s_thread, nullptr); // wait for s_thread to finish

        fclose(s_file);
        s_file = nullptr;
        free(s_last_values);
        s_last_values = nullptr;
    }
    else
    {
        log_debug(_("Sensor log not open, cannot close."));
    }
}

bool slog_activate(const char *path,
                   const Psensor *const *ss,
                   pthread_mutex_t *mutex,
                   unsigned int p)
{
    bool ret;

    s_sensors = ss;
    s_sensors_mutex = mutex;
    s_period = p;

    slog_activate_lock(s_sensors_mutex);
    ret = slog_open(path, s_sensors);
    slog_activate_unlock(s_sensors_mutex);

    if (ret)
        pthread_create(&s_thread, nullptr, slog_routine, nullptr);

    return ret;
}
