/* Kinetic compatibility-layer tests, part 2: mallinfo, fts, sys/tree.h,
 * argp. Built and run by tests/run.sh. Exit status = number of failures. */
#define _GNU_SOURCE
#include <argp.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/tree.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures, checks;

#define CHECK(cond) do { \
	checks++; \
	if (!(cond)) { \
		failures++; \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
	} \
} while (0)

#define SECTION(name) fprintf(stderr, "-- %s\n", name)

/* ---- mallinfo ---------------------------------------------------------- */

static void test_mallinfo(void)
{
	SECTION("mallinfo (real statistics)");
	struct mallinfo2 a, b, c, d, e;
	enum { N = 1000 };
	void *p[N];
	void *volatile keep = malloc(64);   /* keeps the arena non-empty */
	int i;

	a = mallinfo2();

	for (i = 0; i < N; i++) p[i] = malloc(100);
	b = mallinfo2();
	CHECK(b.uordblks >= a.uordblks + N * 100);
	CHECK(b.arena >= a.arena);
	CHECK(b.uordblks + b.fordblks == b.arena);

	for (i = 0; i < N; i++) free(p[i]);
	c = mallinfo2();
	CHECK(c.uordblks + 90000 < b.uordblks);       /* most of it came back */

	{
		size_t big = 2u << 20;
		/* volatile + a store: the compiler may not elide the malloc/free pair */
		char *volatile m = malloc(big);
		m[0] = 1;
		d = mallinfo2();
		CHECK(d.hblks == c.hblks + 1);
		CHECK(d.hblkd >= c.hblkd + big);
		free(m);
		e = mallinfo2();
		CHECK(e.hblks == c.hblks);
		CHECK(e.hblkd == c.hblkd);
	}

	/* the int-typed struct agrees, and malloc_stats() does not crash */
	{
		struct mallinfo mi = mallinfo();
		struct mallinfo2 m2 = mallinfo2();
		CHECK(mi.arena > 0 && (size_t)mi.arena <= m2.arena + 65536);
		CHECK(mi.uordblks > 0);
	}
	free(keep);
	{
		int saved = dup(2);
		freopen("/dev/null", "w", stderr);
		malloc_stats();
		fflush(stderr);
		dup2(saved, 2);
		close(saved);
		clearerr(stderr);
	}
}

/* ---- fts --------------------------------------------------------------- */

static char trace[8192];
static size_t rootlen;

static void tr_add(int info, const char *path)
{
	char k = '?';
	const char *rel = path + rootlen;
	switch (info) {
	case FTS_D: k = 'D'; break;
	case FTS_DP: k = 'P'; break;
	case FTS_F: k = 'F'; break;
	case FTS_SL: k = 'L'; break;
	case FTS_SLNONE: k = 'N'; break;
	case FTS_DC: k = 'C'; break;
	case FTS_DNR: k = 'R'; break;
	case FTS_NS: k = '?'; break;
	case FTS_DOT: k = '.'; break;
	case FTS_NSOK: k = 'f'; break;
	case FTS_DEFAULT: k = 'S'; break;
	}
	size_t n = strlen(trace);
	snprintf(trace + n, sizeof trace - n, "%s%c:%s", n ? " " : "", k, *rel ? rel : ".");
}

static int by_name(const FTSENT **a, const FTSENT **b)
{
	return strcmp((*a)->fts_name, (*b)->fts_name);
}

static void mk(const char *base, const char *rel, const char *content)
{
	char path[512];
	snprintf(path, sizeof path, "%s/%s", base, rel);
	if (content) {
		int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (fd >= 0) { write(fd, content, strlen(content)); close(fd); }
	} else {
		mkdir(path, 0755);
	}
}

static void mklink(const char *base, const char *rel, const char *target)
{
	char path[512];
	snprintf(path, sizeof path, "%s/%s", base, rel);
	symlink(target, path);
}

static void rm_tree(const char *root)
{
	char *paths[2] = { (char *)root, 0 };
	FTS *f = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, 0);
	FTSENT *e;
	if (!f) return;
	while ((e = fts_read(f))) {
		if (e->fts_info == FTS_DP) rmdir(e->fts_accpath);
		else if (e->fts_info != FTS_D) unlink(e->fts_accpath);
	}
	fts_close(f);
}

