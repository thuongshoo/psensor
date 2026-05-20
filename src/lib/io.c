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
#ifndef _LARGEFILE_SOURCE
	#define _LARGEFILE_SOURCE 1
#endif
#include <io.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>


#include <plog.h>


/* Directory separator */
#if defined(__MINGW32__)
#define DIRSEP ('\\')
#else
#define DIRSEP '/'
#endif

#define FCOPY_BUF_SZ 4096U

/* ================================================================
 * Path checking
 * ================================================================ */

bool is_dir(const char *path)
{
    struct stat st;
    
    if (lstat(path, &st) != 0)
        return false;
    
    return S_ISDIR(st.st_mode);
}

bool is_file(const char *path)
{
    struct stat st;
    
    if (lstat(path, &st) != 0)
        return false;
    
    return S_ISREG(st.st_mode);
}

/* ================================================================
 * Path normalization
 * ================================================================ */
static char *dir_normalize(const char *dpath)
{
	if (dpath == nullptr || dpath[0] == '\0')
		return nullptr;

	char *npath = strdup(dpath);
	if (npath == nullptr)
		return nullptr;

	size_t n = strlen(npath);
	if (n > 1 && npath[n - 1] == '/')
		npath[n - 1] = '\0';

	return npath;
}

char *path_normalize(const char *dpath)
{
	return dir_normalize(dpath);
}

/* ================================================================
 * Path joining
 * ================================================================ */

char *path_append(const char *dir, const char *path)
{
	char *ndir = dir_normalize(dir);

	/* Both nullptr or empty? */
	if (ndir == nullptr && (path == nullptr || path[0] == '\0')) {
		return nullptr;
	}

	/* Only path? */
	if (ndir == nullptr)
		return strdup(path);

	/* Only dir? */
	if (path == nullptr || path[0] == '\0')
		return ndir;

	/* Join dir + '/' + path */
	size_t len = strlen(ndir) + 1 + strlen(path) + 1;
	char *ret = (char *)malloc(len);
	if (ret == nullptr) {
		free(ndir);
		return nullptr;
	}

	snprintf(ret, len, "%s/%s", ndir, path);
	free(ndir);

	return ret;
}

/* ================================================================
 * Directory listing
 * ================================================================ */

static char **paths_add(char **paths, size_t n, char *path)
{
	size_t new_size = (n + 1) * sizeof(char *);
	char **result = (char **)realloc((void*)paths, new_size);
	if (result == nullptr) {
		free(path);
		return nullptr;
	}

	/* Shift existing entries right, insert new path at front */
	(void)memmove((void*) (result + 1), (void*)result, n * sizeof(char *));
	result[0] = path;
	
	return result;
}

char **dir_list(const char *dpath, int (*filter)(const char *))
{
	DIR *dir = opendir(dpath);
	if (dir == nullptr)
		return nullptr;

	/* Allocate sentinel nullptr */
	char **paths = (char **)calloc(1, sizeof(char *));
	if (paths == nullptr) {
		closedir(dir);
		return nullptr;
	}

	size_t n = 0;
	struct dirent *ent;

	while ((ent = readdir(dir)) != nullptr) {
		const char *name = ent->d_name;

		/* Skip . and .. */
		if (name[0] == '.' && (name[1] == '\0' || 
		                       (name[1] == '.' && name[2] == '\0')))
			continue;

		char *path = path_append(dpath, name);
		if (path == nullptr)
			continue;

		if (filter == nullptr || filter(path)) {
			char **tmp = paths_add(paths, n, path);
			if (tmp == nullptr) {
				/* Memory error: free all and bail */
				closedir(dir);
				paths_free(paths);
				return nullptr;
			}
			paths = tmp;
			n++;
		} else {
			free(path);
		}
	}

	closedir(dir);
	return paths;
}

void paths_free(char **paths)
{
	if (paths == nullptr)
		return;

	for (char **p = paths; *p != nullptr; p++)
		free(*p);

	free((void*)paths);
}

/* ================================================================
 * File size and content
 * ================================================================ */

