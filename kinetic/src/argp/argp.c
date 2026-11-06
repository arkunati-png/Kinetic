#define _GNU_SOURCE
#include <argp.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GNU-style argument parsing (argp).
 *
 * Original implementation of the documented interface: option tables,
 * parser callbacks with the ARGP_KEY_* protocol, child parsers, and the
 * automatic --help / --usage / --version options. Help text follows the
 * familiar GNU layout but is not guaranteed to match it byte for byte.
 */

const char *argp_program_version;
void (*argp_program_version_hook)(FILE *, struct argp_state *);
const char *argp_program_bug_address;
error_t argp_err_exit_status = 64; /* EX_USAGE */

#define KEY_USAGE (-100)
#define DOC_COL 29
#define RMARGIN 79

static const struct argp_option help_options[] = {
	{ "help", '?', 0, 0, "Give this help list", -1 },
	{ "usage", KEY_USAGE, 0, 0, "Give a short usage message", 0 },
	{ "version", 'V', 0, 0, "Print program version", -1 },
	{ 0 }
};

static int is_end(const struct argp_option *o)
{
	return !o->key && !o->name && !o->doc && !o->group;
}

static int has_version(void)
{
	return argp_program_version || argp_program_version_hook;
}

static int printable_key(int k)
{
	return k > 0 && k < 127 && isprint(k);
}

/* ---- option table with alias resolution -------------------------------- */

struct opt {
	const struct argp_option *o;
	const struct argp_option *real; /* closest non-alias: key/arg/flags */
	struct parser *p;               /* owner, NULL for built-in help options */
};

struct parser {
	const struct argp *argp;
	struct parser *parent;
	int child_idx;
	void **child_inputs;
	void *hook;
	unsigned args_processed;
	struct argp_state st;
};

struct ctx {
	struct parser *ps;
	int np;
	struct opt *opts;
	int nopts;
	struct argp_state master;
	void *input;
};

static int count_parsers(const struct argp *a)
{
	int n = 1, i;
	for (i = 0; a->children && a->children[i].argp; i++)
		n += count_parsers(a->children[i].argp);
	return n;
}

static int count_opts(const struct argp *a)
{
	int n = 0, i;
	const struct argp_option *o;
	for (o = a->options; o && !is_end(o); o++) n++;
	for (i = 0; a->children && a->children[i].argp; i++)
		n += count_opts(a->children[i].argp);
	return n;
}

static void flatten(struct ctx *c, const struct argp *a, struct parser *parent, int idx)
{
	struct parser *p = &c->ps[c->np++];
	int i, nch = 0;

	p->argp = a;
	p->parent = parent;
	p->child_idx = idx;
	while (a->children && a->children[nch].argp) nch++;
	if (nch) p->child_inputs = calloc(nch, sizeof(void *));
	for (i = 0; i < nch; i++) flatten(c, a->children[i].argp, p, i);
}

static void add_opts(struct ctx *c, const struct argp_option *tab, struct parser *p)
{
	const struct argp_option *o, *real = 0;
	for (o = tab; o && !is_end(o); o++) {
		if (!(o->flags & OPTION_ALIAS)) real = o;
		if (o->flags & OPTION_DOC) continue;
		if (!real || !real->key) continue;
		if (!o->name && !printable_key(real->key)) continue;
		if (!o->name && (o->flags & OPTION_ALIAS)) continue;
		c->opts[c->nopts].o = o;
		c->opts[c->nopts].real = real;
		c->opts[c->nopts].p = p;
		c->nopts++;
	}
}

/* ---- state helpers ----------------------------------------------------- */

static error_t call(struct ctx *c, struct parser *p, int key, char *arg)
{
	struct argp_state *st = &p->st, *m = &c->master;
	error_t e;

	if (!p->argp->parser) return ARGP_ERR_UNKNOWN;
	st->next = m->next;
	st->quoted = m->quoted;
	st->arg_num = p->args_processed;
	st->input = p->parent ? p->parent->child_inputs[p->child_idx] : c->input;
	st->child_inputs = p->child_inputs;
	e = p->argp->parser(key, arg, st);
	m->next = st->next;
	if (key == ARGP_KEY_ARG && e != ARGP_ERR_UNKNOWN) p->args_processed++;
	return e;
}

/* ---- help output ------------------------------------------------------- */