static void walk(const char *root, int opts, const char *skip_name)
{
	char *paths[2] = { (char *)root, 0 };
	FTS *f = fts_open(paths, opts, by_name);
	FTSENT *e;
	trace[0] = 0;
	rootlen = strlen(root);
	if (!f) { strcpy(trace, "OPEN-FAILED"); return; }
	while ((e = fts_read(f))) {
		tr_add(e->fts_info, e->fts_path);
		if (skip_name && e->fts_info == FTS_D && !strcmp(e->fts_name, skip_name))
			fts_set(f, e, FTS_SKIP);
	}
	CHECK(errno == 0);
	fts_close(f);
}

static void test_fts(void)
{
	SECTION("fts");
	char base[] = "/tmp/kinetic-fts-XXXXXX";
	char root[600], root2[600];

	CHECK(mkdtemp(base) != 0);
	snprintf(root, sizeof root, "%s/root", base);
	snprintf(root2, sizeof root2, "%s/root2", base);
	mkdir(root, 0755);
	mk(root, "a", 0); mk(root, "a/x.txt", "x"); mk(root, "a/y.txt", "yy");
	mk(root, "b", 0); mk(root, "b/c", 0); mk(root, "b/c/z.txt", "z");
	mk(root, "empty", 0); mk(root, "file1", "f");
	mklink(root, "link", "a");
	mklink(root, "dangling", "nonexistent");
	mkdir(root2, 0755);
	mk(root2, "d", 0);
	mklink(root2, "d/loop", "..");

	walk(root, FTS_PHYSICAL, 0);
	CHECK(!strcmp(trace,
		"D:. D:/a F:/a/x.txt F:/a/y.txt P:/a D:/b D:/b/c F:/b/c/z.txt P:/b/c P:/b "
		"L:/dangling D:/empty P:/empty F:/file1 L:/link P:."));

	walk(root, FTS_LOGICAL, 0);
	CHECK(!strcmp(trace,
		"D:. D:/a F:/a/x.txt F:/a/y.txt P:/a D:/b D:/b/c F:/b/c/z.txt P:/b/c P:/b "
		"N:/dangling D:/empty P:/empty F:/file1 D:/link F:/link/x.txt F:/link/y.txt "
		"P:/link P:."));

	walk(root2, FTS_LOGICAL, 0);
	CHECK(!strcmp(trace, "D:. D:/d C:/d/loop P:/d P:."));

	walk(root, FTS_PHYSICAL, "b");              /* FTS_SKIP: no descent into b */
	CHECK(strstr(trace, "D:/b P:/b L:/dangling") != 0);
	CHECK(strstr(trace, "/b/c") == 0);

	/* levels, stat data, multiple roots keep their order */
	{
		char r1[700], r2[700];
		char *paths[3] = { r1, r2, 0 };
		FTS *f;
		FTSENT *e;
		int saw_z = 0, order_ok = 0, ndirs = 0;
		snprintf(r1, sizeof r1, "%s/b", root);
		snprintf(r2, sizeof r2, "%s/a", root);
		f = fts_open(paths, FTS_PHYSICAL, 0);
		CHECK(f != 0);
		while (f && (e = fts_read(f))) {
			if (e->fts_info == FTS_D) ndirs++;
			if (e->fts_info == FTS_D && e->fts_level == 0 && ndirs == 1)
				order_ok = !strcmp(e->fts_name, r1) || strstr(e->fts_path, "/b") != 0;
			if (e->fts_info == FTS_F && !strcmp(e->fts_name, "z.txt")) {
				saw_z = 1;
				CHECK(e->fts_level == 2);       /* b=0, c=1, z.txt=2 */
				CHECK(S_ISREG(e->fts_statp->st_mode) && e->fts_statp->st_size == 1);
				CHECK(e->fts_namelen == 5);
				CHECK(e->fts_parent && !strcmp(e->fts_parent->fts_name, "c"));
			}
		}
		CHECK(saw_z && order_ok);
		CHECK(ndirs == 3);                      /* b, b/c, a */
		fts_close(f);
	}

	/* fts_children: roots before the first read; names only; real entries */
	{
		char *paths[2] = { root, 0 };
		FTS *f = fts_open(paths, FTS_PHYSICAL, by_name);
		FTSENT *e, *c;
		int n = 0;
		CHECK(f != 0);
		c = fts_children(f, 0);
		CHECK(c && c->fts_level == 0 && !c->fts_link);
		e = fts_read(f);
		CHECK(e && e->fts_info == FTS_D);
		c = fts_children(f, FTS_NAMEONLY);
		CHECK(c && !strcmp(c->fts_name, "a") && c->fts_info == FTS_NSOK);
		for (n = 0; c; c = c->fts_link) n++;
		CHECK(n == 6);
		c = fts_children(f, 0);                 /* now with full stat info */
		CHECK(c && c->fts_info == FTS_D);
		e = fts_read(f);                        /* uses the children just fetched */
		CHECK(e && !strcmp(e->fts_name, "a") && e->fts_info == FTS_D);
		CHECK(fts_children(f, 99) == 0 && errno == EINVAL);
		fts_close(f);
	}

	/* FTS_SEEDOT yields "." and ".." for every directory */
	{
		char *paths[2] = { root, 0 };
		FTS *f = fts_open(paths, FTS_PHYSICAL | FTS_SEEDOT, 0);
		FTSENT *e;
		int dots = 0;
		while (f && (e = fts_read(f))) if (e->fts_info == FTS_DOT) dots++;
		CHECK(dots == 10);                      /* root a b c empty */
		fts_close(f);
	}

	/* FTS_NOSTAT avoids stat for non-directories */
	{
		char *paths[2] = { root, 0 };
		FTS *f = fts_open(paths, FTS_PHYSICAL | FTS_NOSTAT, 0);
		FTSENT *e;
		int nsok = 0, dirs = 0;
		while (f && (e = fts_read(f))) {
			if (e->fts_info == FTS_NSOK) nsok++;
			if (e->fts_info == FTS_D) dirs++;
		}
		CHECK(dirs == 5);                       /* root a b c empty */
		CHECK(nsok >= 4);
		fts_close(f);
	}

	/* errors */
	{
		char *bad[2] = { "/nonexistent-kinetic-path", 0 };
		char *empty[2] = { "", 0 };
		FTS *f = fts_open(bad, FTS_PHYSICAL, 0);
		FTSENT *e;
		CHECK(f != 0);
		e = fts_read(f);
		CHECK(e && e->fts_info == FTS_NS && e->fts_errno == ENOENT);
		CHECK(fts_read(f) == 0);
		fts_close(f);
		CHECK(fts_open(bad, 0, 0) == 0 && errno == EINVAL);
		CHECK(fts_open(bad, FTS_LOGICAL | FTS_PHYSICAL, 0) == 0 && errno == EINVAL);
		CHECK(fts_open(empty, FTS_PHYSICAL, 0) == 0 && errno == ENOENT);
	}

	/* unreadable directory is reported, not fatal (skipped when root) */
	if (geteuid() != 0) {
		char *paths[2] = { root, 0 };
		FTS *f;
		FTSENT *e;
		int dnr = 0;
		char d[700];
		snprintf(d, sizeof d, "%s/empty", root);
		chmod(d, 0);
		f = fts_open(paths, FTS_PHYSICAL, 0);
		while (f && (e = fts_read(f)))
			if (e->fts_info == FTS_DNR) { dnr++; CHECK(e->fts_errno == EACCES); }
		CHECK(dnr == 1);
		fts_close(f);
		chmod(d, 0755);
	}

	/* no leaks: repeated traversals leave the heap where it was */
	{
		char *paths[2] = { root, 0 };
		struct mallinfo2 before, after;
		int i;
		walk(root, FTS_LOGICAL, 0);             /* warm up allocator state */
		before = mallinfo2();
		for (i = 0; i < 300; i++) {
			FTS *f = fts_open(paths, FTS_LOGICAL, by_name);
			FTSENT *e;
			while ((e = fts_read(f))) {
				if (i % 3 == 0 && e->fts_info == FTS_D && e->fts_level == 1)
					fts_set(f, e, FTS_SKIP);
			}
			fts_close(f);
		}
		after = mallinfo2();
		CHECK(after.uordblks < before.uordblks + 16384);
	}

	rm_tree(base);
	CHECK(access(base, F_OK) == -1);
}

