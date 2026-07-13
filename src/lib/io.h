/*
 *  Copyright (C) 2010-2016 jeanfi@gmail.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *   02110-1301 USA
 */

#ifndef PSENSOR_IO_H
#define PSENSOR_IO_H

#include <bool.h>

// #define P_IO_VER 6

#include <sys/types.h>
#include <sys/stat.h>

/* Returns true if a given 'path' denotes a directory, false otherwise */
bool is_dir(const char *path);

/* Returns true if a given 'path' denotes a regular file, false otherwise */
bool is_file(const char *path);

/* Returns a normalized path (must be freed with free()) */
char *path_normalize(const char *dpath);

/* Returns NULL-terminated array of directory entries matching filter.
 * NULL filter means all entries. Free with paths_free().
 */
char **dir_list(const char *dpath, int (*filter)(const char *path));
void paths_free(char **paths);

/* Join directory and path (must be freed with free()) */
char *path_append(const char *dir, const char *path);

/* Returns size of file, or -1 on error */
long file_get_size(const char *path);

/* Returns file content (must be freed with free()), or NULL on error */
char *file_get_content(const char *path);

enum file_copy_error
{
    FILE_COPY_ERROR_NONE = 0,
    FILE_COPY_ERROR_OPEN_SRC,
    FILE_COPY_ERROR_OPEN_DST,
    FILE_COPY_ERROR_READ,
    FILE_COPY_ERROR_WRITE,
    FILE_COPY_ERROR_ALLOC_BUFFER
};

/* Print human-readable error for file_copy */
void file_copy_print_error(int code, const char *src, const char *dst);

/* Copy file. Returns 0 on success, error code otherwise */
int file_copy(const char *src, const char *dst);

/* Recursively copy directory */
int dir_rcopy(const char *src, const char *dst);

/* Create directories recursively */
void mkdirs(const char *dirs, mode_t mode);

#endif
