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
#include <ptime.h>

#include <stdlib.h>
#include <string.h>
const int P_TIME_VER = 3;

static const int ISO8601_DATE_LENGTH = 10; /* YYYY-MM-DD */

static bool time_to_ISO8601_string(char *stringBuffer, time_t *t)
{
    struct tm local_time;

    memset(&local_time, 0, sizeof(struct tm));
    if (!gmtime_r(t, &local_time))
        return false;

    strftime(stringBuffer, ISO8601_TIME_LENGTH, "%FT%T", &local_time);

    return true;
}

char *time_to_ISO8601_date(time_t *t)
{
    struct tm lt;

    memset(&lt, 0, sizeof(struct tm));
    if (!gmtime_r(t, &lt))
        return nullptr;

    return tm_to_ISO8601_date(&lt);
}

char *tm_to_ISO8601_date(const struct tm *tm)
{
    char *str = malloc(ISO8601_DATE_LENGTH + 1);
    if (str == nullptr)
        return nullptr;

    if (strftime(str, ISO8601_DATE_LENGTH + 1, "%F", tm))
        return str;

    free(str);
    return nullptr;
}

bool get_current_ISO8601_time(char *stringBuffer)
{
    time_t t = time(nullptr);
    return time_to_ISO8601_string(stringBuffer, &t);
}

void time_to_string_buffer(time_t s, char *buffer, size_t buffer_size)
{
    /* note: localtime returns a static field, no free required */
    const struct tm *tm = localtime(&s);

    strftime(buffer, buffer_size, "%H:%M", tm);
}

char *time_to_str2(const time_t *t)
{
    /* note: localtime returns a static field, no free required */
    const struct tm *tm = localtime(t);

    if (!tm)
        return nullptr;

    char *str = calloc(24, sizeof(char));

    if (str == nullptr)
        return nullptr;

    strftime(str, 16, "%H:%M:%S", tm);

    return str;
}

bool time_to_str3(const time_t *t, char *stringBuffer, size_t bufferSize)
{
    struct tm localTime;

    if (!localtime_r(t, &localTime))
        return false;

#ifdef _WIN32
    // Windows approach: format the standard text, then append the timestamp manually
    size_t len = strftime(stringBuffer, bufferSize, "%Y-%m-%d", t);
    snprintf(stringBuffer + len, bufferSize - len, "%lld", (long long)localTime);
#else
    // Linux/POSIX approach
    strftime(stringBuffer, bufferSize, "%s", &localTime);
#endif

    return true;
}