/* ---- sys/tree.h -------------------------------------------------------- */

struct node {
	int key;
	RB_ENTRY(node) link;
};
RB_HEAD(ntree, node);
static int ncmp(struct node *a, struct node *b) { return (a->key > b->key) - (a->key < b->key); }
RB_PROTOTYPE_STATIC(ntree, node, link, ncmp)
RB_GENERATE_STATIC(ntree, node, link, ncmp)

/* returns the black height, or -1 if an invariant is broken */
static int rb_check(struct node *n, struct node *parent)
{
	int l, r;
	if (!n) return 1;
	if (RB_PARENT(n, link) != parent) return -1;
	if (RB_COLOR(n, link) == RB_RED && parent && RB_COLOR(parent, link) == RB_RED) return -1;
	l = rb_check(RB_LEFT(n, link), n);
	r = rb_check(RB_RIGHT(n, link), n);
	if (l < 0 || r < 0 || l != r) return -1;
	return l + (RB_COLOR(n, link) == RB_BLACK);
}

#define KEYS 700

static void test_rb(void)
{
	SECTION("sys/tree.h red-black");
	static struct node pool[KEYS];
	static char present[KEYS];
	struct ntree head = RB_INITIALIZER(&head);
	int i, op, count = 0, bad = 0;
	struct node *n, *tmp;

	CHECK(RB_EMPTY(&head) && RB_MIN(ntree, &head) == 0 && RB_MAX(ntree, &head) == 0);
	for (i = 0; i < KEYS; i++) pool[i].key = i * 3;     /* gaps for NFIND */

	srand(12345);
	for (op = 0; op < 30000; op++) {
		int k = rand() % KEYS;
		if (present[k]) {
			struct node *r = RB_REMOVE(ntree, &head, &pool[k]);
			if (r != &pool[k]) bad++;
			present[k] = 0; count--;
		} else {
			if (RB_INSERT(ntree, &head, &pool[k]) != 0) bad++;
			present[k] = 1; count++;
			/* duplicate insert returns the existing node and changes nothing */
			if (RB_INSERT(ntree, &head, &pool[k]) != &pool[k]) bad++;
		}
		if (op % 37 == 0) {
			struct node *root = RB_ROOT(&head);
			if (root && RB_COLOR(root, link) != RB_BLACK) bad++;
			if (rb_check(root, 0) < 0) bad++;
		}
	}
	CHECK(bad == 0);
	CHECK(rb_check(RB_ROOT(&head), 0) >= 0);

	/* ordered iteration matches the model, forward and backward */
	{
		int expect = -1, seen = 0, ok = 1;
		RB_FOREACH(n, ntree, &head) {
			do expect++; while (expect < KEYS && !present[expect]);
			if (expect >= KEYS || n->key != expect * 3) ok = 0;
			seen++;
		}
		CHECK(ok && seen == count);
		seen = 0;
		RB_FOREACH_REVERSE(n, ntree, &head) seen++;
		CHECK(seen == count);
	}

	/* find / nfind / min / max */
	{
		int ok = 1;
		for (i = 0; i < KEYS; i++) {
			struct node probe = { i * 3 }, between = { i * 3 + 1 };
			struct node *f = RB_FIND(ntree, &head, &probe);
			struct node *nf = RB_NFIND(ntree, &head, &between);
			int j = i + 1;
			if ((f != 0) != present[i]) ok = 0;
			while (j < KEYS && !present[j]) j++;
			if (j >= KEYS ? nf != 0 : (!nf || nf->key != j * 3)) ok = 0;
		}
		CHECK(ok);
		if (count) {
			int lo = 0, hi = KEYS - 1;
			while (!present[lo]) lo++;
			while (!present[hi]) hi--;
			CHECK(RB_MIN(ntree, &head)->key == lo * 3);
			CHECK(RB_MAX(ntree, &head)->key == hi * 3);
		}
	}

	/* safe iteration lets us remove while walking */
	RB_FOREACH_SAFE(n, ntree, &head, tmp) RB_REMOVE(ntree, &head, n);
	CHECK(RB_EMPTY(&head));
}

