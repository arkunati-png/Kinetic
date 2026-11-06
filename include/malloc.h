#ifndef _MALLOC_H
#define _MALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t

#include <bits/alltypes.h>

void *malloc (size_t);
void *calloc (size_t, size_t);
void *realloc (void *, size_t);
void free (void *);
void *valloc (size_t);
void *memalign(size_t, size_t);

size_t malloc_usable_size(void *);

/* Kinetic additions. With the default allocator mallinfo()/mallinfo2() are
 * computed from allocator metadata on demand; mallopt() is accepted and
 * ignored and malloc_trim() is a no-op. See COMPAT.md. */
struct mallinfo {
	int arena, ordblks, smblks, hblks, hblkd;
	int usmblks, fsmblks, uordblks, fordblks, keepcost;
};

struct mallinfo2 {
	size_t arena, ordblks, smblks, hblks, hblkd;
	size_t usmblks, fsmblks, uordblks, fordblks, keepcost;
};

struct mallinfo mallinfo(void);
struct mallinfo2 mallinfo2(void);
int mallopt(int, int);
int malloc_trim(size_t);
void malloc_stats(void);

#define M_MXFAST 1
#define M_TRIM_THRESHOLD (-1)
#define M_TOP_PAD (-2)
#define M_MMAP_THRESHOLD (-3)
#define M_MMAP_MAX (-4)
#define M_CHECK_ACTION (-5)
#define M_PERTURB (-6)
#define M_ARENA_TEST (-7)
#define M_ARENA_MAX (-8)

#ifdef __cplusplus
}
#endif

#endif
