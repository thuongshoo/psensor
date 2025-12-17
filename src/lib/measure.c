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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "measure.h"

struct measure *measures_double_create(size_t size)
{
	size_t i;
	struct measure *result = (struct measure *)malloc(size * sizeof(struct measure));
	//thanh printf("measures_double_create=%lu\n", size);
    if (result != NULL)
    {
    	for (i = 0; i < size; i++) {
	    	result[i].value = UNKNOWN_DOUBLE_VALUE;
		    timerclear(&result[i].time);
	    }
    }

	return result;
}

void measures_free(struct measure *measures)
{
	free(measures);
}

void measure_copy(const struct measure *src, struct measure *dst)
{
	memcpy(dst, src, sizeof(struct measure));
}

// measures.c - PRIVATE IMPLEMENTATION


// measures_t* measures_create(size_t max_length) {
//     measures_t* m = malloc(sizeof(measures_t));
    
// 	m->array = (struct measure *)malloc(max_length * sizeof(struct measure));
// 	for (size_t i = 0; i < max_length; i++) {
// 		m->array[i].value = UNKNOWN_DOUBLE_VALUE;
// 		timerclear(&m->array[i].time);
// 	}

//     m->max_length = max_length;
//     m->head = 0;
//     m->count = 0;
//     m->wrapped = false;
//     return m;
// }

// void measures_add(measures_t* m, double value, struct timeval time) {
//     // Circular buffer implementation
//     m->head = (m->head + 1) % m->max_length;
//     m->array[m->head].value = value;
//     m->array[m->head].time = time;
    
//     if (m->count < m->max_length) m->count++;
//     else m->wrapped = true;
// }

// double measures_get_value(const measures_t* m, int index_from_newest) {
//     if (index_from_newest >= m->count) return UNKNOWN_DOUBLE_VALUE;
    
//     int actual_idx = m->head - index_from_newest;
//     if (actual_idx < 0) actual_idx += m->max_length;
    
//     return m->array[actual_idx].value;
// }