struct snode {
	int key;
	SPLAY_ENTRY(snode) link;
};
SPLAY_HEAD(stree, snode);
static int scmp(struct snode *a, struct snode *b) { return (a->key > b->key) - (a->key < b->key); }
SPLAY_PROTOTYPE(stree, snode, link, scmp)
SPLAY_GENERATE(stree, snode, link, scmp)

static void test_splay(void)
{
	SECTION("sys/tree.h splay");
	static struct snode pool[KEYS];
	static char present[KEYS];
	struct stree head = SPLAY_INITIALIZER(&head);
	int i, op, count = 0, bad = 0;
	struct snode *n;

	CHECK(SPLAY_EMPTY(&head) && SPLAY_MIN(stree, &head) == 0);
	for (i = 0; i < KEYS; i++) pool[i].key = i;
	srand(777);
	for (op = 0; op < 20000; op++) {
		int k = rand() % KEYS;
		if (present[k]) {
			if (SPLAY_REMOVE(stree, &head, &pool[k]) != &pool[k]) bad++;
			present[k] = 0; count--;
		} else {
			if (SPLAY_INSERT(stree, &head, &pool[k]) != 0) bad++;
			present[k] = 1; count++;
		}
		k = rand() % KEYS;
		if ((SPLAY_FIND(stree, &head, &pool[k]) != 0) != present[k]) bad++;
	}
	CHECK(bad == 0);
	{
		int expect = -1, seen = 0, ok = 1;
		SPLAY_FOREACH(n, stree, &head) {
			do expect++; while (expect < KEYS && !present[expect]);
			if (expect >= KEYS || n->key != expect) ok = 0;
			seen++;
		}
		CHECK(ok && seen == count);
	}
	if (count) {
		int lo = 0, hi = KEYS - 1;
		while (!present[lo]) lo++;
		while (!present[hi]) hi--;
		CHECK(SPLAY_MIN(stree, &head)->key == lo);
		CHECK(SPLAY_MAX(stree, &head)->key == hi);
	}
}

