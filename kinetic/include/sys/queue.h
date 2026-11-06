#ifndef _SYS_QUEUE_H
#define _SYS_QUEUE_H

/* Kinetic <sys/queue.h>: intrusive singly-linked lists, lists, simple tail
 * queues, tail queues and circular queues (see queue(3)). Original MIT
 * implementation of the traditional interface. */

#include <stddef.h>

/* ---- singly-linked list ------------------------------------------------ */

#define SLIST_HEAD(name, type) struct name { struct type *slh_first; }
#define SLIST_HEAD_INITIALIZER(head) { NULL }
#define SLIST_ENTRY(type) struct { struct type *sle_next; }

#define SLIST_EMPTY(head) ((head)->slh_first == NULL)
#define SLIST_FIRST(head) ((head)->slh_first)
#define SLIST_NEXT(elm, field) ((elm)->field.sle_next)
#define SLIST_INIT(head) do { (head)->slh_first = NULL; } while (0)

#define SLIST_FOREACH(var, head, field) \
	for ((var) = SLIST_FIRST(head); (var); (var) = SLIST_NEXT(var, field))

#define SLIST_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = SLIST_FIRST(head); \
	     (var) && ((tvar) = SLIST_NEXT(var, field), 1); (var) = (tvar))

#define SLIST_INSERT_HEAD(head, elm, field) do { \
	(elm)->field.sle_next = (head)->slh_first; \
	(head)->slh_first = (elm); \
} while (0)

#define SLIST_INSERT_AFTER(slistelm, elm, field) do { \
	(elm)->field.sle_next = (slistelm)->field.sle_next; \
	(slistelm)->field.sle_next = (elm); \
} while (0)

#define SLIST_REMOVE_HEAD(head, field) do { \
	(head)->slh_first = (head)->slh_first->field.sle_next; \
} while (0)

#define SLIST_REMOVE_AFTER(elm, field) do { \
	(elm)->field.sle_next = (elm)->field.sle_next->field.sle_next; \
} while (0)

#define SLIST_REMOVE(head, elm, type, field) do { \
	if ((head)->slh_first == (elm)) { \
		SLIST_REMOVE_HEAD(head, field); \
	} else { \
		struct type *kn_cur = (head)->slh_first; \
		while (kn_cur->field.sle_next != (elm)) \
			kn_cur = kn_cur->field.sle_next; \
		kn_cur->field.sle_next = (elm)->field.sle_next; \
	} \
} while (0)

/* ---- doubly-linked list ------------------------------------------------ */

#define LIST_HEAD(name, type) struct name { struct type *lh_first; }
#define LIST_HEAD_INITIALIZER(head) { NULL }
#define LIST_ENTRY(type) \
	struct { struct type *le_next; struct type **le_prev; }

#define LIST_EMPTY(head) ((head)->lh_first == NULL)
#define LIST_FIRST(head) ((head)->lh_first)
#define LIST_NEXT(elm, field) ((elm)->field.le_next)
#define LIST_INIT(head) do { (head)->lh_first = NULL; } while (0)

#define LIST_FOREACH(var, head, field) \
	for ((var) = LIST_FIRST(head); (var); (var) = LIST_NEXT(var, field))

#define LIST_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = LIST_FIRST(head); \
	     (var) && ((tvar) = LIST_NEXT(var, field), 1); (var) = (tvar))

#define LIST_INSERT_HEAD(head, elm, field) do { \
	if (((elm)->field.le_next = (head)->lh_first) != NULL) \
		(head)->lh_first->field.le_prev = &(elm)->field.le_next; \
	(head)->lh_first = (elm); \
	(elm)->field.le_prev = &(head)->lh_first; \
} while (0)

#define LIST_INSERT_AFTER(listelm, elm, field) do { \
	if (((elm)->field.le_next = (listelm)->field.le_next) != NULL) \
		(listelm)->field.le_next->field.le_prev = &(elm)->field.le_next; \
	(listelm)->field.le_next = (elm); \
	(elm)->field.le_prev = &(listelm)->field.le_next; \
} while (0)

