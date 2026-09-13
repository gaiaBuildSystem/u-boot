/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Minimal <stdlib.h> replacement for the Doom engine running inside
 * U-Boot. Standard names are mapped onto small wrappers (implemented in
 * compat.c) so that they resolve correctly regardless of which U-Boot
 * allocator is in use.
 */

#ifndef __DG_STDLIB_H
#define __DG_STDLIB_H

#include <stddef.h>
#include <dg_stdint.h>

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
#define RAND_MAX	0x7fffffff

void *dg_malloc(size_t size);
void *dg_calloc(size_t nmemb, size_t size);
void *dg_realloc(void *ptr, size_t size);
void dg_free(void *ptr);

void dg_exit(int status) __attribute__((noreturn));
void dg_abort(void) __attribute__((noreturn));
int dg_atexit(void (*func)(void));

int dg_atoi(const char *str);
long dg_strtol(const char *str, char **endptr, int base);
unsigned long dg_strtoul(const char *str, char **endptr, int base);
double dg_atof(const char *str);

char *dg_getenv(const char *name);
int dg_system(const char *command);

int dg_abs(int n);
long dg_labs(long n);

int rand(void);
void srand(unsigned int seed);

/* qsort() is provided directly by U-Boot (lib/qsort.c) */
void qsort(void *base, size_t nmemb, size_t size,
	   int (*compar)(const void *, const void *));

#define malloc(s)	dg_malloc(s)
#define calloc(n, s)	dg_calloc(n, s)
#define realloc(p, s)	dg_realloc(p, s)
#define free(p)		dg_free(p)
#define exit(s)		dg_exit(s)
#define abort()		dg_abort()
#define atexit(f)	dg_atexit(f)
#define atoi(s)		dg_atoi(s)
#define strtol(s, e, b)	dg_strtol(s, e, b)
#define strtoul(s, e, b) dg_strtoul(s, e, b)
#define atof(s)		dg_atof(s)
#define getenv(n)	dg_getenv(n)
#define system(c)	dg_system(c)
#define abs		dg_abs
#define labs		dg_labs

#endif /* __DG_STDLIB_H */