/* Write text wrapped at RMARGIN. `col` is the current column; continuation
 * lines are indented to `indent`. Returns the final column. */
static int put_wrapped(FILE *f, const char *s, int col, int indent)
{
	while (*s) {
		const char *w = s;
		int len;
		if (*s == '\n') {
			fputc('\n', f);
			col = 0;
			s++;
			continue;
		}
		if (*s == ' ') {
			s++;
			if (col > indent) {
				const char *e = s;
				while (*e && *e != ' ' && *e != '\n') e++;
				if (col + 1 + (e - s) <= RMARGIN) {
					fputc(' ', f);
					col++;
				} else {
					fputc('\n', f);
					col = 0;
				}
			}
			continue;
		}
		while (*s && *s != ' ' && *s != '\n') s++;
		len = s - w;
		while (col < indent) {
			fputc(' ', f);
			col++;
		}
		fwrite(w, 1, len, f);
		col += len;
	}
	return col;
}

static char *filtered(const struct argp *a, int key, const char *text, void *input)
{
	if (a->help_filter) return a->help_filter(key, text, input);
	return (char *)text;
}

static void print_see(FILE *f, const char *name)
{
	fprintf(f, "Try `%s --help' or `%s --usage' for more information.\n", name, name);
}

static void print_short_usage(FILE *f, const struct argp *a, const char *name)
{
	const char *doc = a->args_doc;
	const char *nl;
	int first = 1;

	if (!doc) {
		fprintf(f, "Usage: %s [OPTION...]\n", name);
		return;
	}
	do {
		nl = strchr(doc, '\n');
		fprintf(f, first ? "Usage: %s [OPTION...] " : "  or: %s [OPTION...] ", name);
		if (nl) fwrite(doc, 1, nl - doc, f);
		else fputs(doc, f);
		fputc('\n', f);
		first = 0;
		doc = nl ? nl + 1 : 0;
	} while (doc && *doc);
}

static int usage_emit(FILE *f, int col, int indent, const char *tok)
{
	int len = strlen(tok);
	if (col + 1 + len > RMARGIN) {
		int i;
		fputc('\n', f);
		for (i = 0; i < indent; i++) fputc(' ', f);
		col = indent;
	} else {
		fputc(' ', f);
		col++;
	}
	fputs(tok, f);
	return col + len;
}

static void usage_walk(const struct argp *a, FILE *f, int *col, int indent,
	char *shorts, int *nshorts, int pass)
{
	const struct argp_option *o, *real = 0;
	int i;
	char tok[256];

	for (o = a->options; o && !is_end(o); o++) {
		const struct argp_option *r;
		if (!(o->flags & OPTION_ALIAS)) real = o;
		r = real;
		if (!r || (o->flags & (OPTION_DOC|OPTION_HIDDEN|OPTION_NO_USAGE))) continue;
		if (pass == 0) {
			if (!(o->flags & OPTION_ALIAS) && printable_key(r->key) && !r->arg)
				shorts[(*nshorts)++] = r->key;
		} else if (pass == 1) {
			if (!(o->flags & OPTION_ALIAS) && printable_key(r->key) && r->arg) {
				if (r->flags & OPTION_ARG_OPTIONAL)
					snprintf(tok, sizeof tok, "[-%c[%s]]", r->key, r->arg);
				else
					snprintf(tok, sizeof tok, "[-%c %s]", r->key, r->arg);
				*col = usage_emit(f, *col, indent, tok);
			}
		} else if (o->name) {
			if (!r->arg)
				snprintf(tok, sizeof tok, "[--%s]", o->name);
			else if (r->flags & OPTION_ARG_OPTIONAL)
				snprintf(tok, sizeof tok, "[--%s[=%s]]", o->name, r->arg);
			else
				snprintf(tok, sizeof tok, "[--%s=%s]", o->name, r->arg);
			*col = usage_emit(f, *col, indent, tok);
		}
	}
	for (i = 0; a->children && a->children[i].argp; i++)
		usage_walk(a->children[i].argp, f, col, indent, shorts, nshorts, pass);
}

