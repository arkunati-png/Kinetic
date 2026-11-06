#ifndef _GNU_LIBC_VERSION_H
#define _GNU_LIBC_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

/* Kinetic is not glibc: these return Kinetic's own version strings
 * ("kinetic-X.Y.Z" and "kinetic"), never a fabricated glibc version. */
const char *gnu_get_libc_version(void);
const char *gnu_get_libc_release(void);

#ifdef __cplusplus
}
#endif

#endif
