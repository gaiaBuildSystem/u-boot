// SPDX-License-Identifier: GPL-2.0+
/*
 * C-library compatibility shim for running the Doom engine inside U-Boot.
 *
 * The engine is built against the dg_*.h replacement headers; this file
 * provides the handful of libc facilities that U-Boot does not, layered
 * on top of U-Boot services and the backing store in dg_store.h.
 */

#include <linux/types.h>
#include <linux/string.h>
#include <vsprintf.h>
#include <malloc.h>
#include <setjmp.h>

#define DG_COMPAT_IMPL
#include "dg_stdio.h"
#include "dg_store.h"

/* U-Boot console primitives (avoid including U-Boot's <stdio.h>, which
 * collides with the FILE-based dg_stdio.h). */
extern int printf(const char *fmt, ...);
extern void putc(const char c);

/* exit()/longjmp() machinery, set up by the doom command (cmd_doom.c) */
extern jmp_buf doom_exit_env;
extern int doom_exit_active;

/* ---- memory ---------------------------------------------------------- */

void *dg_malloc(size_t size)
{
	return malloc(size);
}

void *dg_calloc(size_t nmemb, size_t size)
{
	return calloc(nmemb, size);
}

void *dg_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

void dg_free(void *ptr)
{
	free(ptr);
}

/* ---- process control ------------------------------------------------- */

#define MAX_ATEXIT	32
static void (*atexit_funcs[MAX_ATEXIT])(void);
static int atexit_count;

int dg_atexit(void (*func)(void))
{
	if (atexit_count >= MAX_ATEXIT)
		return -1;
	atexit_funcs[atexit_count++] = func;
	return 0;
}

void dg_exit(int status)
{
	int i;

	for (i = atexit_count - 1; i >= 0; i--)
		atexit_funcs[i]();

	if (doom_exit_active)
		longjmp(doom_exit_env, status ? status : 1);

	/* No command context to return to: spin so we do not run off. */
	printf("doom: exit(%d) with no jump target\n", status);
	for (;;)
		;
}

void dg_abort(void)
{
	dg_exit(134);
}

char *dg_getenv(const char *name)
{
	(void)name;
	return NULL;
}

int dg_system(const char *command)
{
	(void)command;
	return -1;
}

/* ---- integer/float conversion ---------------------------------------- */

int dg_abs(int n)
{
	return n < 0 ? -n : n;
}

long dg_labs(long n)
{
	return n < 0 ? -n : n;
}

int dg_atoi(const char *str)
{
	return (int)simple_strtol(str, NULL, 10);
}

long dg_strtol(const char *str, char **endptr, int base)
{
	return simple_strtol(str, endptr, base);
}

unsigned long dg_strtoul(const char *str, char **endptr, int base)
{
	return simple_strtoul(str, endptr, base);
}

double dg_atof(const char *str)
{
	double val = 0.0, frac = 0.0, scale = 1.0;
	int sign = 1;

	while (*str == ' ' || *str == '\t')
		str++;
	if (*str == '-') {
		sign = -1;
		str++;
	} else if (*str == '+') {
		str++;
	}
	while (*str >= '0' && *str <= '9')
		val = val * 10.0 + (*str++ - '0');
	if (*str == '.') {
		str++;
		while (*str >= '0' && *str <= '9') {
			frac = frac * 10.0 + (*str++ - '0');
			scale *= 10.0;
		}
	}
	return sign * (val + frac / scale);
}

/* ---- ctype ----------------------------------------------------------- */

int dg_toupper(int c)
{
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 'A';
	return c;
}

int dg_tolower(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	return c;
}

/* ---- string ---------------------------------------------------------- */

char *dg_strdup(const char *s)
{
	size_t len = strlen(s) + 1;
	char *p = malloc(len);

	if (p)
		memcpy(p, s, len);
	return p;
}

/* ---- minimal sscanf (integers only, which is all Doom needs) --------- */

