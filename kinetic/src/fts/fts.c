#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fts.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Directory-tree traversal with the fts(3) interface.
 *
 * Original implementation. Notable design choice: the process never
 * changes its working directory (every entry is reached by its full path,
 * so fts_accpath == fts_path, exactly as with FTS_NOCHDIR). This keeps fts
 * safe to use from threads; programs that merely use fts_accpath, which is
 * all portable ones, behave identically.
 */

/* Per-entry private data lives in front of the public FTSENT in the same
 * allocation. Sized to keep the FTSENT 16-byte aligned. */
struct hdr {
	struct stat st;
	FTSENT *kids;     /* children, valid once (state & BUILT) */
	FTSENT *nextkid;  /* next child to hand out from fts_read */
	int state;
};
#define BUILT 1
#define NAMEONLY 2
#define HDRSZ ((sizeof(struct hdr) + 15) & ~(size_t)15)
#define HDR(e) ((struct hdr *)((char *)(e) - HDRSZ))

struct priv {
	FTS pub;          /* must be first */
	FTSENT *sentinel; /* virtual parent of the root entries */
	int started, done;
};

static FTSENT *ent_new(const char *name, size_t namelen, const char *dir,
	size_t dirlen)
{
	int slash = dirlen && dir[dirlen-1] != '/';
	size_t pathlen = dirlen + slash + namelen;
	size_t sz = HDRSZ + offsetof(FTSENT, fts_name) + namelen + 1 + pathlen + 1;
	char *mem = calloc(1, sz);
	FTSENT *e;
	char *path;

	if (!mem) return 0;
	e = (FTSENT *)(mem + HDRSZ);
	memcpy(e->fts_name, name, namelen);
	path = e->fts_name + namelen + 1;
	if (dirlen) memcpy(path, dir, dirlen);
	if (slash) path[dirlen] = '/';
	memcpy(path + dirlen + slash, name, namelen);
	e->fts_path = e->fts_accpath = path;
	e->fts_namelen = namelen > 65535 ? 65535 : namelen;
	e->fts_pathlen = pathlen > 65535 ? 65535 : pathlen;
	e->fts_statp = &HDR(e)->st;
	e->fts_instr = FTS_NOINSTR;
	e->fts_symfd = -1;
	return e;
}

static void list_free(FTSENT *e)
{
	while (e) {
		FTSENT *n = e->fts_link;
		list_free(HDR(e)->kids);
		free(HDR(e));
		e = n;
	}
}

/* Fill in fts_info and the stat-derived fields. dtype is the readdir
 * d_type hint, or -1 when there is none. */
static void classify(FTSENT *e, int follow, int dtype)
{
	struct stat *st = e->fts_statp;
	int r;

	e->fts_errno = 0;
	if (dtype >= 0 && dtype != DT_DIR && dtype != DT_UNKNOWN && dtype != DT_LNK) {
		e->fts_info = FTS_NSOK;
		return;
	}
	r = follow ? stat(e->fts_accpath, st) : lstat(e->fts_accpath, st);
	if (r < 0) {
		int err = errno;
		if (follow && lstat(e->fts_accpath, st) == 0 && S_ISLNK(st->st_mode)) {
			e->fts_info = FTS_SLNONE;
		} else {
			memset(st, 0, sizeof *st);
			e->fts_info = FTS_NS;
			e->fts_errno = err;
		}
		return;
	}
	e->fts_ino = st->st_ino;
	e->fts_dev = st->st_dev;
	e->fts_nlink = st->st_nlink;
	if (S_ISDIR(st->st_mode)) e->fts_info = FTS_D;
	else if (S_ISLNK(st->st_mode)) e->fts_info = FTS_SL;
	else if (S_ISREG(st->st_mode)) e->fts_info = FTS_F;
	else e->fts_info = FTS_DEFAULT;
}

static int cmp_tramp(const void *a, const void *b, void *arg)
{
	int (*cmp)(const FTSENT **, const FTSENT **) =
		(int (*)(const FTSENT **, const FTSENT **))arg;
	return cmp((const FTSENT **)a, (const FTSENT **)b);
}

/* Sort a sibling chain (linked through fts_link) with the caller compare. */
static FTSENT *list_sort(FTSENT *head, int n,
	int (*cmp)(const FTSENT **, const FTSENT **))
{
	FTSENT **v, *e;
	int i;

	if (!cmp || n < 2) return head;
	v = malloc(n * sizeof *v);
	if (!v) return head; /* unsorted is better than failing */
	for (i = 0, e = head; e && i < n; e = e->fts_link) v[i++] = e;
	qsort_r(v, n, sizeof *v, cmp_tramp, (void *)cmp);
	for (i = 0; i < n-1; i++) v[i]->fts_link = v[i+1];
	v[n-1]->fts_link = 0;
	head = v[0];
	free(v);
	return head;
}

