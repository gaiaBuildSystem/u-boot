/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Backing store for the Doom engine's file I/O
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 *
 * The engine reads its WAD and reads and writes a config file and save
 * games. Sandbox can pass these through to real host files; any other
 * board has no filesystem to hand at this point, so files live in memory
 * instead, with the WAD placed there before the game starts.
 */

#ifndef __DG_STORE_H
#define __DG_STORE_H

/* flags for dg_open(), matching the usual open(2) meanings */
enum {
	DGO_RDONLY	= 0,
	DGO_WRONLY	= 1,
	DGO_RDWR	= 2,
	DGO_MASK	= 3,

	DGO_CREAT	= 0x40,
	DGO_TRUNC	= 0x200,
};

/* whence values for dg_lseek() */
enum {
	DGS_SET,
	DGS_CUR,
	DGS_END,
};

int dg_open(const char *path, int flags);
int dg_close(int fd);
long dg_read(int fd, void *buf, unsigned long count);
long dg_write(int fd, const void *buf, unsigned long count);
long dg_lseek(int fd, long offset, int whence);
int dg_unlink(const char *path);

/**
 * dg_store_add_mem() - Present a block of memory as a file
 *
 * This is how the WAD reaches the engine on a board with no filesystem:
 * something else loads it into memory and it is named here. The memory is
 * used in place, not copied, so it must stay valid while the game runs.
 *
 * @name: Name the engine will open, e.g. "doom1.wad"
 * @buf: Contents of the file
 * @size: Number of bytes at @buf
 * Return: 0 if OK, -ENOSPC if there is no room for another file
 */
int dg_store_add_mem(const char *name, void *buf, unsigned long size);

/**
 * dg_store_reset() - Forget every file, freeing any which were written
 */
void dg_store_reset(void);

#endif /* __DG_STORE_H */