static void print_full_usage(FILE *f, const struct argp *a, const char *name, int help)
{
	char shorts[512];
	int nshorts = 0, col, indent, i;
	char tok[600];
	struct argp helpargp = { help_options, 0, 0, 0, 0, 0, 0 };
	struct argp_option vopts[4];

	/* the built-in options take part in the usage line unless disabled */
	memcpy(vopts, help_options, sizeof vopts);
	if (!has_version()) { vopts[2] = vopts[3]; }
	helpargp.options = vopts;

	col = fprintf(f, "Usage: %s", name);
	indent = col + 1;

	usage_walk(a, f, &col, indent, shorts, &nshorts, 0);
	if (help) usage_walk(&helpargp, f, &col, indent, shorts, &nshorts, 0);
	if (nshorts) {
		tok[0] = '[';
		tok[1] = '-';
		for (i = 0; i < nshorts; i++) tok[2 + i] = shorts[i];
		tok[2 + nshorts] = ']';
		tok[3 + nshorts] = 0;
		col = usage_emit(f, col, indent, tok);
	}
	usage_walk(a, f, &col, indent, shorts, &nshorts, 1);
	usage_walk(a, f, &col, indent, shorts, &nshorts, 2);
	if (help) usage_walk(&helpargp, f, &col, indent, shorts, &nshorts, 2);
	if (a->args_doc) {
		const char *nl = strchr(a->args_doc, '\n');
		int len = nl ? nl - a->args_doc : (int)strlen(a->args_doc);
		char *copy = strndup(a->args_doc, len);
		if (copy) {
			col = usage_emit(f, col, indent, copy);
			free(copy);
		}
	}
	fputc('\n', f);
}

static int needs_dup_note(const struct argp *a)
{
	const struct argp_option *o;
	int i;
	for (o = a->options; o && !is_end(o); o++)
		if (!(o->flags & (OPTION_DOC|OPTION_HIDDEN|OPTION_ALIAS)) && o->arg &&
		    o->name && printable_key(o->key)) return 1;
	for (i = 0; a->children && a->children[i].argp; i++)
		if (needs_dup_note(a->children[i].argp)) return 1;
	return 0;
}

static void print_options(FILE *f, const struct argp *a, void *input, int *any)
{
	const struct argp_option *o;
	int i;

	for (o = a->options; o && !is_end(o); o++) {
		char left[256];
		int n = 0, col;
		const struct argp_option *al;
		const char *doc;
		int last_named = 0;

		if (o->flags & OPTION_ALIAS) continue; /* merged into its target below */
		if (o->flags & OPTION_HIDDEN) continue;
		if (!o->name && !o->key && o->doc) {
			/* heading */
			fprintf(f, "\n %s\n", o->doc);
			*any = 1;
			continue;
		}
		if (o->flags & OPTION_DOC) {
			if (!o->name) continue;
			n = snprintf(left, sizeof left, "  %s", o->name);
		} else {
			int have_short = printable_key(o->key);
			n = snprintf(left, sizeof left, "  ");
			if (have_short) n += snprintf(left + n, sizeof left - n, "-%c", o->key);
			else n += snprintf(left + n, sizeof left - n, "  ");
			if (o->name) {
				n += snprintf(left + n, sizeof left - n,
					have_short ? ", --%s" : "  --%s", o->name);
				last_named = 1;
			}
			for (al = o + 1; !is_end(al) && (al->flags & OPTION_ALIAS); al++) {
				if (al->flags & OPTION_HIDDEN) continue;
				if (al->name) {
					n += snprintf(left + n, sizeof left - n, ", --%s", al->name);
					last_named = 1;
				}
			}
			if (o->arg) {
				int opt = o->flags & OPTION_ARG_OPTIONAL;
				if (last_named)
					n += snprintf(left + n, sizeof left - n, opt ? "[=%s]" : "=%s", o->arg);
				else
					n += snprintf(left + n, sizeof left - n, opt ? "[%s]" : " %s", o->arg);
			}
			if (!have_short && !o->name) continue;
		}
		fputs(left, f);
		col = n;
		doc = o->doc ? filtered(a, o->key, o->doc, input) : 0;
		if (doc && *doc) {
			if (col >= DOC_COL - 1) {
				fputc('\n', f);
				col = 0;
			}
			col = put_wrapped(f, doc, col, DOC_COL);
		}
		fputc('\n', f);
		*any = 1;
	}
	for (i = 0; a->children && a->children[i].argp; i++) {
		const struct argp_child *ch = &a->children[i];
		if (ch->header) fprintf(f, "\n %s\n", ch->header);
		print_options(f, ch->argp, input, any);
	}
}