static int follow_for(struct priv *sp, FTSENT *e)
{
	int o = sp->pub.fts_options;
	if (o & FTS_LOGICAL) return 1;
	return e->fts_level == FTS_ROOTLEVEL && (o & FTS_COMFOLLOW);
}

FTS *fts_open(char *const *argv, int options,
	int (*compar)(const FTSENT **, const FTSENT **))
{
	struct priv *sp;
	FTSENT *head = 0, *tail = 0;
	int n = 0, i;

	if ((options & ~(FTS_OPTIONMASK|FTS_STOP|FTS_NAMEONLY))
	    || !(options & (FTS_LOGICAL|FTS_PHYSICAL))
	    || ((options & FTS_LOGICAL) && (options & FTS_PHYSICAL))) {
		errno = EINVAL;
		return 0;
	}

	sp = calloc(1, sizeof *sp);
	if (!sp) return 0;
	sp->pub.fts_compar = compar;
	sp->pub.fts_options = options;
	sp->pub.fts_rfd = -1;

	sp->sentinel = ent_new("", 0, "", 0);
	if (!sp->sentinel) goto fail;
	sp->sentinel->fts_level = FTS_ROOTPARENTLEVEL;
	sp->sentinel->fts_info = FTS_INIT;

	for (i = 0; argv && argv[i]; i++) {
		FTSENT *e;
		if (!argv[i][0]) {
			errno = ENOENT;
			goto fail;
		}
		e = ent_new(argv[i], strlen(argv[i]), "", 0);
		if (!e) goto fail;
		e->fts_parent = sp->sentinel;
		e->fts_level = FTS_ROOTLEVEL;
		if (tail) tail->fts_link = e; else head = e;
		tail = e;
		n++;
		/* keep the list reachable so failure paths can free it */
		HDR(sp->sentinel)->kids = head;
		/* a root that cannot be stat'ed is still reported by fts_read */
		classify(e, follow_for(sp, e), -1);
	}
	HDR(sp->sentinel)->kids = list_sort(head, n, compar);
	HDR(sp->sentinel)->nextkid = HDR(sp->sentinel)->kids;
	sp->pub.fts_nitems = n;
	return &sp->pub;

fail:
	{
		int err = errno;
		if (sp->sentinel) {
			list_free(HDR(sp->sentinel)->kids);
			free(HDR(sp->sentinel));
		}
		free(sp);
		errno = err;
	}
	return 0;
}

/* Read directory p into a list of children. Returns 0, or -1 with errno. */
static int build(struct priv *sp, FTSENT *p, int nameonly)
{
	struct hdr *h = HDR(p);
	DIR *d;
	struct dirent *de;
	FTSENT *head = 0, *tail = 0;
	int n = 0;
	int options = sp->pub.fts_options;

	d = opendir(p->fts_accpath);
	if (!d) return -1;
	while ((de = readdir(d))) {
		FTSENT *e;
		int dot = de->d_name[0] == '.' &&
			(!de->d_name[1] || (de->d_name[1] == '.' && !de->d_name[2]));
		if (dot && !(options & FTS_SEEDOT)) continue;
		e = ent_new(de->d_name, strlen(de->d_name), p->fts_path, strlen(p->fts_path));
		if (!e) {
			int err = errno;
			closedir(d);
			list_free(head);
			errno = err;
			return -1;
		}
		e->fts_parent = p;
		e->fts_level = p->fts_level + 1;
		e->fts_ino = de->d_ino;
		if (dot) e->fts_info = FTS_DOT;
		else if (nameonly) e->fts_info = FTS_NSOK;
		else classify(e, follow_for(sp, e), (options & FTS_NOSTAT) ? de->d_type : -1);
		if (tail) tail->fts_link = e; else head = e;
		tail = e;
		n++;
	}
	closedir(d);
	h->kids = list_sort(head, n, sp->pub.fts_compar);
	h->nextkid = h->kids;
	h->state = BUILT | (nameonly ? NAMEONLY : 0);
	sp->pub.fts_nitems = n;
	return 0;
}

/* Look for an ancestor of e that is the same directory (symlink loop). */
static FTSENT *find_cycle(FTSENT *e)
{
	FTSENT *a;
	for (a = e->fts_parent; a && a->fts_level >= FTS_ROOTLEVEL; a = a->fts_parent)
		if (a->fts_ino == e->fts_ino && a->fts_dev == e->fts_dev) return a;
	return 0;
}

