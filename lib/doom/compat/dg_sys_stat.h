/* SPDX-License-Identifier: GPL-2.0+ */
/* Minimal <sys/stat.h> replacement: mkdir() is a no-op on sandbox. */

#ifndef __DG_SYS_STAT_H
#define __DG_SYS_STAT_H

static inline int dg_mkdir_stub(const char *path, int mode)
{
	(void)path;
	(void)mode;
	return 0;
}

#define mkdir(p, m)	dg_mkdir_stub(p, m)

#endif /* __DG_SYS_STAT_H */
