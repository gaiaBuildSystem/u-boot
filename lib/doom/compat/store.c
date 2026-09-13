// SPDX-License-Identifier: GPL-2.0+
/*
 * Backing store for the Doom engine's file I/O
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 *
 * Sandbox hands the calls to the host, so that the WAD, the config file
 * and save games are real files. Every other board keeps them in memory:
 * the WAD is placed there before the game starts and anything the engine
 * writes lasts until the command exits.
 */

#include <linux/types.h>
#include <linux/string.h>
#include <malloc.h>
#include <errno.h>

#include "dg_store.h"

#ifdef CONFIG_SANDBOX

#include <os.h>

/* the OS_* values are the host's, so translate rather than assume */
static int to_os_flags(int flags)
{
	int os_flags;

	switch (flags & DGO_MASK) {
	case DGO_WRONLY:
		os_flags = OS_O_WRONLY;
		break;
	case DGO_RDWR:
		os_flags = OS_O_RDWR;
		break;
	default:
		os_flags = OS_O_RDONLY;
		break;
	}
	if (flags & DGO_CREAT)
		os_flags |= OS_O_CREAT;
	if (flags & DGO_TRUNC)
		os_flags |= OS_O_TRUNC;

	return os_flags;
}

int dg_open(const char *path, int flags)
{
	return os_open(path, to_os_flags(flags));
}

int dg_close(int fd)
{
	return os_close(fd);
}

long dg_read(int fd, void *buf, unsigned long count)
{
	return os_read(fd, buf, count);
}

long dg_write(int fd, const void *buf, unsigned long count)
{
	return os_write(fd, buf, count);
}

long dg_lseek(int fd, long offset, int whence)
{
	int os_whence;

	switch (whence) {
	case DGS_CUR:
		os_whence = OS_SEEK_CUR;
		break;
	case DGS_END:
		os_whence = OS_SEEK_END;
		break;
	default:
		os_whence = OS_SEEK_SET;
		break;
	}

	return os_lseek(fd, offset, os_whence);
}

int dg_unlink(const char *path)
{
	return os_unlink(path);
}

int dg_store_add_mem(const char *name, void *buf, unsigned long size)
{
	return 0;
}

void dg_store_reset(void)
{
}

#else /* !CONFIG_SANDBOX */

/* the WAD, a config file and a few save slots */
#define STORE_MAX_FILES		12
#define STORE_NAME_LEN		32

/**
 * struct store_file - A file held in memory
 *
 * @name: Name the engine opens it by
 * @buf: Contents, either supplied by dg_store_add_mem() or allocated here
 * @size: Number of valid bytes in @buf
 * @alloc: Number of bytes available at @buf, 0 if the memory is not ours
 * @pos: Current read/write position
 * @used: true if this slot holds a file
 * @open: true if the file is open, in which case @pos is meaningful
 */
struct store_file {
	char name[STORE_NAME_LEN];
	u8 *buf;
	unsigned long size;
	unsigned long alloc;
	unsigned long pos;
	bool used;
	bool open;
};

static struct store_file store_files[STORE_MAX_FILES];