static void print_help_options(FILE *f, int sflags)
{
	const struct argp_option *o;
	for (o = help_options; !is_end(o); o++) {
		char left[128];
		int n;
		if (o->key == 'V' && !has_version()) continue;
		(void)sflags;
		if (printable_key(o->key))
			n = snprintf(left, sizeof left, "  -%c, --%s", o->key, o->name);
		else
			n = snprintf(left, sizeof left, "      --%s", o->name);
		fputs(left, f);
		while (n++ < DOC_COL) fputc(' ', f);
		fputs(o->doc, f);
		fputc('\n', f);
	}
}

/* Split doc into the part before and after a vertical tab. */
static void split_doc(const char *doc, char **pre, char **post)
{
	const char *vt = doc ? strchr(doc, '\v') : 0;
	*pre = *post = 0;
	if (!doc) return;
	if (vt) {
		*pre = strndup(doc, vt - doc);
		*post = strdup(vt + 1);
	} else {
		*pre = strdup(doc);
	}
}

static void help_impl(const struct argp *a, FILE *f, unsigned flags,
	const char *name, unsigned sflags, void *input)
{
	int help = !(sflags & ARGP_NO_HELP);
	char *pre, *post;
	int any = 0;

	if (flags & ARGP_HELP_USAGE) print_full_usage(f, a, name, help);
	if (flags & ARGP_HELP_SHORT_USAGE) print_short_usage(f, a, name);

	if (flags & ARGP_HELP_LONG) {
		if (flags & ARGP_HELP_PRE_DOC) {
			split_doc(a->doc, &pre, &post);
			if (pre && *pre) {
				char *t = filtered(a, ARGP_KEY_HELP_PRE_DOC, pre, input);
				put_wrapped(f, t, 0, 0);
				fputc('\n', f);
			}
			free(pre);
			free(post);
		}
		fputc('\n', f);
		print_options(f, a, input, &any);
		if (help) {
			if (any) fputc('\n', f);
			print_help_options(f, sflags);
		}
		if (needs_dup_note(a)) {
			fputs("\nMandatory or optional arguments to long options are also mandatory or optional\n"
			      "for any corresponding short options.\n", f);
		}
	}

	if ((flags & ARGP_HELP_POST_DOC) && (flags & ARGP_HELP_LONG)) {
		split_doc(a->doc, &pre, &post);
		if (post && *post) {
			char *t = filtered(a, ARGP_KEY_HELP_POST_DOC, post, input);
			fputc('\n', f);
			put_wrapped(f, t, 0, 0);
			fputc('\n', f);
		}
		free(pre);
		free(post);
	}

	if ((flags & ARGP_HELP_BUG_ADDR) && argp_program_bug_address)
		fprintf(f, "\nReport bugs to %s.\n", argp_program_bug_address);

	if (flags & ARGP_HELP_SEE) print_see(f, name);
}

void argp_help(const struct argp *a, FILE *f, unsigned flags, char *name)
{
	if (!name) name = program_invocation_short_name;
	help_impl(a, f, flags, name, 0, 0);
	fflush(f);
	if (flags & ARGP_HELP_EXIT_ERR) exit(argp_err_exit_status);
	if (flags & ARGP_HELP_EXIT_OK) exit(0);
}

void argp_state_help(const struct argp_state *s, FILE *f, unsigned flags)
{
	const char *name = program_invocation_short_name;
	unsigned sflags = 0;
	void *input = 0;

	if (s) {
		if ((s->flags & ARGP_NO_ERRS) && f == s->err_stream) goto done;
		name = s->name ? s->name : name;
		sflags = s->flags;
		input = s->input;
	}
	help_impl(s ? s->root_argp : 0, f, flags & ~(unsigned)(ARGP_HELP_EXIT_ERR|ARGP_HELP_EXIT_OK),
		name, sflags, input);
	fflush(f);
done:
	if (!s || !(s->flags & ARGP_NO_EXIT)) {
		if (flags & ARGP_HELP_EXIT_ERR) exit(argp_err_exit_status);
		if (flags & ARGP_HELP_EXIT_OK) exit(0);
	}
}

void argp_usage(const struct argp_state *s)
{
	argp_state_help(s, s->err_stream, ARGP_HELP_STD_USAGE);
}

