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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE 1
#endif

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

static FILE *file;
static double *s_last_values;
static unsigned int period;
static const Psensor *const *s_sensors;
static pthread_mutex_t *sensors_mutex;
static pthread_t thread;
static time_t st;
static volatile int slog_thread_running = 1;

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
    if (file)
    {
        log_err(_("Sensor log file already open."));
        return false;
    }

    char *lpath = path ? (char *)path : get_default_path();

    file = fopen(lpath, "a");

    if (!file)
        log_err(_("Cannot open sensor log file: %s."), lpath);

    if (!path)
        free(lpath);

    if (!file)
        return false;

    st = time(nullptr);
    char *time = time_to_str3(&st);
    fprintf(file, "I,%s,%s\n", time, VERSION);
    free(time);

    while (*sensors)
    {
        fprintf(file, "S,%s,%x\n", (*sensors)->id, (*sensors)->type);
        sensors++;
    }

    fflush(file);

    return true;
}

static void slog_write_sensors(const Psensor *const *sensors)
{
    struct timeval tv;
    bool is_first_call;

    if (!file)
    {
        log_debug(_("Sensor log file not open."));
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

    fprintf(file, "%ld", (long int)(tv.tv_sec - st));
    for (size_t i = 0; i < count; i++)
    {
        double v = psensor_get_current_value(sensors[i]);

        if (!is_first_call && s_last_values[i] == v)
            fputc(',', file);
        else
            fprintf(file, ",%.1f", v);

        s_last_values[i] = v;
    }

    fputc('\n', file);

    fflush(file);
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
    while (slog_thread_running)
    {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);

        slog_activate_lock(sensors_mutex);
        slog_write_sensors(s_sensors);
        slog_activate_unlock(sensors_mutex);

        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);

        sleep(period);
    }
    return nullptr;
}

void slog_close(void)
{
    if (file)
    {
        slog_thread_running = 0;       // signal to stop thread
        pthread_join(thread, nullptr); // wait for thread to finish

        fclose(file);
        file = nullptr;
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
    sensors_mutex = mutex;
    period = p;

    slog_activate_lock(sensors_mutex);
    ret = slog_open(path, s_sensors);
    slog_activate_unlock(sensors_mutex);

    if (ret)
        pthread_create(&thread, nullptr, slog_routine, nullptr);

    return ret;
}
