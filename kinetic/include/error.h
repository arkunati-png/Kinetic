#ifndef _ERROR_H
#define _ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

void error(int, int, const char *, ...);
void error_at_line(int, int, const char *, unsigned int, const char *, ...);

extern void (*error_print_progname)(void);
extern unsigned int error_message_count;
extern int error_one_per_line;

#ifdef __cplusplus
}
#endif

#endif
