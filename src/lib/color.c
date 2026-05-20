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
#include "color.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

void color_set(struct color *c, color_channel_datatype_t r, color_channel_datatype_t g, color_channel_datatype_t b)
{
	c->red = r;
	c->green = g;
	c->blue = b;
}

struct color *color_new(color_channel_datatype_t r, color_channel_datatype_t g, color_channel_datatype_t b)
{
	struct color *color = malloc(sizeof(struct color));
	if (color == nullptr)
		return nullptr;

	color_set(color, r, g, b);
	return color;
}

struct color *color_dup(const struct color *color)
{
	return color_new(color->red, color->green, color->blue);
}

bool is_color(const char *str)
{
	if (str == nullptr)
		return false;

	size_t n = strlen(str);

	if (n != 13 || str[0] != '#')
		return false;

	for (size_t i = 1; i < n; i++) {
		if (!isxdigit((unsigned char)str[i]))
			return false;
	}

	return true;
}

struct color *str_to_color(const char *str)
{
	if (!is_color(str))
		return nullptr;

	/* Parse RRGGBB components directly without strncpy/strtol */
	unsigned int red, green, blue;
	
	/* #RRRRGGGGBBBB format: 4 hex digits per component */
	const char r_str[5] = {str[1], str[2], str[3], str[4], '\0'};
	const char g_str[5] = {str[5], str[6], str[7], str[8], '\0'};
	const char b_str[5] = {str[9], str[10], str[11], str[12], '\0'};
	
	errno = 0;
	char *endptr;
	
	red = (unsigned int)strtoul(r_str, &endptr, 16);
	if (errno != 0 || *endptr != '\0')
		return nullptr;
	
	green = (unsigned int)strtoul(g_str, &endptr, 16);
	if (errno != 0 || *endptr != '\0')
		return nullptr;
	
	blue = (unsigned int)strtoul(b_str, &endptr, 16);
	if (errno != 0 || *endptr != '\0')
		return nullptr;

	return color_new((color_channel_datatype_t)red / NUMBER_MAX_UINT_32,
	                 (color_channel_datatype_t)green / NUMBER_MAX_UINT_32,
	                 (color_channel_datatype_t)blue / NUMBER_MAX_UINT_32);
}

static inline color_channel_datatype_t clamp_color_channel_datatype_t(color_channel_datatype_t value, color_channel_datatype_t min, color_channel_datatype_t max)
{
	if (value < min)
		return min;
	if (value > max)
		return max;
	return value;
}

char *color_to_str(const struct color *color)
{
	if (color == nullptr)
		return nullptr;

	char *str = nullptr;
	
	/* Clamp values to [0.0, 1.0] */
	color_channel_datatype_t r = clamp_color_channel_datatype_t(color->red, NUMBER_ZERO, NUMBER_ONE);
	color_channel_datatype_t g = clamp_color_channel_datatype_t(color->green, NUMBER_ZERO, NUMBER_ONE);
	color_channel_datatype_t b = clamp_color_channel_datatype_t(color->blue, NUMBER_ZERO, NUMBER_ONE);
	
	int result = asprintf(&str, "#%04x%04x%04x",
		(unsigned int)( (NUMBER_MAX_UINT_32 * r) + NUMBER_ZERO_POINT_FIVE),
		(unsigned int)( (NUMBER_MAX_UINT_32 * g) + NUMBER_ZERO_POINT_FIVE),
		(unsigned int)( (NUMBER_MAX_UINT_32 * b) + NUMBER_ZERO_POINT_FIVE));
	
	if (result == -1) {
		return nullptr;
	}
	
	return str;
}