/* strip any directory part, since there are no directories here */
static const char *basename_of(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

static struct store_file *find_file(const char *path)
{
	const char *name = basename_of(path);
	int i;

	for (i = 0; i < STORE_MAX_FILES; i++) {
		struct store_file *file = &store_files[i];

		if (file->used && !strcmp(file->name, name))
			return file;
	}

	return NULL;
}

static struct store_file *alloc_slot(const char *path)
{
	int i;

	for (i = 0; i < STORE_MAX_FILES; i++) {
		struct store_file *file = &store_files[i];

		if (!file->used) {
			strlcpy(file->name, basename_of(path),
				STORE_NAME_LEN);
			file->used = true;

			return file;
		}
	}

	return NULL;
}

/* fd 0, 1 and 2 belong to the console, so files start above them */
#define FD_BASE		3

static struct store_file *file_from_fd(int fd)
{
	int idx = fd - FD_BASE;

	if (idx < 0 || idx >= STORE_MAX_FILES)
		return NULL;
	if (!store_files[idx].open)
		return NULL;

	return &store_files[idx];
}

static int fd_of(struct store_file *file)
{
	return (file - store_files) + FD_BASE;
}

/* make sure at least @need bytes can be written at @pos */
static int make_room(struct store_file *file, unsigned long need)
{
	unsigned long want = file->pos + need;
	unsigned long alloc;
	u8 *buf;

	if (want <= file->alloc)
		return 0;

	/* grow in reasonable steps rather than on every write */
	alloc = file->alloc ? file->alloc : 4096;
	while (alloc < want)
		alloc *= 2;

	buf = realloc(file->buf, alloc);
	if (!buf)
		return -ENOMEM;
	memset(buf + file->size, '\0', alloc - file->size);
	file->buf = buf;
	file->alloc = alloc;

	return 0;
}

int dg_open(const char *path, int flags)
{
	struct store_file *file = find_file(path);

	if (!file) {
		/*
		 * Only a file being created can be missing. The engine opens
		 * its config file and save games speculatively and copes with
		 * them not being there
		 */
		if (!(flags & DGO_CREAT))
			return -ENOENT;
		file = alloc_slot(path);
		if (!file)
			return -ENOSPC;
	}
	if (file->open)
		return -EBUSY;

	if (flags & DGO_TRUNC) {
		/* memory given to us is read-only, so start a copy of our own */
		if (!file->alloc)
			file->buf = NULL;
		file->size = 0;
	}
	file->pos = 0;
	file->open = true;

	return fd_of(file);
}

int dg_close(int fd)
{
	struct store_file *file = file_from_fd(fd);

	if (!file)
		return -EBADF;
	file->open = false;

	return 0;
}

long dg_read(int fd, void *buf, unsigned long count)
{
	struct store_file *file = file_from_fd(fd);
	unsigned long avail;

	if (!file)
		return -EBADF;
	avail = file->pos < file->size ? file->size - file->pos : 0;
	if (count > avail)
		count = avail;
	memcpy(buf, file->buf + file->pos, count);
	file->pos += count;

	return count;
}

long dg_write(int fd, const void *buf, unsigned long count)
{
	struct store_file *file = file_from_fd(fd);
	int ret;

	if (!file)
		return -EBADF;
	ret = make_room(file, count);
	if (ret)
		return ret;
	memcpy(file->buf + file->pos, buf, count);
	file->pos += count;
	if (file->pos > file->size)
		file->size = file->pos;

	return count;
}

long dg_lseek(int fd, long offset, int whence)
{
	struct store_file *file = file_from_fd(fd);
	long pos;

	if (!file)
		return -EBADF;
	switch (whence) {
	case DGS_CUR:
		pos = file->pos + offset;
		break;
	case DGS_END:
		pos = file->size + offset;
		break;
	default:
		pos = offset;
		break;
	}
	if (pos < 0)
		return -EINVAL;
	file->pos = pos;

	return pos;
}

int dg_unlink(const char *path)
{
	struct store_file *file = find_file(path);

	if (!file)
		return -ENOENT;
	if (file->alloc)
		free(file->buf);
	memset(file, '\0', sizeof(*file));

	return 0;
}

int dg_store_add_mem(const char *name, void *buf, unsigned long size)
{
	struct store_file *file = alloc_slot(name);

	if (!file)
		return -ENOSPC;
	file->buf = buf;
	file->size = size;
	file->alloc = 0;		/* not ours to free or grow */

	return 0;
}

void dg_store_reset(void)
{
	int i;

	for (i = 0; i < STORE_MAX_FILES; i++) {
		struct store_file *file = &store_files[i];

		if (file->used && file->alloc)
			free(file->buf);
		memset(file, '\0', sizeof(*file));
	}
}

#endif /* CONFIG_SANDBOX */
