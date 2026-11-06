#define _GNU_SOURCE
#include <limits.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Allocator statistics.
 *
 * With the default allocator (mallocng) the numbers are real: they are
 * computed on demand by walking the allocator's group metadata under the
 * malloc lock, so the malloc/free fast paths pay nothing for them. They
 * are a snapshot and, like glibc's, approximate under concurrency. With
 * any other allocator selected at configure time the counters read zero.
 *
 * Field meaning (same as glibc's struct mallinfo2):
 *   arena    bytes obtained from the system for small/medium allocations
 *   uordblks bytes handed out to the program from the arena
 *   fordblks bytes in the arena that are free
 *   ordblks  number of free slots in the arena
 *   hblks    number of individually mmap()ed large allocations
 *   hblkd    bytes in those mappings
 * The remaining fields are unused by this allocator and report 0.
 */

#ifdef KINETIC_MALLOCNG

#include "../../../src/malloc/mallocng/meta.h"

struct mallinfo2 mallinfo2(void)
{
	struct mallinfo2 mi;
	size_t arena = 0, used = 0, nested = 0, nfree = 0;
	struct meta_area *a;
	int i;

	memset(&mi, 0, sizeof mi);

	rdlock();
	for (a = ctx.meta_area_head; a; a = a->next) {
		for (i = 0; i < a->nslots; i++) {
			struct meta *m = &a->slots[i];
			uint32_t valid, freemask;
			unsigned cnt, nf;
			size_t stride;

			if (!m->mem) continue; /* never used, or recycled */
			cnt = m->last_idx + 1;
			valid = (2u << m->last_idx) - 1;
			freemask = (m->avail_mask | m->freed_mask) & valid;
			nf = __builtin_popcount(freemask);
			stride = get_stride(m);

			if (m->sizeclass == 63) {
				/* one dedicated mapping per allocation */
				mi.hblks++;
				mi.hblkd += (size_t)m->maplen * 4096;
				continue;
			}
			if (m->maplen) {
				arena += (size_t)m->maplen * 4096;
			} else if (!m->freeable) {
				/* memory donated by the dynamic linker (page slack
				 * between library segments): it is arena, but was not
				 * obtained from the allocator's own mappings */
				arena += UNIT + stride * cnt;
			} else {
				/* group carved out of a slot of a larger group:
				 * that parent slot is not user data */
				nested += UNIT + stride * cnt;
			}
			used += (size_t)(cnt - nf) * stride;
			nfree += nf;
		}
	}
	unlock();

	mi.arena = arena;
	mi.uordblks = used > nested ? used - nested : 0;
	mi.fordblks = arena > mi.uordblks ? arena - mi.uordblks : 0;
	mi.ordblks = nfree;
	return mi;
}

#else

struct mallinfo2 mallinfo2(void)
{
	struct mallinfo2 mi;
	memset(&mi, 0, sizeof mi);
	return mi;
}

#endif

static int clamp(size_t v)
{
	return v > INT_MAX ? INT_MAX : (int)v;
}

struct mallinfo mallinfo(void)
{
	struct mallinfo2 m2 = mallinfo2();
	struct mallinfo mi;
	memset(&mi, 0, sizeof mi);
	mi.arena = clamp(m2.arena);
	mi.ordblks = clamp(m2.ordblks);
	mi.hblks = clamp(m2.hblks);
	mi.hblkd = clamp(m2.hblkd);
	mi.uordblks = clamp(m2.uordblks);
	mi.fordblks = clamp(m2.fordblks);
	return mi;
}

void malloc_stats(void)
{
	struct mallinfo2 mi = mallinfo2();
	fprintf(stderr, "Arena 0:\n");
	fprintf(stderr, "system bytes     = %10zu\n", mi.arena);
	fprintf(stderr, "in use bytes     = %10zu\n", mi.uordblks);
	fprintf(stderr, "Total (incl. mmap):\n");
	fprintf(stderr, "system bytes     = %10zu\n", mi.arena + mi.hblkd);
	fprintf(stderr, "in use bytes     = %10zu\n", mi.uordblks + mi.hblkd);
	fprintf(stderr, "max mmap regions = %10zu\n", mi.hblks);
}
