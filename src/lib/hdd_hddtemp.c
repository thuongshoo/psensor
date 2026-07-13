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

/* Part of the code in this file is based on GNOME sensors applet code
 * hddtemp-plugin.c see http://sensors-applet.sourceforge.net/
 */
#include <hdd.h>
#include <locale.h>
#include <libintl.h>
#define _(str) gettext(str)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include <psensor.h>

static const char *PROVIDER_NAME = "hddtemp";

static const char *HDDTEMP_SERVER_IP_ADDRESS = "127.0.0.1";
static const int HDDTEMP_PORT_NUMBER = 7634;
static const size_t HDDTEMP_OUTPUT_BUFFER_LENGTH = 4048;

struct hdd_info
{
    int temp;
    char *name;
};

static char *fetch(void)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        log_err(_("%s: failed to open socket."), PROVIDER_NAME);
        return nullptr;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(HDDTEMP_SERVER_IP_ADDRESS);
    address.sin_port = htons(HDDTEMP_PORT_NUMBER);

    char *buffer = nullptr;

    if (connect(sockfd,
                (struct sockaddr *)&address,
                (socklen_t)sizeof(address)) == -1)
    {
        log_err(_("%s: failed to open connection."), PROVIDER_NAME);
    }
    else
    {
        buffer = malloc(HDDTEMP_OUTPUT_BUFFER_LENGTH);
        if (buffer == nullptr)
        { /* Thêm: kiểm tra malloc */
            log_err(_("%s: memory allocation failed."), PROVIDER_NAME);
            close(sockfd);
            return nullptr;
        }

        char *pc = buffer;
        ssize_t n;
        size_t output_length = 0;
        while (output_length < HDDTEMP_OUTPUT_BUFFER_LENGTH - 1 && (n = read(sockfd,
                                                                             pc,
                                                                             HDDTEMP_OUTPUT_BUFFER_LENGTH -
                                                                                 output_length)) > 0)
        {

            output_length += (size_t)n;  /* Ép kiểu tường minh */
            pc = buffer + output_length; /* Sửa: dùng buffer thay vì &pc[n] */
        }

        buffer[output_length] = '\0';
    }

    close(sockfd);

    return buffer;
}

static int str_index(const char *str, char d) /* Thêm const */
{
    const char *c;
    int i;

    if (str == nullptr || *str == '\0') /* Dùng == nullptr thay vì !str */
        return -1;

    c = str;
    i = 0;

    while (*c != '\0')
    {
        if (*c == d)
            return i;
        i++;
        c++;
    }

    return -1;
}

static Psensor *
create_sensor(char *id, char *name, unsigned int values_max_length)
{
    unsigned int t;

    t = SENSOR_TYPE_HDD | SENSOR_TYPE_HDDTEMP | SENSOR_TYPE_TEMP;

    char *chip = strdup(_("Disk"));
    if (chip == nullptr) /* Thêm: kiểm tra strdup */
        return nullptr;

    Psensor *tmp_psensor = psensor_create(id, name, chip,
                                          t,
                                          values_max_length);
    if (tmp_psensor == nullptr)
    {
        free(chip);
        return nullptr;
    }
    return tmp_psensor;
}

static char *next_hdd_info(char *string, struct hdd_info *info)
{
    char *c;
    int idx_name_n, i;
    long temp_long; /* Sửa: dùng long cho strtol */
    char *endptr;

    if (string == nullptr || strlen(string) <= 5 /* Sửa: == nullptr */
        || string[0] != '|')
        return nullptr;

    /* skip first pipe */
    c = string + 1;

    /* name */
    idx_name_n = str_index(c, '|');

    if (idx_name_n == -1)
        return nullptr;
    c = c + idx_name_n + 1;

    /* skip label */
    i = str_index(c, '|');
    if (i == -1)
        return nullptr;
    c = c + i + 1;

    /* temp - Sửa: dùng strtol thay vì atoi */
    i = str_index(c, '|');
    if (i == -1)
        return nullptr;

    errno = 0;
    temp_long = strtol(c, &endptr, 10);
    if (errno != 0 || endptr == c || temp_long > INT_MAX || temp_long < INT_MIN)
    {
        log_err(_("%s: invalid temperature value."), PROVIDER_NAME);
        return nullptr;
    }
    c = c + i + 1;

    /* skip unit  */
    i = str_index(c, '|');
    if (i == -1)
        return nullptr;
    c = c + i + 1;

    info->name = malloc((size_t)idx_name_n + 1); /* Ép kiểu tường minh */
    if (info->name == nullptr)
    { /* Thêm: kiểm tra malloc */
        log_err(_("%s: memory allocation failed."), PROVIDER_NAME);
        return nullptr;
    }
    strncpy(info->name, string + 1, (size_t)idx_name_n);
    info->name[idx_name_n] = '\0';

    info->temp = (int)temp_long; /* Ép kiểu tường minh */

    return c;
}

void hddtemp_psensor_list_append(Psensor ***sensors, unsigned int values_max_length)
{
    char *hddtemp_output = fetch();
    if (hddtemp_output == nullptr)
        return;

    if (hddtemp_output[0] != '|')
    {
        log_err(_("%s: wrong string: %s."),
                PROVIDER_NAME,
                hddtemp_output);

        free(hddtemp_output);
        return;
    }

    char *c = hddtemp_output;

    struct hdd_info info;
    info.name = nullptr; /* Khởi tạo để an toàn */
    info.temp = 0;

    while (c && (c = next_hdd_info(c, &info)))
    {
        size_t id_len = strlen(PROVIDER_NAME) + 1 + strlen(info.name) + 1;
        char *id = malloc(id_len);
        if (id == nullptr)
        { /* Thêm: kiểm tra malloc */
            free(info.name);
            continue;
        }
        sprintf(id, "%s %s", PROVIDER_NAME, info.name);

        Psensor *sensor = create_sensor(id, info.name, values_max_length);
        if (sensor != nullptr)
        {
            psensor_list_append(sensors, sensor);
        }
        free(id);        /* Luôn free id */
        free(info.name); /* Luôn free info.name */
    }

    free(hddtemp_output);
}

static void update(Psensor **sensors, const struct hdd_info *info)
{
    while (*sensors)
    {
        if (!((*sensors)->type & SENSOR_TYPE_REMOTE) && (*sensors)->type & SENSOR_TYPE_HDDTEMP && !strcmp((*sensors)->id + 8, info->name))
            psensor_set_current_value(*sensors,
                                      (double)info->temp);

        sensors++;
    }
}

static bool contains_hddtemp_sensor(Psensor **sensors)
{
    if (sensors == nullptr)
        return false;

    while (*sensors)
    {
        const Psensor *s = *sensors;
        if (!(s->type & SENSOR_TYPE_REMOTE) && (s->type & SENSOR_TYPE_HDDTEMP))
            return true;
        sensors++;
    }

    return false;
}

void hddtemp_psensor_list_update(Psensor **sensors)
{
    char *hddtemp_output;

    if (!contains_hddtemp_sensor(sensors))
        return;

    hddtemp_output = fetch();

    if (hddtemp_output == nullptr)
        return;

    if (hddtemp_output[0] == '|')
    {
        char *c = hddtemp_output;
        struct hdd_info info;

        info.name = nullptr;
        info.temp = 0;

        while (c && (c = next_hdd_info(c, &info)))
        {

            update(sensors, &info);

            free(info.name);
        }
    }
    else
    {
        log_err(_("%s: wrong string: %s."),
                PROVIDER_NAME,
                hddtemp_output);
    }

    free(hddtemp_output);
}
