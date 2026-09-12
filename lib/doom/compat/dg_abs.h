/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Prototypes for the abs() the engine uses without declaring it
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 *
 * m_fixed.c calls abs() with no header in scope. The call is redirected to
 * dg_abs() on the command line, which needs a prototype to be visible as
 * well, since a compiler is entitled to reject an undeclared call. This
 * holds only those two, so that forcing it into every engine file does not
 * drag in the rest of dg_stdlib.h, whose rand() disagrees with U-Boot's.
 */

#ifndef __DG_ABS_H
#define __DG_ABS_H

int dg_abs(int n);
long dg_labs(long n);

#endif /* __DG_ABS_H */