#define LIST_INSERT_BEFORE(listelm, elm, field) do { \
	(elm)->field.le_prev = (listelm)->field.le_prev; \
	(elm)->field.le_next = (listelm); \
	*(listelm)->field.le_prev = (elm); \
	(listelm)->field.le_prev = &(elm)->field.le_next; \
} while (0)

#define LIST_REMOVE(elm, field) do { \
	if ((elm)->field.le_next != NULL) \
		(elm)->field.le_next->field.le_prev = (elm)->field.le_prev; \
	*(elm)->field.le_prev = (elm)->field.le_next; \
} while (0)

/* ---- simple tail queue ------------------------------------------------- */

#define STAILQ_HEAD(name, type) \
	struct name { struct type *stqh_first; struct type **stqh_last; }
#define STAILQ_HEAD_INITIALIZER(head) { NULL, &(head).stqh_first }
#define STAILQ_ENTRY(type) struct { struct type *stqe_next; }

#define STAILQ_EMPTY(head) ((head)->stqh_first == NULL)
#define STAILQ_FIRST(head) ((head)->stqh_first)
#define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)
#define STAILQ_INIT(head) do { \
	(head)->stqh_first = NULL; \
	(head)->stqh_last = &(head)->stqh_first; \
} while (0)

#define STAILQ_FOREACH(var, head, field) \
	for ((var) = STAILQ_FIRST(head); (var); (var) = STAILQ_NEXT(var, field))

#define STAILQ_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = STAILQ_FIRST(head); \
	     (var) && ((tvar) = STAILQ_NEXT(var, field), 1); (var) = (tvar))

#define STAILQ_INSERT_HEAD(head, elm, field) do { \
	if (((elm)->field.stqe_next = (head)->stqh_first) == NULL) \
		(head)->stqh_last = &(elm)->field.stqe_next; \
	(head)->stqh_first = (elm); \
} while (0)

#define STAILQ_INSERT_TAIL(head, elm, field) do { \
	(elm)->field.stqe_next = NULL; \
	*(head)->stqh_last = (elm); \
	(head)->stqh_last = &(elm)->field.stqe_next; \
} while (0)

#define STAILQ_INSERT_AFTER(head, listelm, elm, field) do { \
	if (((elm)->field.stqe_next = (listelm)->field.stqe_next) == NULL) \
		(head)->stqh_last = &(elm)->field.stqe_next; \
	(listelm)->field.stqe_next = (elm); \
} while (0)

#define STAILQ_REMOVE_HEAD(head, field) do { \
	if (((head)->stqh_first = (head)->stqh_first->field.stqe_next) == NULL) \
		(head)->stqh_last = &(head)->stqh_first; \
} while (0)

#define STAILQ_REMOVE(head, elm, type, field) do { \
	if ((head)->stqh_first == (elm)) { \
		STAILQ_REMOVE_HEAD(head, field); \
	} else { \
		struct type *kn_cur = (head)->stqh_first; \
		while (kn_cur->field.stqe_next != (elm)) \
			kn_cur = kn_cur->field.stqe_next; \
		if ((kn_cur->field.stqe_next = (elm)->field.stqe_next) == NULL) \
			(head)->stqh_last = &kn_cur->field.stqe_next; \
	} \
} while (0)

#define STAILQ_CONCAT(head1, head2) do { \
	if (!STAILQ_EMPTY(head2)) { \
		*(head1)->stqh_last = (head2)->stqh_first; \
		(head1)->stqh_last = (head2)->stqh_last; \
		STAILQ_INIT(head2); \
	} \
} while (0)

/* ---- tail queue -------------------------------------------------------- */

#define TAILQ_HEAD(name, type) \
	struct name { struct type *tqh_first; struct type **tqh_last; }
#define TAILQ_HEAD_INITIALIZER(head) { NULL, &(head).tqh_first }
#define TAILQ_ENTRY(type) \
	struct { struct type *tqe_next; struct type **tqe_prev; }

