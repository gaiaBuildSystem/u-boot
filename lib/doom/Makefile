# SPDX-License-Identifier: GPL-2.0+
#
# DOOM for U-Boot. The engine (engine/) and libc shim (compat/) are built
# with warnings silenced as they are third-party GPL source.

ccflags-y := -w -fcommon -I$(srctree)/lib/doom/compat
ccflags-y += -DDOOMGENERIC_RESX=960 -DDOOMGENERIC_RESY=600

# m_fixed.c calls abs() without including a header for it, so the macro in
# dg_stdlib.h does not reach it. Define it for the whole build instead: on
# sandbox an abs() of our own would override the host C library's.
ccflags-y += -Dabs=dg_abs -Dlabs=dg_labs

obj-y += dg_uboot.o
obj-y += compat/
obj-y += engine/
