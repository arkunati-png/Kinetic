#ifndef _OBSTACK_H
#define _OBSTACK_H

/* Kinetic <obstack.h>: object stacks, an arena-style allocator used by
 * GNU software (gcc, binutils, m4, bison, make, ...). Original MIT
 * implementation of the documented interface; the header's accessors are
 * static inlines so every argument is evaluated exactly once. */

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>

struct _obstack_chunk {
	char *limit;
	struct _obstack_chunk *prev;
	char contents[4];
};

struct obstack {
	long chunk_size;
	struct _obstack_chunk *chunk;
	char *object_base;
	char *next_free;
	char *chunk_limit;
	union {
		ptrdiff_t tempint;
		void *tempptr;
	} temp;
	int alignment_mask;
	void *(*chunkfun)(size_t);
	void (*freefun)(void *);
	void *(*chunkfun_extra)(void *, size_t);
	void (*freefun_extra)(void *, void *);
	void *extra_arg;
	unsigned use_extra_arg : 1;
	unsigned maybe_empty_object : 1;
	unsigned alloc_failed : 1;
};

extern void (*obstack_alloc_failed_handler)(void);
extern int obstack_exit_failure;

int _obstack_begin(struct obstack *, size_t, size_t,
	void *(*)(size_t), void (*)(void *));
int _obstack_begin_1(struct obstack *, size_t, size_t,
	void *(*)(void *, size_t), void (*)(void *, void *), void *);
void _obstack_newchunk(struct obstack *, size_t);
void _obstack_free(struct obstack *, void *);
void obstack_free(struct obstack *, void *);
int _obstack_allocated_p(struct obstack *, void *);
size_t _obstack_memory_used(struct obstack *);
int obstack_printf(struct obstack *, const char *, ...);
int obstack_vprintf(struct obstack *, const char *, va_list);

/* Users define obstack_chunk_alloc / obstack_chunk_free before calling
 * obstack_init() (or obstack_begin()). */
#define obstack_init(h) \
	_obstack_begin((h), 0, 0, \
		(void *(*)(size_t))obstack_chunk_alloc, (void (*)(void *))obstack_chunk_free)
#define obstack_begin(h, size) \
	_obstack_begin((h), (size), 0, \
		(void *(*)(size_t))obstack_chunk_alloc, (void (*)(void *))obstack_chunk_free)
#define obstack_specify_allocation(h, size, align, alloc, freefn) \
	_obstack_begin((h), (size), (align), (alloc), (freefn))
#define obstack_specify_allocation_with_arg(h, size, align, alloc, freefn, arg) \
	_obstack_begin_1((h), (size), (align), (alloc), (freefn), (arg))

#define obstack_base(h) ((void *)(h)->object_base)
#define obstack_next_free(h) ((void *)(h)->next_free)
#define obstack_object_size(h) ((size_t)((h)->next_free - (h)->object_base))
#define obstack_room(h) ((size_t)((h)->chunk_limit - (h)->next_free))
#define obstack_chunk_size(h) ((h)->chunk_size)
#define obstack_alignment_mask(h) ((h)->alignment_mask)

static inline void __kn_obstack_make_room(struct obstack *h, size_t n)
{
	if (obstack_room(h) < n) _obstack_newchunk(h, n);
}

static inline void *__kn_obstack_finish(struct obstack *h)
{
	void *value = h->object_base;
	if (h->next_free == h->object_base) h->maybe_empty_object = 1;
	h->next_free = (char *)(((ptrdiff_t)h->next_free + h->alignment_mask)
		& ~(ptrdiff_t)h->alignment_mask);
	if (h->next_free > h->chunk_limit) h->next_free = h->chunk_limit;
	h->object_base = h->next_free;
	return value;
}

static inline void __kn_obstack_grow(struct obstack *h, const void *p, size_t n)
{
	__kn_obstack_make_room(h, n);
	memcpy(h->next_free, p, n);
	h->next_free += n;
}

static inline void __kn_obstack_grow0(struct obstack *h, const void *p, size_t n)
{
	__kn_obstack_make_room(h, n + 1);
	memcpy(h->next_free, p, n);
	h->next_free += n;
	*h->next_free++ = 0;
}

static inline void __kn_obstack_blank(struct obstack *h, ptrdiff_t n)
{
	if (n > 0) __kn_obstack_make_room(h, (size_t)n);
	h->next_free += n;
}

static inline void __kn_obstack_1grow(struct obstack *h, int c)
{
	__kn_obstack_make_room(h, 1);
	*h->next_free++ = (char)c;
}

static inline void __kn_obstack_ptr_grow(struct obstack *h, const void *p)
{
	__kn_obstack_grow(h, &p, sizeof p);
}

static inline void __kn_obstack_int_grow(struct obstack *h, int i)
{
	__kn_obstack_grow(h, &i, sizeof i);
}

static inline void *__kn_obstack_alloc(struct obstack *h, size_t n)
{
	__kn_obstack_blank(h, (ptrdiff_t)n);
	return __kn_obstack_finish(h);
}

static inline void *__kn_obstack_copy(struct obstack *h, const void *p, size_t n)
{
	__kn_obstack_grow(h, p, n);
	return __kn_obstack_finish(h);
}

static inline void *__kn_obstack_copy0(struct obstack *h, const void *p, size_t n)
{
	__kn_obstack_grow0(h, p, n);
	return __kn_obstack_finish(h);
}

#define obstack_make_room(h, n) __kn_obstack_make_room((h), (n))
#define obstack_finish(h) __kn_obstack_finish(h)
#define obstack_grow(h, p, n) __kn_obstack_grow((h), (p), (n))
#define obstack_grow0(h, p, n) __kn_obstack_grow0((h), (p), (n))
#define obstack_blank(h, n) __kn_obstack_blank((h), (n))
#define obstack_1grow(h, c) __kn_obstack_1grow((h), (c))
#define obstack_ptr_grow(h, p) __kn_obstack_ptr_grow((h), (p))
#define obstack_int_grow(h, i) __kn_obstack_int_grow((h), (i))
#define obstack_alloc(h, n) __kn_obstack_alloc((h), (n))
#define obstack_copy(h, p, n) __kn_obstack_copy((h), (p), (n))
#define obstack_copy0(h, p, n) __kn_obstack_copy0((h), (p), (n))

/* "Fast" variants assume the caller already checked obstack_room(). */
#define obstack_blank_fast(h, n) ((void)((h)->next_free += (n)))
#define obstack_1grow_fast(h, c) ((void)(*(h)->next_free++ = (char)(c)))
#define obstack_ptr_grow_fast(h, p) \
	((void)(*(void **)(h)->next_free = (void *)(p), (h)->next_free += sizeof(void *)))
#define obstack_int_grow_fast(h, i) \
	((void)(*(int *)(h)->next_free = (i), (h)->next_free += sizeof(int)))

#define obstack_free(h, obj) _obstack_free((h), (obj))

#ifdef __cplusplus
}
#endif

#endif