static const char *skip_ws(const char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\n')
		s++;
	return s;
}

static int digit_val(int c, int base)
{
	int v;

	if (c >= '0' && c <= '9')
		v = c - '0';
	else if (c >= 'a' && c <= 'z')
		v = c - 'a' + 10;
	else if (c >= 'A' && c <= 'Z')
		v = c - 'A' + 10;
	else
		return -1;
	return v < base ? v : -1;
}

static const char *parse_int(const char *s, int base, int is_signed,
			     unsigned long *out)
{
	unsigned long val = 0;
	int neg = 0, any = 0, d;

	s = skip_ws(s);
	if (is_signed && (*s == '-' || *s == '+')) {
		neg = (*s == '-');
		s++;
	}
	if ((base == 16 || base == 0) && s[0] == '0' &&
	    (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
		base = 16;
	} else if (base == 0 && s[0] == '0') {
		base = 8;
	} else if (base == 0) {
		base = 10;
	}
	while ((d = digit_val(*s, base)) >= 0) {
		val = val * base + d;
		any = 1;
		s++;
	}
	if (!any)
		return NULL;
	*out = neg ? (unsigned long)(-(long)val) : val;
	return s;
}

int sscanf(const char *str, const char *fmt, ...)
{
	va_list ap;
	int count = 0;

	va_start(ap, fmt);
	while (*fmt) {
		if (*fmt == '%') {
			int base = 10, is_signed = 1;
			unsigned long val;
			const char *next;

			fmt++;
			switch (*fmt) {
			case 'd':
				base = 10;
				break;
			case 'u':
				base = 10;
				is_signed = 0;
				break;
			case 'i':
				base = 0;
				break;
			case 'o':
				base = 8;
				is_signed = 0;
				break;
			case 'x':
			case 'X':
				base = 16;
				is_signed = 0;
				break;
			default:
				goto done;
			}
			next = parse_int(str, base, is_signed, &val);
			if (!next)
				goto done;
			*va_arg(ap, int *) = (int)val;
			str = next;
			count++;
			fmt++;
		} else if (*fmt == ' ' || *fmt == '\t' || *fmt == '\n') {
			str = skip_ws(str);
			fmt++;
		} else {
			if (*str != *fmt)
				goto done;
			str++;
			fmt++;
		}
	}
done:
	va_end(ap);
	return count;
}

/* ---- FILE I/O over the backing store --------------------------------- */

static FILE std_in = { 0, 0, 0 };
static FILE std_out = { 1, 0, 0 };
static FILE std_err = { 2, 0, 0 };
FILE *stdin = &std_in;
FILE *stdout = &std_out;
FILE *stderr = &std_err;

static int is_console(FILE *s)
{
	return s == stdout || s == stderr;
}

FILE *fopen(const char *path, const char *mode)
{
	int flags = DGO_RDONLY;
	int append = 0;
	FILE *f;
	int fd;

	if (strchr(mode, 'w'))
		flags = DGO_WRONLY | DGO_CREAT | DGO_TRUNC;
	else if (strchr(mode, 'a')) {
		flags = DGO_RDWR | DGO_CREAT;
		append = 1;
	} else
		flags = DGO_RDONLY;
	if (strchr(mode, '+'))
		flags = (flags & ~DGO_MASK) | DGO_RDWR;

	fd = dg_open(path, flags);
	if (fd < 0)
		return NULL;

	f = malloc(sizeof(*f));
	if (!f) {
		dg_close(fd);
		return NULL;
	}
	f->fd = fd;
	f->eof = 0;
	f->err = 0;
	if (append)
		dg_lseek(fd, 0, DGS_END);
	return f;
}

int fclose(FILE *stream)
{
	int ret;

	if (!stream || is_console(stream))
		return 0;
	ret = dg_close(stream->fd);
	free(stream);
	return ret < 0 ? EOF : 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	ssize_t ret;
	size_t total = size * nmemb;

	if (!stream || !total)
		return 0;
	ret = dg_read(stream->fd, ptr, total);
	if (ret <= 0) {
		stream->eof = 1;
		return 0;
	}
	return (size_t)ret / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t total = size * nmemb;

	if (!stream || !total)
		return 0;
	if (is_console(stream)) {
		const char *p = ptr;
		size_t i;

		for (i = 0; i < total; i++)
			putc(p[i]);
		return nmemb;
	}
	if (dg_write(stream->fd, ptr, total) < 0)
		return 0;
	return nmemb;
}

int fseek(FILE *stream, long offset, int whence)
{
	if (!stream || is_console(stream))
		return -1;
	return dg_lseek(stream->fd, offset, whence) < 0 ? -1 : 0;
}

long ftell(FILE *stream)
{
	if (!stream || is_console(stream))
		return -1;
	return (long)dg_lseek(stream->fd, 0, DGS_CUR);
}

int dg_fflush(FILE *stream)
{
	(void)stream;
	return 0;
}

int feof(FILE *stream)
{
	return stream ? stream->eof : 1;
}

int ferror(FILE *stream)
{
	return stream ? stream->err : 1;
}

char *fgets(char *s, int size, FILE *stream)
{
	int i = 0;
	char c;

	if (!stream || size <= 0)
		return NULL;
	while (i < size - 1) {
		if (dg_read(stream->fd, &c, 1) != 1) {
			stream->eof = 1;
			break;
		}
		s[i++] = c;
		if (c == '\n')
			break;
	}
	if (i == 0)
		return NULL;
	s[i] = '\0';
	return s;
}

int dg_fputc(int c, FILE *stream)
{
	char ch = c;

	if (is_console(stream)) {
		putc(ch);
		return c;
	}
	return dg_write(stream->fd, &ch, 1) == 1 ? c : EOF;
}

int dg_fgetc(FILE *stream)
{
	char c;

	if (!stream || dg_read(stream->fd, &c, 1) != 1) {
		if (stream)
			stream->eof = 1;
		return EOF;
	}
	return (unsigned char)c;
}

int dg_fputs(const char *s, FILE *stream)
{
	size_t len = strlen(s);

	return fwrite(s, 1, len, stream) == len ? 0 : EOF;
}

int putchar(int c)
{
	putc((char)c);
	return c;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap)
{
	char buf[1024];
	int len;

	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	if (len < 0)
		return len;
	fwrite(buf, 1, len < (int)sizeof(buf) ? len : (int)sizeof(buf) - 1,
	       stream);
	return len;
}

int dg_fprintf(FILE *stream, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vfprintf(stream, fmt, ap);
	va_end(ap);
	return ret;
}

int dg_remove(const char *path)
{
	return dg_unlink(path);
}

int rename(const char *oldpath, const char *newpath)
{
	char buf[4096];
	int in, out;
	ssize_t n;

	in = dg_open(oldpath, DGO_RDONLY);
	if (in < 0)
		return -1;
	out = dg_open(newpath, DGO_WRONLY | DGO_CREAT | DGO_TRUNC);
	if (out < 0) {
		dg_close(in);
		return -1;
	}
	while ((n = dg_read(in, buf, sizeof(buf))) > 0) {
		if (dg_write(out, buf, n) != n) {
			dg_close(in);
			dg_close(out);
			return -1;
		}
	}
	dg_close(in);
	dg_close(out);
	dg_unlink(oldpath);
	return 0;
}

void setbuf(FILE *stream, char *buf)
{
	(void)stream;
	(void)buf;
}

int setvbuf(FILE *stream, char *buf, int mode, size_t size)
{
	(void)stream;
	(void)buf;
	(void)mode;
	(void)size;
	return 0;
}

int fileno(FILE *stream)
{
	return stream ? stream->fd : -1;
}
