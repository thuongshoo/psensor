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
#ifndef PSENSOR_LMSENSOR_H
#define PSENSOR_LMSENSOR_H

#include <config.h>
#include <bool.h>
#include <psensor.h>

#if defined(HAVE_LIBSENSORS) && HAVE_LIBSENSORS

static inline bool lmsensor_is_supported(void) { return true; }

size_t lmsensor_psensor_list_update(Psensor **);
void lmsensor_psensor_list_append(Psensor ***, unsigned int);
void lmsensor_cleanup(void);

#else

static inline bool lmsensor_is_supported(void) { return false; }

static inline size_t lmsensor_psensor_list_update(Psensor **s) { return 0;}
static inline void lmsensor_psensor_list_append(Psensor ***s, unsigned int n) {}
static inline void lmsensor_cleanup(void) {}

#endif

#endif
