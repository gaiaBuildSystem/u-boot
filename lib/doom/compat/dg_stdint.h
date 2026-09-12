/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * <stdint.h> replacement for the Doom engine. U-Boot's own <stdint.h> is
 * a deliberate no-op (it uses <linux/types.h>), which would also reintroduce
 * type clashes, so define the fixed-width types here from compiler builtins.
 */

#ifndef __DG_STDINT_H
#define __DG_STDINT_H

typedef signed char		int8_t;
typedef unsigned char		uint8_t;
typedef short			int16_t;
typedef unsigned short		uint16_t;
typedef int			int32_t;
typedef unsigned int		uint32_t;
typedef long long		int64_t;
typedef unsigned long long	uint64_t;

typedef long			intptr_t;
typedef unsigned long		uintptr_t;

#define INT8_MAX	0x7f
#define UINT8_MAX	0xff
#define INT16_MAX	0x7fff
#define UINT16_MAX	0xffff
#define INT32_MAX	0x7fffffff
#define UINT32_MAX	0xffffffffU

#endif /* __DG_STDINT_H */
