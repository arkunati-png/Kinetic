#define _GNU_SOURCE
#include <error.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void (*error_print_progname)(void);
unsigned int error_message_count;
int error_one_per_line;

static void report(int status, int errnum, int has_loc, const char *file,
	unsigned int line, const char *fmt, va_list ap)
{
	static const char *last_file;
	static unsigned int last_line;

	flockfile(stderr);

	if (has_loc && error_one_per_line) {
		int same = last_line == line && file && last_file
			&& (file == last_file || !strcmp(file, last_file));
		if (same) {
			funlockfile(stderr);
			return;
		}
		last_file = file;
		last_line = line;
	}

	fflush(stdout);
	if (error_print_progname) {
		error_print_progname();
	} else {
		fprintf(stderr, has_loc ? "%s:" : "%s: ", program_invocation_name);
	}
	if (has_loc) {
		if (file) fprintf(stderr, "%s:%u: ", file, line);
		else fputc(' ', stderr);
	}

	vfprintf(stderr, fmt, ap);
	error_message_count++;
	if (errnum) fprintf(stderr, ": %s", strerror(errnum));
	fputc('\n', stderr);
	fflush(stderr);

	funlockfile(stderr);
	if (status) exit(status);
}

void error(int status, int errnum, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	report(status, errnum, 0, 0, 0, fmt, ap);
	va_end(ap);
}

void error_at_line(int status, int errnum, const char *file,
	unsigned int line, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	report(status, errnum, 1, file, line, fmt, ap);
	va_end(ap);
}
