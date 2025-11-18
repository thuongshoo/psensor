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
#include <stdlib.h>
#include <string.h>

#include <ptime.h>

const int P_TIME_VER = 3;

static const int ISO8601_TIME_LENGTH = 19; /* YYYY-MM-DDThh:mm:ss */
static const int ISO8601_DATE_LENGTH = 10; /* YYYY-MM-DD */

char *time_to_ISO8601_time(time_t *t)
{
	struct tm lt;

	memset(&lt, 0, sizeof(struct tm));
	if (!gmtime_r(t, &lt))
		return NULL;

	return tm_to_ISO8601_time(&lt);
}

char *time_to_ISO8601_date(time_t *t)
{
	struct tm lt;

	memset(&lt, 0, sizeof(struct tm));
	if (!gmtime_r(t, &lt))
		return NULL;

	return tm_to_ISO8601_date(&lt);
}

char *tm_to_ISO8601_date(const struct tm *tm)
{
	char *str = malloc(ISO8601_DATE_LENGTH + 1);
	if (str == NULL)
		return NULL;

	if (strftime(str, ISO8601_DATE_LENGTH + 1, "%F", tm))
		return str;

	free(str);
	return NULL;
}

char *tm_to_ISO8601_time(const struct tm *tm)
{
	char *str = malloc(ISO8601_TIME_LENGTH + 1);
	if (str == NULL)
		return NULL;

	if (strftime(str, ISO8601_TIME_LENGTH + 1, "%FT%T", tm))
		return str;

	free(str);
	return NULL;
}

char *get_current_ISO8601_time(void)
{
	time_t t = time(NULL);
	return time_to_ISO8601_time(&t);
}

char *time_to_str(const time_t s)
{
    /* note: localtime returns a static field, no free required */
    const struct tm *tm = localtime(&s);

    if (!tm)
        return NULL;

    char *str = calloc(8, sizeof(char));

    if (str ==NULL)
    {
       return NULL;
    }        

    strftime(str, 6, "%H:%M", tm);

    return str;
}

char *time_to_str2(const time_t s)
{
    
    /* note: localtime returns a static field, no free required */
    const struct tm *tm = localtime(&s);

    if (!tm)
        return NULL;

    char *str = calloc(24, sizeof(char));

    if (str == NULL)
        return NULL;

    strftime(str, 16, "%H:%M:%S", tm);

    return str;
}

char *time_to_str3(const time_t *t)
{
	struct tm lt;
	char *str;

	if (!localtime_r(t, &lt))
		return NULL;

	str = malloc(64);

	if (strftime(str, 64, "%s", &lt))
		return str;

	free(str);
	return NULL;
}