void argp_error(const struct argp_state *s, const char *fmt, ...)
{
	va_list ap;
	if (!(s->flags & ARGP_NO_ERRS)) {
		FILE *f = s->err_stream;
		flockfile(f);
		fprintf(f, "%s: ", s->name);
		va_start(ap, fmt);
		vfprintf(f, fmt, ap);
		va_end(ap);
		fputc('\n', f);
		funlockfile(f);
		argp_state_help(s, f, ARGP_HELP_STD_ERR);
	}
}

void argp_failure(const struct argp_state *s, int status, int errnum,
	const char *fmt, ...)
{
	va_list ap;
	if (!s || !(s->flags & ARGP_NO_ERRS)) {
		FILE *f = s ? s->err_stream : stderr;
		fflush(stdout);
		flockfile(f);
		fprintf(f, "%s: ", s && s->name ? s->name : program_invocation_short_name);
		if (fmt) {
			va_start(ap, fmt);
			vfprintf(f, fmt, ap);
			va_end(ap);
			if (errnum) fputs(": ", f);
		}
		if (errnum) fputs(strerror(errnum), f);
		fputc('\n', f);
		funlockfile(f);
		fflush(f);
	}
	if (status && (!s || !(s->flags & ARGP_NO_EXIT))) exit(status);
}

/* ---- parsing ----------------------------------------------------------- */

/* Report a malformed command line. Prints the message and standard help
 * (which exits unless ARGP_NO_EXIT) and records the failure. */
static void syntax_error(struct ctx *c, int *failed, const char *fmt, const char *a)
{
	*failed = 1;
	argp_error(&c->master, fmt, a);
}

static void run_builtin(struct ctx *c, int key)
{
	struct argp_state *m = &c->master;
	if (key == '?') {
		argp_state_help(m, m->out_stream, ARGP_HELP_STD_HELP);
	} else if (key == KEY_USAGE) {
		argp_state_help(m, m->out_stream, ARGP_HELP_USAGE | ARGP_HELP_EXIT_OK);
	} else {
		if (argp_program_version_hook) argp_program_version_hook(m->out_stream, m);
		else if (argp_program_version) fprintf(m->out_stream, "%s\n", argp_program_version);
		fflush(m->out_stream);
		if (!(m->flags & ARGP_NO_EXIT)) exit(0);
	}
}

/* Result of an option lookup: a user option, or one of the built-in help
 * options (identified by its key). */
struct found {
	const struct opt *op;
	int builtin;
	int ambiguous;
};

static struct found find_short(struct ctx *c, int ch)
{
	struct found f = { 0, 0, 0 };
	int i;

	for (i = 0; i < c->nopts; i++) {
		if (printable_key(c->opts[i].real->key) && c->opts[i].real->key == ch) {
			f.op = &c->opts[i];
			return f;
		}
	}
	if (!(c->master.flags & ARGP_NO_HELP)) {
		if (ch == '?') f.builtin = '?';
		else if (ch == 'V' && has_version()) f.builtin = 'V';
	}
	return f;
}

/* Exact name, or a unique prefix. Prefixes that name the same option (e.g.
 * an option and its alias) are not ambiguous. */
static struct found find_long(struct ctx *c, const char *name, size_t len)
{
	struct found f = { 0, 0, 0 };
	const struct argp_option *o;
	int i, bkey = 0;

	for (i = 0; i < c->nopts; i++) {
		const char *n = c->opts[i].o->name;
		if (!n || strncmp(n, name, len)) continue;
		if (!n[len]) {
			f.op = &c->opts[i];
			f.builtin = 0;
			f.ambiguous = 0;
			return f;
		}
		if (f.op && f.op->real->key != c->opts[i].real->key) f.ambiguous = 1;
		else if (!f.op) f.op = &c->opts[i];
	}
	if (!(c->master.flags & ARGP_NO_HELP)) {
		for (o = help_options; !is_end(o); o++) {
			if (o->key == 'V' && !has_version()) continue;
			if (strncmp(o->name, name, len)) continue;
			if (!o->name[len]) {
				f.op = 0;
				f.builtin = o->key;
				f.ambiguous = 0;
				return f;
			}
			if (f.op || bkey) f.ambiguous = 1;
			else bkey = o->key;
		}
	}
	if (f.ambiguous) return f;
	if (!f.op && bkey) f.builtin = bkey;
	return f;
}