#define TAILQ_EMPTY(head) ((head)->tqh_first == NULL)
#define TAILQ_FIRST(head) ((head)->tqh_first)
#define TAILQ_NEXT(elm, field) ((elm)->field.tqe_next)
#define TAILQ_LAST(head, headname) \
	(*(((struct headname *)((head)->tqh_last))->tqh_last))
#define TAILQ_PREV(elm, headname, field) \
	(*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))
#define TAILQ_INIT(head) do { \
	(head)->tqh_first = NULL; \
	(head)->tqh_last = &(head)->tqh_first; \
} while (0)

#define TAILQ_FOREACH(var, head, field) \
	for ((var) = TAILQ_FIRST(head); (var); (var) = TAILQ_NEXT(var, field))

#define TAILQ_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = TAILQ_FIRST(head); \
	     (var) && ((tvar) = TAILQ_NEXT(var, field), 1); (var) = (tvar))

#define TAILQ_FOREACH_REVERSE(var, head, headname, field) \
	for ((var) = TAILQ_LAST(head, headname); (var); \
	     (var) = TAILQ_PREV(var, headname, field))

#define TAILQ_FOREACH_REVERSE_SAFE(var, head, headname, field, tvar) \
	for ((var) = TAILQ_LAST(head, headname); \
	     (var) && ((tvar) = TAILQ_PREV(var, headname, field), 1); \
	     (var) = (tvar))

#define TAILQ_INSERT_HEAD(head, elm, field) do { \
	if (((elm)->field.tqe_next = (head)->tqh_first) != NULL) \
		(head)->tqh_first->field.tqe_prev = &(elm)->field.tqe_next; \
	else \
		(head)->tqh_last = &(elm)->field.tqe_next; \
	(head)->tqh_first = (elm); \
	(elm)->field.tqe_prev = &(head)->tqh_first; \
} while (0)

#define TAILQ_INSERT_TAIL(head, elm, field) do { \
	(elm)->field.tqe_next = NULL; \
	(elm)->field.tqe_prev = (head)->tqh_last; \
	*(head)->tqh_last = (elm); \
	(head)->tqh_last = &(elm)->field.tqe_next; \
} while (0)

#define TAILQ_INSERT_AFTER(head, listelm, elm, field) do { \
	if (((elm)->field.tqe_next = (listelm)->field.tqe_next) != NULL) \
		(elm)->field.tqe_next->field.tqe_prev = &(elm)->field.tqe_next; \
	else \
		(head)->tqh_last = &(elm)->field.tqe_next; \
	(listelm)->field.tqe_next = (elm); \
	(elm)->field.tqe_prev = &(listelm)->field.tqe_next; \
} while (0)

#define TAILQ_INSERT_BEFORE(listelm, elm, field) do { \
	(elm)->field.tqe_prev = (listelm)->field.tqe_prev; \
	(elm)->field.tqe_next = (listelm); \
	*(listelm)->field.tqe_prev = (elm); \
	(listelm)->field.tqe_prev = &(elm)->field.tqe_next; \
} while (0)

#define TAILQ_REMOVE(head, elm, field) do { \
	if (((elm)->field.tqe_next) != NULL) \
		(elm)->field.tqe_next->field.tqe_prev = (elm)->field.tqe_prev; \
	else \
		(head)->tqh_last = (elm)->field.tqe_prev; \
	*(elm)->field.tqe_prev = (elm)->field.tqe_next; \
} while (0)

#define TAILQ_CONCAT(head1, head2, field) do { \
	if (!TAILQ_EMPTY(head2)) { \
		*(head1)->tqh_last = (head2)->tqh_first; \
		(head2)->tqh_first->field.tqe_prev = (head1)->tqh_last; \
		(head1)->tqh_last = (head2)->tqh_last; \
		TAILQ_INIT(head2); \
	} \
} while (0)

/* ---- circular queue ---------------------------------------------------- */

#define CIRCLEQ_HEAD(name, type) \
	struct name { struct type *cqh_first; struct type *cqh_last; }
#define CIRCLEQ_HEAD_INITIALIZER(head) \
	{ (void *)&(head), (void *)&(head) }
