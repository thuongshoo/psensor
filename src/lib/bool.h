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
#ifndef PSENSOR_BOOL_H
#define PSENSOR_BOOL_H

#include "config.h"

/* C23: bool là keyword, không cần include */
#if __STDC_VERSION__ >= 202311L
    /* bool đã có sẵn, không cần gì cả */

#elifdef HAVE_STDBOOL_H
    /* C99/C11: dùng stdbool.h */
    #include <stdbool.h>

#else
    /* C89: tự define */
    #ifndef __cplusplus
        #ifndef bool
            #define bool _Bool
        #endif
    #endif
    #ifndef true
        #define true 1
    #endif
    #ifndef false
        #define false 0
    #endif
#endif

#endif /* PSENSOR_BOOL_H */