long file_get_size(const char *path)
{
	if (!is_file(path))
		return -1;

	FILE *fp = fopen(path, "rb");
	if (fp == nullptr)
		return -1;

	long size = -1;
	
	if (fseek(fp, 0, SEEK_END) == 0) {
		size = ftell(fp);
		if (size < 0)
			size = -1;
	}

	fclose(fp);
	return size;
}

char *file_get_content(const char *fpath)
{
	long size = file_get_size(fpath);
	
	if (size < 0)
		return nullptr;

	/* Empty file */
	if (size == 0) {
		char *page = (char *)malloc(1);
		if (page != nullptr)
			*page = '\0';
		return page;
	}

	FILE *fp = fopen(fpath, "rb");
	if (fp == nullptr)
		return nullptr;

	char *page = (char *)malloc((size_t)size + 1);
	if (page == nullptr) {
		fclose(fp);
		return nullptr;
	}

	size_t bytes_read = fread(page, 1, (size_t)size, fp);
	fclose(fp);

	if (bytes_read != (size_t)size) {
		free(page);
		return nullptr;
	}

	page[size] = '\0';
	return page;
}

/* ================================================================
 * File copy
 * ================================================================ */

static int FILE_copy(FILE *src, FILE *dst)
{
	int ret = 0;
	char *buf = malloc(FCOPY_BUF_SZ);

	if (nullptr == buf)
		return FILE_COPY_ERROR_ALLOC_BUFFER;

	while (0 == ret)
	{
		size_t n = fread(buf, 1, FCOPY_BUF_SZ, src);
		if (n == FCOPY_BUF_SZ)
		{
			if (fwrite(buf, 1, n, dst) != n)
				ret = FILE_COPY_ERROR_WRITE;
		}
		else
		{
			if (!feof(src) || ferror(src))
				ret = FILE_COPY_ERROR_READ;
			else
				break;
		}
	}

	free(buf);

	return ret;
}

int file_copy(const char *src, const char *dst)
{
	log_functionname("copy %s to %s", src, dst);

	FILE *fsrc = fopen(src, "rb");
	if (fsrc == nullptr)
		return FILE_COPY_ERROR_OPEN_SRC;

	FILE *fdst = fopen(dst, "wb");
	if (fdst == nullptr) {
		fclose(fsrc);
		return FILE_COPY_ERROR_OPEN_DST;
	}

	int ret = FILE_copy(fsrc, fdst);
	
	fclose(fdst);
	fclose(fsrc);
	
	return ret;
}

void file_copy_print_error(int code, const char *src, const char *dst)
{
	switch (code) {
	case FILE_COPY_ERROR_NONE:
		break;
	case FILE_COPY_ERROR_OPEN_SRC:
		(void)fprintf(stderr, "File copy error: failed to open %s.\n", src);
		break;
	case FILE_COPY_ERROR_OPEN_DST:
		(void)fprintf(stderr, "File copy error: failed to open %s.\n", dst);
		break;
	case FILE_COPY_ERROR_READ:
		(void)fprintf(stderr, "File copy error: failed to read %s.\n", src);
		break;
	case FILE_COPY_ERROR_WRITE:
		(void)fprintf(stderr, "File copy error: failed to write %s.\n", dst);
		break;
	case FILE_COPY_ERROR_ALLOC_BUFFER:
		(void)fprintf(stderr, "File copy error: failed to allocate buffer.\n");
		break;
	default:
		(void)fprintf(stderr, "File copy error: unknown error %d.\n", code);
		break;
	}
}

/* ================================================================
 * Directory creation
 * ================================================================ */

void mkdirs(const char *dirs, mode_t mode)
{
	log_functionname("mkdirs %s", dirs);

	if (dirs == nullptr || dirs[0] == '\0')
		return;

	char *dir = strdup(dirs);
	if (dir == nullptr)
		return;

	size_t len = strlen(dir);
	
	/* Create parent directories */
	for (size_t i = 1; i < len; i++) {
		if (dir[i] == DIRSEP) {
			dir[i] = '\0';
			(void)mkdir(dir, mode);
			dir[i] = DIRSEP;
		}
	}

	/* Create final directory */
	(void)mkdir(dir, mode);
	
	free(dir);
}