#define CIRCLEQ_ENTRY(type) \
	struct { struct type *cqe_next; struct type *cqe_prev; }

#define CIRCLEQ_EMPTY(head) ((head)->cqh_first == (void *)(head))
#define CIRCLEQ_FIRST(head) ((head)->cqh_first)
#define CIRCLEQ_LAST(head) ((head)->cqh_last)
#define CIRCLEQ_NEXT(elm, field) ((elm)->field.cqe_next)
#define CIRCLEQ_PREV(elm, field) ((elm)->field.cqe_prev)
#define CIRCLEQ_LOOP_NEXT(head, elm, field) \
	(((elm)->field.cqe_next == (void *)(head)) \
	 ? ((head)->cqh_first) : ((elm)->field.cqe_next))
#define CIRCLEQ_LOOP_PREV(head, elm, field) \
	(((elm)->field.cqe_prev == (void *)(head)) \
	 ? ((head)->cqh_last) : ((elm)->field.cqe_prev))
#define CIRCLEQ_INIT(head) do { \
	(head)->cqh_first = (void *)(head); \
	(head)->cqh_last = (void *)(head); \
} while (0)

#define CIRCLEQ_FOREACH(var, head, field) \
	for ((var) = (head)->cqh_first; (var) != (const void *)(head); \
	     (var) = (var)->field.cqe_next)

#define CIRCLEQ_FOREACH_REVERSE(var, head, field) \
	for ((var) = (head)->cqh_last; (var) != (const void *)(head); \
	     (var) = (var)->field.cqe_prev)

#define CIRCLEQ_INSERT_AFTER(head, listelm, elm, field) do { \
	(elm)->field.cqe_next = (listelm)->field.cqe_next; \
	(elm)->field.cqe_prev = (listelm); \
	if ((listelm)->field.cqe_next == (void *)(head)) \
		(head)->cqh_last = (elm); \
	else \
		(listelm)->field.cqe_next->field.cqe_prev = (elm); \
	(listelm)->field.cqe_next = (elm); \
} while (0)

#define CIRCLEQ_INSERT_BEFORE(head, listelm, elm, field) do { \
	(elm)->field.cqe_next = (listelm); \
	(elm)->field.cqe_prev = (listelm)->field.cqe_prev; \
	if ((listelm)->field.cqe_prev == (void *)(head)) \
		(head)->cqh_first = (elm); \
	else \
		(listelm)->field.cqe_prev->field.cqe_next = (elm); \
	(listelm)->field.cqe_prev = (elm); \
} while (0)

#define CIRCLEQ_INSERT_HEAD(head, elm, field) do { \
	(elm)->field.cqe_next = (head)->cqh_first; \
	(elm)->field.cqe_prev = (void *)(head); \
	if ((head)->cqh_last == (void *)(head)) \
		(head)->cqh_last = (elm); \
	else \
		(head)->cqh_first->field.cqe_prev = (elm); \
	(head)->cqh_first = (elm); \
} while (0)

#define CIRCLEQ_INSERT_TAIL(head, elm, field) do { \
	(elm)->field.cqe_next = (void *)(head); \
	(elm)->field.cqe_prev = (head)->cqh_last; \
	if ((head)->cqh_first == (void *)(head)) \
		(head)->cqh_first = (elm); \
	else \
		(head)->cqh_last->field.cqe_next = (elm); \
	(head)->cqh_last = (elm); \
} while (0)

#define CIRCLEQ_REMOVE(head, elm, field) do { \
	if ((elm)->field.cqe_next == (void *)(head)) \
		(head)->cqh_last = (elm)->field.cqe_prev; \
	else \
		(elm)->field.cqe_next->field.cqe_prev = (elm)->field.cqe_prev; \
	if ((elm)->field.cqe_prev == (void *)(head)) \
		(head)->cqh_first = (elm)->field.cqe_next; \
	else \
		(elm)->field.cqe_prev->field.cqe_next = (elm)->field.cqe_next; \
} while (0)

#endif
