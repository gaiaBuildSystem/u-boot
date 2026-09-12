/* SPDX-License-Identifier: GPL-2.0+ */
/* Minimal <assert.h> replacement routed through Doom's I_Error(). */

#ifndef __DG_ASSERT_H
#define __DG_ASSERT_H

void I_Error(char *error, ...);

#define assert(x) \
	do { \
		if (!(x)) \
			I_Error("Assertion '%s' failed at %s:%d", \
				#x, __FILE__, __LINE__); \
	} while (0)

#endif /* __DG_ASSERT_H */
