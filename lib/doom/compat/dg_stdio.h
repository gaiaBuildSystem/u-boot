/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Minimal <stdio.h> replacement for the Doom engine running inside
 * U-Boot. A small FILE implementation is layered on top of the sandbox
 * os_*() host syscalls (see compat.c) so that WAD loading, save games
 * and the config file work against real files on the host.
 *
 * The printf() family is provided directly by U-Boot; compat.c defines
 * DG_COMPAT_IMPL before including this header so that it picks up the
 * real U-Boot prototypes instead of the ones declared here.
 */

#ifndef __DG_STDIO_H
#define __DG_STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include <dg_stdint.h>

#ifndef EOF
#define EOF	(-1)
#endif

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

#ifndef BUFSIZ
#define BUFSIZ		1024
#endif

typedef struct _dg_file {
	int fd;
	int eof;
	int err;
} FILE;

/*
 * Everything here is given a dg_ name and reached through a macro. Two
 * reasons: U-Boot's console already exports fprintf(), fputs(), fputc(),
 * fgetc() and fflush() as 'int file' based functions; and on sandbox the
 * whole of U-Boot is linked against the host C library, so a symbol
 * called fopen() or stdout() here would override the host's for the
 * entire process, which breaks anything else in it that does file I/O.
 */
extern FILE *dg_stdin;
extern FILE *dg_stdout;
extern FILE *dg_stderr;

#define stdin	dg_stdin
#define stdout	dg_stdout
#define stderr	dg_stderr

FILE *dg_fopen(const char *path, const char *mode);
int dg_fclose(FILE *stream);
size_t dg_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t dg_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int dg_fseek(FILE *stream, long offset, int whence);
long dg_ftell(FILE *stream);
int dg_feof(FILE *stream);
int dg_ferror(FILE *stream);
char *dg_fgets(char *s, int size, FILE *stream);
int dg_vfprintf(FILE *stream, const char *fmt, va_list ap);

#define fopen		dg_fopen
#define fclose		dg_fclose
#define fread		dg_fread
#define fwrite		dg_fwrite
#define fseek		dg_fseek
#define ftell		dg_ftell
#define feof		dg_feof
#define ferror		dg_ferror
#define fgets		dg_fgets
#define vfprintf	dg_vfprintf

int dg_fflush(FILE *stream);
int dg_fputs(const char *s, FILE *stream);
int dg_fputc(int c, FILE *stream);
int dg_fgetc(FILE *stream);
int dg_fprintf(FILE *stream, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

#define fflush(s)	dg_fflush(s)
#define fputs(s, f)	dg_fputs(s, f)
#define fputc(c, f)	dg_fputc(c, f)
#define fgetc(f)	dg_fgetc(f)
#define fprintf		dg_fprintf
int dg_putchar(int c);
int dg_remove(const char *path);
int dg_rename(const char *oldpath, const char *newpath);
void dg_setbuf(FILE *stream, char *buf);
int dg_setvbuf(FILE *stream, char *buf, int mode, size_t size);
int dg_fileno(FILE *stream);

#define putchar		dg_putchar
#define remove		dg_remove
#define rename		dg_rename
#define setbuf		dg_setbuf
#define setvbuf		dg_setvbuf
#define fileno		dg_fileno

int dg_sscanf(const char *str, const char *fmt, ...)
	__attribute__((format(scanf, 2, 3)));

#define sscanf		dg_sscanf

#ifndef DG_COMPAT_IMPL
/* These are real U-Boot symbols; declare with standard signatures. */
int printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int sprintf(char *buf, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));
int snprintf(char *buf, size_t size, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int vprintf(const char *fmt, va_list ap);
int puts(const char *s);
#endif /* !DG_COMPAT_IMPL */

#endif /* __DG_STDIO_H */