/* ---- argp -------------------------------------------------------------- */

struct args {
	int verbose, count, longonly, hidden, child_flag;
	const char *output, *opt;
	int nargs;
	const char *args[16];
	int init, end, noargs, success, error, fini, require;
};

static struct argp_option opts[] = {
	{ "verbose", 'v', 0, 0, "Produce verbose output", 0 },
	{ "noisy", 0, 0, OPTION_ALIAS },
	{ "output", 'o', "FILE", 0, "Write output to FILE", 0 },
	{ "count", 'n', "N", 0, "Repeat N times", 0 },
	{ "opt", 'p', "VAL", OPTION_ARG_OPTIONAL, "Optional value", 0 },
	{ "longonly", 1001, 0, 0, "A long-only option", 0 },
	{ "hidden", 'H', 0, OPTION_HIDDEN, "should not show", 0 },
	{ 0 }
};

static error_t child_parse(int key, char *arg, struct argp_state *st)
{
	(void)arg;
	if (key == 'c') { (*(int *)st->input)++; return 0; }
	return ARGP_ERR_UNKNOWN;
}

static const struct argp_option child_opts[] = {
	{ "child-flag", 'c', 0, 0, "Flag owned by the child parser", 0 },
	{ 0 }
};
static const struct argp child_argp = { child_opts, child_parse, 0, 0, 0, 0, 0 };
static const struct argp_child children[] = { { &child_argp, 0, "Child options:", 0 }, { 0 } };

