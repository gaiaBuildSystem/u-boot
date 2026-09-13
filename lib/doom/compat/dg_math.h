/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * <math.h> replacement. Doom is almost entirely fixed-point; only a
 * handful of double-precision calls are made at start-up to build the
 * projection tables. These are implemented in libm.c.
 */

#ifndef __DG_MATH_H
#define __DG_MATH_H

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

double sin(double x);
double cos(double x);
double tan(double x);
double atan(double x);
double atan2(double y, double x);
double floor(double x);
double ceil(double x);
double fabs(double x);
double sqrt(double x);
double pow(double base, double exp);

#endif /* __DG_MATH_H */
