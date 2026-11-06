#ifndef _SYS_CDEFS_H
#define _SYS_CDEFS_H

/* Kinetic: a small <sys/cdefs.h> for software that expects one to exist.
 * Only widely used, non-glibc-internal helpers are provided. */

#include <features.h>

#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

#ifndef __P
#define __P(args) args
#endif

#define __CONCAT(x,y) x ## y
#define __STRING(x) #x

#ifdef __GNUC__
#define __GNUC_PREREQ(maj, min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
#define __GNUC_PREREQ(maj, min) 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#define __THROW __attribute__((__nothrow__))
#define __NTH(fn) fn __attribute__((__nothrow__))
#define __attribute_pure__ __attribute__((__pure__))
#define __attribute_const__ __attribute__((__const__))
#define __attribute_malloc__ __attribute__((__malloc__))
#define __attribute_unused__ __attribute__((__unused__))
#define __attribute_used__ __attribute__((__used__))
#define __attribute_noinline__ __attribute__((__noinline__))
#define __attribute_deprecated__ __attribute__((__deprecated__))
#define __attribute_warn_unused_result__ __attribute__((__warn_unused_result__))
#define __nonnull(params) __attribute__((__nonnull__ params))
#define __wur __attribute__((__warn_unused_result__))
#define __glibc_likely(c) __builtin_expect(!!(c), 1)
#define __glibc_unlikely(c) __builtin_expect(!!(c), 0)
#define __predict_true(c) __builtin_expect(!!(c), 1)
#define __predict_false(c) __builtin_expect(!!(c), 0)
#else
#define __THROW
#define __NTH(fn) fn
#define __attribute_pure__
#define __attribute_const__
#define __attribute_malloc__
#define __attribute_unused__
#define __attribute_used__
#define __attribute_noinline__
#define __attribute_deprecated__
#define __attribute_warn_unused_result__
#define __nonnull(params)
#define __wur
#define __glibc_likely(c) (c)
#define __glibc_unlikely(c) (c)
#define __predict_true(c) (c)
#define __predict_false(c) (c)
#endif

#ifndef __extension__
#ifndef __GNUC__
#define __extension__
#endif
#endif

#endif