static error_t parse_opt(int key, char *arg, struct argp_state *st)
{
	struct args *a = st->input;
	switch (key) {
	case 'v': a->verbose++; break;
	case 'o': a->output = arg; break;
	case 'n': a->count = atoi(arg); break;
	case 'p': a->opt = arg ? arg : "(none)"; break;
	case 1001: a->longonly++; break;
	case 'H': a->hidden++; break;
	case ARGP_KEY_ARG:
		if (a->nargs >= 16) return ARGP_ERR_UNKNOWN;
		a->args[a->nargs++] = arg;
		break;
	case ARGP_KEY_INIT: a->init++; st->child_inputs[0] = &a->child_flag; break;
	case ARGP_KEY_NO_ARGS: a->noargs++; break;
	case ARGP_KEY_END:
		a->end++;
		if (a->require && st->arg_num < (unsigned)a->require) return EINVAL;
		break;
	case ARGP_KEY_SUCCESS: a->success++; break;
	case ARGP_KEY_ERROR: a->error++; break;
	case ARGP_KEY_FINI: a->fini++; break;
	default: return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static const struct argp argp = {
	opts, parse_opt, "FILE...", "Test program.\vAfter the options.", children, 0, 0
};

static int run(struct args *a, unsigned flags, int *index, int nargv, ...)
{
	char *argv[32];
	va_list ap;
	int i;
	va_start(ap, nargv);
	argv[0] = "prog";
	for (i = 1; i <= nargv; i++) argv[i] = va_arg(ap, char *);
	argv[nargv + 1] = 0;
	va_end(ap);
	memset(a, 0, sizeof *a);
	return argp_parse(&argp, nargv + 1, argv, flags, index, a);
}

static char outbuf[16384];

static int capture(void (*fn)(void))
{
	int p[2], st = 0;
	ssize_t n, total = 0;
	pid_t pid;
	fflush(0);
	if (pipe(p)) return -1;
	pid = fork();
	if (pid == 0) {
		dup2(p[1], 1);
		dup2(p[1], 2);
		close(p[0]);
		fn();
		fflush(0);
		_exit(77);
	}
	close(p[1]);
	while ((n = read(p[0], outbuf + total, sizeof outbuf - 1 - total)) > 0) total += n;
	outbuf[total] = 0;
	close(p[0]);
	waitpid(pid, &st, 0);
	return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static void child_help(void) { struct args a; run(&a, 0, 0, 1, "--help"); }
static void child_usage(void) { struct args a; run(&a, 0, 0, 1, "--usage"); }
static void child_version(void) { struct args a; run(&a, 0, 0, 1, "--version"); }
static void child_badopt(void) { struct args a; run(&a, 0, 0, 1, "--bogus"); }
static void child_shortbad(void) { struct args a; run(&a, 0, 0, 1, "-z"); }
static void child_missing(void) { struct args a; run(&a, 0, 0, 1, "-o"); }
static void child_toomany(void) { struct args a; run(&a, 0, 0, 1, "a"); }

static void test_argp(void)
{
	SECTION("argp");
	struct args a;
	int idx = -1;
	unsigned quiet = ARGP_NO_EXIT | ARGP_NO_ERRS;
	const char *prog = program_invocation_short_name;
	char *mem = 0;
	size_t memsz = 0;
	FILE *ms;
	int rc;

	argp_program_version = "prog 1.2.3";
	argp_program_bug_address = "bugs@example.org";

	/* basic options, clusters, attached/separate/= arguments, abbreviations */
	CHECK(run(&a, quiet, &idx, 7, "-v", "-o", "out.txt", "--count=5", "f1", "f2", "--noisy") == 0);
	CHECK(a.verbose == 2 && !strcmp(a.output, "out.txt") && a.count == 5);
	CHECK(a.nargs == 2 && !strcmp(a.args[0], "f1") && !strcmp(a.args[1], "f2"));
	CHECK(a.init == 1 && a.end == 1 && a.success == 1 && a.fini == 1 && a.error == 0);
	CHECK(a.noargs == 0);

	CHECK(run(&a, quiet, &idx, 3, "-vvo", "x", "-n9") == 0);
	CHECK(a.verbose == 2 && !strcmp(a.output, "x") && a.count == 9);
	CHECK(run(&a, quiet, &idx, 4, "--out=y", "--cou", "7", "--long") == 0);
	CHECK(!strcmp(a.output, "y") && a.count == 7 && a.longonly == 1);
	CHECK(a.noargs == 1);                          /* no non-option arguments */

	/* optional arguments attach only with = or directly */
	CHECK(run(&a, quiet, &idx, 3, "-p", "f1", "--opt=val") == 0);
	CHECK(!strcmp(a.opt, "val") && a.nargs == 1);
	CHECK(run(&a, quiet, &idx, 1, "-pdirect") == 0 && !strcmp(a.opt, "direct"));
	CHECK(run(&a, quiet, &idx, 1, "--opt") == 0 && !strcmp(a.opt, "(none)"));

	/* options after arguments are permuted to the front, like getopt */
	{
		char *av[] = { "prog", "f1", "-v", "f2", "-n", "3", "f3", 0 };
		memset(&a, 0, sizeof a);
		CHECK(argp_parse(&argp, 7, av, quiet, &idx, &a) == 0);
		CHECK(a.verbose == 1 && a.count == 3 && a.nargs == 3);
		CHECK(!strcmp(a.args[0], "f1") && !strcmp(a.args[2], "f3"));
		CHECK(!strcmp(av[1], "-v") && !strcmp(av[2], "-n") && !strcmp(av[3], "3"));
		CHECK(!strcmp(av[4], "f1") && !strcmp(av[6], "f3"));
		CHECK(idx == 7);
	}

	/* "--" ends option parsing; "-" alone is an argument */
	CHECK(run(&a, quiet, &idx, 4, "-v", "--", "-v", "-") == 0);
	CHECK(a.verbose == 1 && a.nargs == 2 && !strcmp(a.args[0], "-v") && !strcmp(a.args[1], "-"));

	/* child parser receives its own options and input */
	CHECK(run(&a, quiet, &idx, 2, "-c", "--child-flag") == 0 && a.child_flag == 2);

	/* hidden options still parse */
	CHECK(run(&a, quiet, &idx, 1, "-H") == 0 && a.hidden == 1);

	/* errors return non-zero and run the ERROR cleanup, not SUCCESS */
	CHECK(run(&a, quiet, &idx, 1, "--bogus") != 0 && a.error == 1 && a.success == 0 && a.fini == 1);
	CHECK(run(&a, quiet, &idx, 1, "-z") != 0);
	CHECK(run(&a, quiet, &idx, 1, "-o") != 0);                 /* missing argument */
	CHECK(run(&a, quiet, &idx, 1, "--verbose=1") != 0);        /* no argument allowed */
	CHECK(run(&a, quiet, &idx, 1, "--c") != 0);                /* ambiguous: count / child-flag */
	{
		char *av[] = { "prog", "x", 0 };
		struct args b;
		memset(&b, 0, sizeof b);
		b.require = 2;                                         /* parser rejects at KEY_END */
		CHECK(argp_parse(&argp, 2, av, quiet, 0, &b) == EINVAL);
	}

	/* too many arguments: parser refuses the 17th */
	{
		char *av[40];
		int i;
		av[0] = "prog";
		for (i = 1; i <= 17; i++) av[i] = "f";
		av[18] = 0;
		memset(&a, 0, sizeof a);
		CHECK(argp_parse(&argp, 18, av, quiet, 0, &a) != 0);
		memset(&a, 0, sizeof a);
		CHECK(argp_parse(&argp, 18, av, quiet, &idx, &a) == 0 && idx == 17);  /* stops there */
	}

	/* help text */
	ms = open_memstream(&mem, &memsz);
	argp_help(&argp, ms, ARGP_HELP_STD_HELP & ~ARGP_HELP_EXIT_OK, "prog");
	fclose(ms);
	CHECK(strstr(mem, "Usage: prog [OPTION...] FILE...\n") != 0);
	CHECK(strstr(mem, "Test program.\n") != 0);
	CHECK(strstr(mem, "  -v, --verbose, --noisy") != 0);
	CHECK(strstr(mem, "Produce verbose output") != 0);
	CHECK(strstr(mem, "  -o, --output=FILE") != 0 && strstr(mem, "Write output to FILE") != 0);
	CHECK(strstr(mem, "  -p, --opt[=VAL]") != 0);
	CHECK(strstr(mem, "      --longonly") != 0);
	CHECK(strstr(mem, " Child options:\n") != 0 && strstr(mem, "-c, --child-flag") != 0);
	CHECK(strstr(mem, "  -?, --help") != 0 && strstr(mem, "      --usage") != 0);
	CHECK(strstr(mem, "  -V, --version") != 0);
	CHECK(strstr(mem, "should not show") == 0 && strstr(mem, "--hidden") == 0);
	CHECK(strstr(mem, "After the options.\n") != 0);
	CHECK(strstr(mem, "Report bugs to bugs@example.org.\n") != 0);
	CHECK(strstr(mem, "Mandatory or optional arguments") != 0);
	{
		/* option documentation starts in a fixed column */
		char *line = strstr(mem, "  -v, --verbose, --noisy");
		char *eol = line ? strchr(line, '\n') : 0;
		CHECK(eol && strstr(line, "Produce verbose output") && (size_t)(strstr(line, "Produce") - line) == 29);
	}
	free(mem);

	ms = open_memstream(&mem, &memsz);
	argp_help(&argp, ms, ARGP_HELP_USAGE, "prog");
	fclose(ms);
	CHECK(strstr(mem, "Usage: prog [-vc?V]") != 0);
	CHECK(strstr(mem, "[-o FILE]") != 0 && strstr(mem, "[-n N]") != 0 && strstr(mem, "[-p[VAL]]") != 0);
	CHECK(strstr(mem, "[--output=FILE]") != 0 && strstr(mem, "[--opt[=VAL]]") != 0);
	CHECK(strstr(mem, "[--help]") != 0 && strstr(mem, "[--usage]") != 0 && strstr(mem, "[--version]") != 0);
	CHECK(strstr(mem, "FILE...") != 0);
	free(mem);

	/* built-in options through argp_parse: output and exit status */
	rc = capture(child_help);
	CHECK(rc == 0 && strstr(outbuf, "Usage: ") && strstr(outbuf, "--verbose") && strstr(outbuf, "bugs@example.org"));
	rc = capture(child_usage);
	CHECK(rc == 0 && strstr(outbuf, "Usage: ") && strstr(outbuf, "[--help]"));
	rc = capture(child_version);
	CHECK(rc == 0 && !strcmp(outbuf, "prog 1.2.3\n"));
	rc = capture(child_badopt);
	CHECK(rc == 64);
	CHECK(strstr(outbuf, prog) && strstr(outbuf, "unrecognized option '--bogus'") &&
	      strstr(outbuf, "Try `") && strstr(outbuf, "--help' or `") && strstr(outbuf, "--usage' for more information."));
	rc = capture(child_shortbad);
	CHECK(rc == 64 && strstr(outbuf, "invalid option -- 'z'"));
	rc = capture(child_missing);
	CHECK(rc == 64 && strstr(outbuf, "option requires an argument -- 'o'"));
	(void)child_toomany;

	/* argp_error / argp_failure honour the flags */
	{
		struct argp_state st;
		memset(&st, 0, sizeof st);
		st.name = "prog";
		st.err_stream = stderr;
		st.root_argp = &argp;
		st.flags = ARGP_NO_EXIT | ARGP_NO_ERRS;
		argp_error(&st, "silent %d", 1);               /* prints nothing, no exit */
		argp_failure(&st, 3, 0, "silent");             /* no exit with NO_EXIT */
		CHECK(1);
	}

	/* ARGP_NO_HELP removes the built-in options */
	{
		char *av[] = { "prog", "--help", 0 };
		memset(&a, 0, sizeof a);
		CHECK(argp_parse(&argp, 2, av, ARGP_SILENT, 0, &a) != 0);
	}
}

int main(void)
{
	test_mallinfo();
	test_fts();
	test_rb();
	test_splay();
	test_argp();
	fprintf(stderr, "\n%d checks, %d failures\n", checks, failures);
	return failures > 255 ? 255 : failures;
}
