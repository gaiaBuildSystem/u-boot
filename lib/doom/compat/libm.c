// SPDX-License-Identifier: GPL-2.0+
/*
 * Tiny double-precision math library for the Doom engine. Doom is
 * fixed-point throughout; these routines are only called a handful of
 * times at start-up to build the projection/viewangle tables, so simple
 * range-reduced series are more than accurate enough.
 */

#include "dg_math.h"

double fabs(double x)
{
	return x < 0 ? -x : x;
}

double floor(double x)
{
	long i = (long)x;

	if (x < 0 && (double)i != x)
		i--;
	return (double)i;
}

double ceil(double x)
{
	long i = (long)x;

	if (x > 0 && (double)i != x)
		i++;
	return (double)i;
}

double sqrt(double x)
{
	double r;
	int i;

	if (x <= 0)
		return 0;
	r = x;
	for (i = 0; i < 40; i++)
		r = 0.5 * (r + x / r);
	return r;
}

/* sin() via range reduction to [-pi, pi] plus an 11th-order Taylor series */
double sin(double x)
{
	double term, sum, x2;
	int i;

	/* reduce modulo 2*pi */
	x -= 2.0 * M_PI * floor(x / (2.0 * M_PI) + 0.5);

	x2 = x * x;
	term = x;
	sum = x;
	for (i = 1; i <= 7; i++) {
		term *= -x2 / ((2 * i) * (2 * i + 1));
		sum += term;
	}
	return sum;
}

double cos(double x)
{
	return sin(x + M_PI / 2.0);
}

double tan(double x)
{
	double c = cos(x);

	if (c == 0.0)
		c = 1e-12;
	return sin(x) / c;
}

/* atan() for the full range, reducing to |x| <= 1 then a polynomial */
double atan(double x)
{
	double x2, sum, term;
	int neg = 0, inv = 0;
	int i;

	if (x < 0) {
		x = -x;
		neg = 1;
	}
	if (x > 1.0) {
		x = 1.0 / x;
		inv = 1;
	}

	x2 = x * x;
	term = x;
	sum = x;
	for (i = 1; i <= 12; i++) {
		term *= -x2;
		sum += term / (2 * i + 1);
	}

	if (inv)
		sum = M_PI / 2.0 - sum;
	return neg ? -sum : sum;
}

double atan2(double y, double x)
{
	if (x > 0)
		return atan(y / x);
	if (x < 0)
		return y >= 0 ? atan(y / x) + M_PI : atan(y / x) - M_PI;
	/* x == 0 */
	if (y > 0)
		return M_PI / 2.0;
	if (y < 0)
		return -M_PI / 2.0;
	return 0.0;
}

double pow(double base, double exp)
{
	/* Doom only ever needs integer powers here. */
	long n = (long)exp;
	double r = 1.0;
	int neg = 0;

	if (n < 0) {
		neg = 1;
		n = -n;
	}
	while (n--)
		r *= base;
	return neg ? 1.0 / r : r;
}
