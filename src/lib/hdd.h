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
#ifndef PSENSOR_HDD_H
#define PSENSOR_HDD_H

#include <bool.h>
#include <config.h>
#include <psensor.h>

#if defined(HAVE_ATASMART) && HAVE_ATASMART

static inline bool atasmart_is_supported(void) { return true; }

void atasmart_psensor_list_append(struct psensor ***, unsigned int);
void atasmart_psensor_list_update(struct psensor **);

#else

static inline bool atasmart_is_supported(void) { return false; }

static inline void atasmart_psensor_list_append(struct psensor ***s, unsigned int n) {}
static inline void atasmart_psensor_list_update(struct psensor **s) {}

#endif

void hddtemp_psensor_list_append(struct psensor ***sensors, unsigned int values_length);
void hddtemp_psensor_list_update(struct psensor **sensors);

#endif