static FTSENT *visit(struct priv *sp, FTSENT *e)
{
	if (e->fts_info == FTS_D && (sp->pub.fts_options & FTS_LOGICAL)) {
		FTSENT *c = find_cycle(e);
		if (c) {
			e->fts_cycle = c;
			e->fts_info = FTS_DC;
		}
	}
	if (e->fts_level == FTS_ROOTLEVEL) sp->pub.fts_dev = e->fts_dev;
	sp->pub.fts_cur = e;
	return e;
}

/* Hand out the next sibling under par, or finish par in post-order. */
static FTSENT *advance(struct priv *sp, FTSENT *par)
{
	struct hdr *h = HDR(par);
	FTSENT *nxt = h->nextkid;

	if (nxt) {
		h->nextkid = nxt->fts_link;
		return visit(sp, nxt);
	}
	if (par == sp->sentinel) {
		sp->done = 1;
		sp->pub.fts_cur = 0;
		errno = 0;
		return 0;
	}
	/* all children visited: release them and report the directory again */
	list_free(h->kids);
	h->kids = h->nextkid = 0;
	h->state = 0;
	par->fts_info = FTS_DP;
	sp->pub.fts_cur = par;
	return par;
}

FTSENT *fts_read(FTS *s)
{
	struct priv *sp = (struct priv *)s;
	FTSENT *p;
	struct hdr *h;

	if (sp->done || (s->fts_options & FTS_STOP)) {
		errno = 0;
		return 0;
	}
	p = s->fts_cur;
	if (!p) {
		if (sp->started) return 0;
		sp->started = 1;
		return advance(sp, sp->sentinel);
	}
	h = HDR(p);

	if (p->fts_instr == FTS_AGAIN) {
		p->fts_instr = FTS_NOINSTR;
		if (p->fts_info == FTS_DP) return p;
		list_free(h->kids);
		h->kids = h->nextkid = 0;
		h->state = 0;
		classify(p, follow_for(sp, p), -1);
		return visit(sp, p);
	}
	if (p->fts_instr == FTS_FOLLOW &&
	    (p->fts_info == FTS_SL || p->fts_info == FTS_SLNONE)) {
		p->fts_instr = FTS_NOINSTR;
		classify(p, 1, -1);
		return visit(sp, p);
	}

	if (p->fts_info == FTS_D) {
		if (p->fts_instr == FTS_SKIP ||
		    ((s->fts_options & FTS_XDEV) && p->fts_dev != s->fts_dev)) {
			list_free(h->kids);
			h->kids = h->nextkid = 0;
			h->state = 0;
			p->fts_info = FTS_DP;
			return p;
		}
		/* reuse children fetched by fts_children(), unless name-only */
		if (!(h->state & BUILT) || (h->state & NAMEONLY)) {
			list_free(h->kids);
			h->kids = h->nextkid = 0;
			if (build(sp, p, 0) < 0) {
				p->fts_errno = errno;
				p->fts_info = FTS_DNR;
				h->state = 0;
				return p;
			}
		}
		if (h->kids) {
			FTSENT *c = h->kids;
			h->nextkid = c->fts_link;
			return visit(sp, c);
		}
		p->fts_info = FTS_DP;
		return p;
	}

	return advance(sp, p->fts_parent);
}

FTSENT *fts_children(FTS *s, int instr)
{
	struct priv *sp = (struct priv *)s;
	FTSENT *p = s->fts_cur;
	struct hdr *h;
	int nameonly = instr == FTS_NAMEONLY;

	if (instr && instr != FTS_NAMEONLY) {
		errno = EINVAL;
		return 0;
	}
	errno = 0;
	if (sp->done || (s->fts_options & FTS_STOP)) return 0;
	if (!p) return HDR(sp->sentinel)->kids; /* before the first read: the roots */
	if (p->fts_info != FTS_D) return 0;

	h = HDR(p);
	if (!(h->state & BUILT) || !!(h->state & NAMEONLY) != nameonly) {
		list_free(h->kids);
		h->kids = h->nextkid = 0;
		if (build(sp, p, nameonly) < 0) {
			p->fts_errno = errno;
			h->state = 0;
			return 0;
		}
	}
	s->fts_child = h->kids;
	return h->kids;
}

int fts_set(FTS *s, FTSENT *f, int instr)
{
	(void)s;
	if (instr && instr != FTS_AGAIN && instr != FTS_FOLLOW &&
	    instr != FTS_NOINSTR && instr != FTS_SKIP) {
		errno = EINVAL;
		return 1;
	}
	f->fts_instr = instr;
	return 0;
}

int fts_close(FTS *s)
{
	struct priv *sp = (struct priv *)s;
	if (!s) {
		errno = EINVAL;
		return -1;
	}
	list_free(HDR(sp->sentinel)->kids);
	free(HDR(sp->sentinel));
	free(sp);
	return 0;
}