static int arg_need(const struct found *f)
{
	if (f->builtin || !f->op->real->arg) return 0;
	return (f->op->real->flags & OPTION_ARG_OPTIONAL) ? 2 : 1;
}

static error_t dispatch(struct ctx *c, const struct found *f, char *arg)
{
	if (f->builtin) {
		run_builtin(c, f->builtin);
		return 0;
	}
	return call(c, f->op->p, f->op->real->key, arg);
}

/* Offer one non-option argument to every parser until one accepts it. */
static error_t give_arg(struct ctx *c, char *a)
{
	int i;
	for (i = 0; i < c->np; i++) {
		error_t e = call(c, &c->ps[i], ARGP_KEY_ARG, a);
		if (e != ARGP_ERR_UNKNOWN) return e;
	}
	return ARGP_ERR_UNKNOWN;
}

error_t argp_parse(const struct argp *argp, int argc, char **argv,
	unsigned flags, int *arg_index, void *input)
{
	struct ctx c;
	error_t err = 0;
	int i, start, nargs = 0, failed = 0, quoted = 0, stopped = 0;
	char **orig = 0;
	char *isopt = 0;

	memset(&c, 0, sizeof c);
	c.input = input;
	c.ps = calloc(count_parsers(argp), sizeof *c.ps);
	c.opts = calloc(count_opts(argp) + 1, sizeof *c.opts);
	isopt = calloc(argc + 1, 1);
	orig = calloc(argc + 1, sizeof *orig);
	if (!c.ps || !c.opts || !isopt || !orig) {
		free(c.ps); free(c.opts); free(isopt); free(orig);
		return ENOMEM;
	}
	memcpy(orig, argv, argc * sizeof *orig);

	flatten(&c, argp, 0, 0);
	c.nopts = 0;
	for (i = 0; i < c.np; i++) add_opts(&c, c.ps[i].argp->options, &c.ps[i]);

	c.master.root_argp = argp;
	c.master.argc = argc;
	c.master.argv = argv;
	c.master.flags = flags;
	c.master.err_stream = stderr;
	c.master.out_stream = stdout;
	c.master.name = program_invocation_short_name;
	c.master.pstate = &c;
	start = (flags & ARGP_PARSE_ARGV0) ? 0 : 1;
	if (start > argc) start = argc;
	c.master.next = start;
	for (i = 0; i < c.np; i++) {
		c.ps[i].st = c.master;
		c.ps[i].st.hook = 0;
	}

	for (i = 0; i < c.np && !err; i++) {
		error_t e = call(&c, &c.ps[i], ARGP_KEY_INIT, 0);
		if (e && e != ARGP_ERR_UNKNOWN) err = e;
	}

	/* Pass 1: options. Non-option arguments are either handled in order
	 * (ARGP_IN_ORDER) or left in place and handled after all options. */
	for (i = start; i < argc && !err && !failed && !stopped; i++) {
		char *a = argv[i];

		if (quoted || a[0] != '-' || !a[1]) {
			nargs++;
			if (flags & ARGP_IN_ORDER) {
				c.master.next = i + 1;
				err = (flags & ARGP_NO_ARGS) ? ARGP_ERR_UNKNOWN : give_arg(&c, a);
				if (err == ARGP_ERR_UNKNOWN) {
					if (arg_index) {
						c.master.next = i;
						stopped = 1;
						err = 0;
					} else {
						syntax_error(&c, &failed, "Too many arguments", "");
						err = 0;
					}
				}
			}
			continue;
		}

		isopt[i] = 1;
		if (a[1] == '-' && !a[2]) {
			quoted = 1;
			continue;
		}

		if (a[1] == '-') {
			char *name = a + 2, *eq = strchr(name, '=');
			size_t len = eq ? (size_t)(eq - name) : strlen(name);
			char *arg = eq ? eq + 1 : 0;
			char shown[256];
			struct found f;
			int need;

			snprintf(shown, sizeof shown, "%.*s", (int)len, name);
			f = find_long(&c, name, len);
			if (f.ambiguous) { syntax_error(&c, &failed, "option '--%s' is ambiguous", shown); break; }
			if (!f.op && !f.builtin) { syntax_error(&c, &failed, "unrecognized option '--%s'", shown); break; }
			need = arg_need(&f);
			if (!need && arg) { syntax_error(&c, &failed, "option '--%s' doesn't allow an argument", shown); break; }
			if (need == 1 && !arg) {
				if (i + 1 < argc) {
					arg = argv[++i];
					isopt[i] = 1;
				} else {
					syntax_error(&c, &failed, "option '--%s' requires an argument", shown);
					break;
				}
			}
			c.master.next = i + 1;
			err = dispatch(&c, &f, arg);
			if (err == ARGP_ERR_UNKNOWN) {
				syntax_error(&c, &failed, "unrecognized option '--%s'", shown);
				err = 0;
			}
			continue;
		}

		/* cluster of short options: -abc, -ofile, -o file */
		{
			char *s;
			for (s = a + 1; *s && !err && !failed; s++) {
				char shown[2] = { *s, 0 };
				struct found f = find_short(&c, (unsigned char)*s);
				char *arg = 0;
				int need;

				if (!f.op && !f.builtin) { syntax_error(&c, &failed, "invalid option -- '%s'", shown); break; }
				need = arg_need(&f);
				if (need) {
					if (s[1]) {
						arg = s + 1;
					} else if (need == 1) {
						if (i + 1 < argc) {
							arg = argv[++i];
							isopt[i] = 1;
						} else {
							syntax_error(&c, &failed, "option requires an argument -- '%s'", shown);
							break;
						}
					}
				}
				c.master.next = i + 1;
				err = dispatch(&c, &f, arg);
				if (err == ARGP_ERR_UNKNOWN) {
					syntax_error(&c, &failed, "invalid option -- '%s'", shown);
					err = 0;
					break;
				}
				if (need) break; /* the rest of the cluster was the argument */
			}
		}
	}

	/* Like getopt, move options to the front and non-options after them. */
	if (!(flags & ARGP_IN_ORDER)) {
		int k = start;
		for (i = start; i < argc; i++) if (isopt[i]) argv[k++] = orig[i];
		c.master.next = k;
		for (i = start; i < argc; i++) if (!isopt[i]) argv[k++] = orig[i];
	}

	if (failed && !err) err = EINVAL;

	/* Pass 2: non-option arguments (unless already handled in order). */
	if (!err && !(flags & ARGP_IN_ORDER) && nargs > 0) {
		int first = c.master.next, handled = 0;

		if (!(flags & ARGP_NO_ARGS)) {
			for (i = 0; i < c.np && !handled && !err; i++) {
				error_t e;
				c.master.next = first;
				e = call(&c, &c.ps[i], ARGP_KEY_ARGS, argv[first]);
				if (!e) { handled = 1; c.master.next = argc; }
				else if (e != ARGP_ERR_UNKNOWN) err = e;
			}
		}
		if (!handled && !err) {
			for (i = first; i < argc; i++) {
				error_t e;
				c.master.next = i + 1;
				e = (flags & ARGP_NO_ARGS) ? ARGP_ERR_UNKNOWN : give_arg(&c, argv[i]);
				if (e == ARGP_ERR_UNKNOWN) {
					if (arg_index) {
						c.master.next = i;
					} else {
						syntax_error(&c, &failed, "Too many arguments", "");
						err = EINVAL;
					}
					break;
				}
				if (e) { err = e; break; }
			}
		}
	}

	/* Finalization: only if everything was consumed. */
	if (!err && !stopped && c.master.next >= argc) {
		if (nargs == 0)
			for (i = 0; i < c.np && !err; i++) {
				error_t e = call(&c, &c.ps[i], ARGP_KEY_NO_ARGS, 0);
				if (e && e != ARGP_ERR_UNKNOWN) err = e;
			}
		for (i = c.np - 1; i >= 0 && !err; i--) {
			error_t e = call(&c, &c.ps[i], ARGP_KEY_END, 0);
			if (e && e != ARGP_ERR_UNKNOWN) err = e;
		}
	}

	if (arg_index) *arg_index = c.master.next;

	for (i = c.np - 1; i >= 0; i--)
		call(&c, &c.ps[i], err ? ARGP_KEY_ERROR : ARGP_KEY_SUCCESS, 0);
	for (i = 0; i < c.np; i++)
		call(&c, &c.ps[i], ARGP_KEY_FINI, 0);

	for (i = 0; i < c.np; i++) free(c.ps[i].child_inputs);
	free(c.ps);
	free(c.opts);
	free(isopt);
	free(orig);
	return err;
}
