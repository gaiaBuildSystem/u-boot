/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * <string.h>/<strings.h> replacement for the Doom engine. The functions
 * are declared directly (resolving to U-Boot's string library at link
 * time) rather than via <linux/string.h>, which would drag in
 * <linux/types.h> and collide with Doom's own types (e.g. sector_t).
 */

#ifndef __DG_STRING_H
#define __DG_STRING_H

#include <stddef.h>
#include <dg_stdint.h>

void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);

char *dg_strdup(const char *s);
#define strdup(s)	dg_strdup(s)

#endif /* __DG_STRING_H */
