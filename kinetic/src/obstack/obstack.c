#define _GNU_SOURCE
#include <obstack.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#undef obstack_free

#define DEFAULT_ALIGNMENT (sizeof(long double) > sizeof(void *) \
	? sizeof(long double) : sizeof(void *))
#define DEFAULT_CHUNK 4064

static void print_and_exit(void)
{
	static const char msg[] = "memory exhausted\n";
	write(2, msg, sizeof msg - 1);
	exit(obstack_exit_failure);
}

void (*obstack_alloc_failed_handler)(void) = print_and_exit;
int obstack_exit_failure = EXIT_FAILURE;

static void *chunk_alloc(struct obstack *h, size_t n)
{
	return h->use_extra_arg ? h->chunkfun_extra(h->extra_arg, n) : h->chunkfun(n);
}

static void chunk_free(struct obstack *h, void *p)
{
	if (h->use_extra_arg) h->freefun_extra(h->extra_arg, p);
	else h->freefun(p);
}

static char *align_up(struct obstack *h, char *p)
{
	return (char *)(((uintptr_t)p + h->alignment_mask) & ~(uintptr_t)h->alignment_mask);
}

static void fail(struct obstack *h)
{
	h->alloc_failed = 1;
	obstack_alloc_failed_handler();
	/* The handler is required not to return. */
	abort();
}

static int begin(struct obstack *h, size_t size, size_t align)
{
	struct _obstack_chunk *chunk;

	if (!align) align = DEFAULT_ALIGNMENT;
	if (!size) size = DEFAULT_CHUNK;
	if (size < sizeof(struct _obstack_chunk)) size = sizeof(struct _obstack_chunk);
	h->chunk_size = size;
	h->alignment_mask = align - 1;
	h->maybe_empty_object = 0;
	h->alloc_failed = 0;

	chunk = h->chunk = chunk_alloc(h, size);
	if (!chunk) fail(h);
	h->next_free = h->object_base = align_up(h, chunk->contents);
	h->chunk_limit = chunk->limit = (char *)chunk + h->chunk_size;
	chunk->prev = 0;
	return 1;
}

int _obstack_begin(struct obstack *h, size_t size, size_t align,
	void *(*alloc)(size_t), void (*freefn)(void *))
{
	h->chunkfun = alloc;
	h->freefun = freefn;
	h->use_extra_arg = 0;
	return begin(h, size, align);
}

int _obstack_begin_1(struct obstack *h, size_t size, size_t align,
	void *(*alloc)(void *, size_t), void (*freefn)(void *, void *), void *arg)
{
	h->chunkfun_extra = alloc;
	h->freefun_extra = freefn;
	h->extra_arg = arg;
	h->use_extra_arg = 1;
	return begin(h, size, align);
}

void _obstack_newchunk(struct obstack *h, size_t length)
{
	struct _obstack_chunk *old = h->chunk, *new;
	size_t obj_size = h->next_free - h->object_base;
	size_t new_size;
	char *new_base;

	if (length > SIZE_MAX / 2 - obj_size - h->alignment_mask - 128) fail(h);
	new_size = obj_size + length + obj_size / 8 + h->alignment_mask + 100;
	if (new_size < (size_t)h->chunk_size) new_size = h->chunk_size;

	new = chunk_alloc(h, new_size);
	if (!new) fail(h);
	h->chunk = new;
	new->prev = old;
	h->chunk_limit = new->limit = (char *)new + new_size;

	new_base = align_up(h, new->contents);
	memcpy(new_base, h->object_base, obj_size);

	/* The old chunk held nothing but the growing object: release it. */
	if (!h->maybe_empty_object && h->object_base == align_up(h, old->contents)) {
		new->prev = old->prev;
		chunk_free(h, old);
	}

	h->object_base = new_base;
	h->next_free = new_base + obj_size;
	h->maybe_empty_object = 0;
}

int _obstack_allocated_p(struct obstack *h, void *obj)
{
	struct _obstack_chunk *lp = h->chunk;
	while (lp && ((void *)lp >= obj || (void *)lp->limit < obj)) lp = lp->prev;
	return lp != 0;
}

void _obstack_free(struct obstack *h, void *obj)
{
	struct _obstack_chunk *lp = h->chunk;

	while (lp && ((void *)lp >= obj || (void *)lp->limit < obj)) {
		struct _obstack_chunk *prev = lp->prev;
		chunk_free(h, lp);
		lp = prev;
		h->maybe_empty_object = 1;
	}
	if (lp) {
		h->object_base = h->next_free = obj;
		h->chunk_limit = lp->limit;
		h->chunk = lp;
	} else if (obj) {
		abort(); /* obj was not allocated from this obstack */
	} else {
		h->chunk = 0;
		h->object_base = h->next_free = h->chunk_limit = 0;
	}
}

void obstack_free(struct obstack *h, void *obj)
{
	_obstack_free(h, obj);
}

size_t _obstack_memory_used(struct obstack *h)
{
	size_t n = 0;
	struct _obstack_chunk *lp;
	for (lp = h->chunk; lp; lp = lp->prev) n += lp->limit - (char *)lp;
	return n;
}

int obstack_vprintf(struct obstack *h, const char *fmt, va_list ap)
{
	va_list ap2;
	int n;

	va_copy(ap2, ap);
	n = vsnprintf(0, 0, fmt, ap2);
	va_end(ap2);
	if (n < 0) return n;

	obstack_make_room(h, (size_t)n + 1);
	n = vsnprintf(h->next_free, (size_t)n + 1, fmt, ap);
	if (n > 0) h->next_free += n;
	return n;
}

int obstack_printf(struct obstack *h, const char *fmt, ...)
{
	va_list ap;
	int n;
	va_start(ap, fmt);
	n = obstack_vprintf(h, fmt, ap);
	va_end(ap);
	return n;
}
