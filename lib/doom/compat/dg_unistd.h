/* SPDX-License-Identifier: GPL-2.0+ */
/* Minimal <unistd.h> replacement for the Doom engine in U-Boot. */

#ifndef __DG_UNISTD_H
#define __DG_UNISTD_H

#include <stddef.h>

int dg_remove(const char *path);
#define unlink(p)	dg_remove(p)

#endif /* __DG_UNISTD_H */
