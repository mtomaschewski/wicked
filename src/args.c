/*
 *	wicked command option/action argument processing utilities
 *
 *	Copyright (C) 2020-2021 SUSE LCC.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 *	Authors:
 *		Marius Tomaschewski
 *		Rubén Torrero Marijnissen
 *		Clemens Famulla-Conrad
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include <unistd.h>
#include <getopt.h>
#include <ctype.h>

#include <wicked/util.h>
#include <wicked/logging.h>

#include "args.h"


/*
 * We format help as 1) option/action, 2) argument/synopis and 3) documentation column:
 *
 * |123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_|
 * |<--- 80 characters ------------------------------------------------------------>|
 * |<--- 40 characters --------------------><--- 40 characters -------------------->|
 * |<--- 20 (name+1) ->_<--- 20 (args+1) ->_<--- 40 documentation ----------------->|
 * |__-X,_--opt-name___|<opt-arg-name>_____|documentation___________________________|
 * |______--opt-name___|<opt-arg-name>_____|documentation___________________________|
 * |__-X_______________|<opt-arg-name>_____|documentation___________________________|
 * |__<- 2+12 -->_<------ 26 (synopis)---->_<--- 40 documentation ----------------->|
 * |__action-name|<action synopis>_________|documentation___________________________|
 *
 * - 1st option/action column is indented by 2 spaces and space padded
 *       to it's max len plus a space (20 for option, 12 for action)
 * - 2nd argument column is space padded up to 20(option) 26(action)
 *
 *   When the width of colum 1 + 2 is longer than 39 characters,
 *   we break into the next line and space pad to 40 characters.
 *
 * - 3rd documentation column of 40 characters. when multiple
 *   lines are use, follow-up lines are indented by 40 spaces.
 */
#define NI_WICKED_HELP_COLUMN_WIDTH	40
#define NI_WICKED_HELP_OPTION_LEN	20
#define NI_WICKED_HELP_OPTION_INDENT	2
#define NI_WICKED_HELP_OPTION_SHORT_LEN	4
#define NI_WICKED_HELP_ACTION_LEN	14
#define NI_WICKED_HELP_ACTION_INDENT	2
#define NI_WICKED_HELP_USAGE_INDENT	2

#define NI_WICKED_HELP_USAGE_SECTION	"Usage"
#define NI_WICKED_HELP_USAGE_OPTIONS	"[option ..]"
#define NI_WICKED_HELP_USAGE_ACTIONS	"<action> .."
#define NI_WICKED_HELP_OPTIONS_SECTION	"Options"
#define NI_WICKED_HELP_ACTIONS_SECTION	"Actions"


int
ni_wicked_ctx_init(ni_wicked_ctx_t *ctx, const ni_wicked_ctx_t *caller, const char *name)
{
	ni_stringbuf_t buf = NI_STRINGBUF_INIT_DYNAMIC;
	const char *command;

	ni_assert(ctx && !ni_string_empty(name));

	memset(ctx, 0, sizeof(*ctx));
	ctx->caller = caller;
	ctx->name = name;

	command = ni_wicked_ctx_command(&buf, ctx);
	ni_string_dup(&ctx->command, command);
	ni_stringbuf_destroy(&buf);

	if (caller && caller->verbosity != NI_VERBOSITY_UNSET)
		ctx->verbosity = caller->verbosity;
	else
		ctx->verbosity = NI_VERBOSITY_UNSET;

	/* (re-)initialize global getopt option start index */
	optind = 1;

	return 0;
}

static inline void
ni_wicked_ctx_destroy_options(ni_wicked_ctx_t *ctx)
{
	if (ctx->opts_short)
		free(ctx->opts_short);
	if (ctx->opts_table)
		free(ctx->opts_table);
	ctx->opts_short = NULL;
	ctx->opts_table = NULL;
}

void
ni_wicked_ctx_destroy(ni_wicked_ctx_t *ctx)
{
	if (ctx) {
		ni_string_free(&ctx->command);
		ni_wicked_ctx_destroy_options(ctx);
	}
}

int
ni_wicked_ctx_set_options(ni_wicked_ctx_t *ctx, const ni_wicked_option_t *options)
{
	ni_stringbuf_t sbuf = NI_STRINGBUF_INIT_DYNAMIC;
	const ni_wicked_option_t *option;
	struct option *opts = NULL;
	size_t count = 0, n;
	int ret = -1;

	if (!ctx)
		return ret;

	ni_stringbuf_putc(&sbuf, '+');   /* be consistent -- always POSIXLY_CORRECT=1 */

	for (option = options; option; ++option) {
		if (!option->name)
			break;

		count++;

		if (!isalnum((unsigned char)option->value))
			continue;

		if (sbuf.string && strchr(sbuf.string, option->value))
			continue;

		ni_stringbuf_putc(&sbuf, option->value);
		switch (option->has_arg) {
			case required_argument:
				ni_stringbuf_putc(&sbuf, ':');
				break;
			case optional_argument:
				ni_stringbuf_putc(&sbuf, ':');
				ni_stringbuf_putc(&sbuf, ':');
				break;
			default:
				break;
		}
	}
	if (!sbuf.string)
		goto failure;

	if (count) {
		opts = calloc(count + 1, sizeof(struct option));
		if (!opts)
			goto failure;

		for (n = 0, option = options; n < count && option; ++n, ++option) {
			if (!option->name)
				break;
			opts[n].name = option->name;
			opts[n].has_arg = option->has_arg;
			opts[n].flag = option->ret_var;
			opts[n].val = option->value;
		}
	}

	ni_wicked_ctx_destroy_options(ctx);
	ctx->options = options;
	ctx->opts_table = opts;
	ni_string_dup(&ctx->opts_short, sbuf.string);
	ni_stringbuf_destroy(&sbuf);

	return 0;

failure:
	if (opts)
		free(opts);
	ni_stringbuf_destroy(&sbuf);
	return ret;
}

int
ni_wicked_ctx_set_actions(ni_wicked_ctx_t *ctx, const ni_wicked_action_t *actions)
{
	if (!ctx)
		return -EINVAL;

	ctx->actions = actions;
	return 0;
}

const char *
ni_wicked_ctx_command(ni_stringbuf_t *buf, const ni_wicked_ctx_t *ctx)
{
	if (!buf || !ctx)
		return NULL;

	if (ctx->command) {
		ni_stringbuf_puts(buf, ctx->command);
	} else {
		if (ctx->caller) {
			ni_wicked_ctx_command(buf, ctx->caller);
			ni_stringbuf_putc(buf, ' ');
		}
		if (ctx->name) {
			ni_stringbuf_puts(buf, ctx->name);
		}
	}
	return buf->string;
}

const ni_wicked_option_t *
ni_wicked_ctx_get_option(const ni_wicked_ctx_t *ctx, int opt)
{
	const ni_wicked_option_t *option;

	if (!ctx || opt == EOF)
		return NULL;

	for (option = ctx->options; option; ++option) {
		if (!option->name)
			break;

		if (opt == option->value)
			return option;
	}
	return NULL;
}

const ni_wicked_action_t *
ni_wicked_action_find(const ni_wicked_action_t *actions, const char *name)
{
	const ni_wicked_action_t *action;

	for (action = actions; action->name; ++action) {
		if (ni_string_eq(action->name, name))
			return action;
	}
	return NULL;
}

int
ni_wicked_action_exec(const ni_wicked_action_t *action,
			const ni_wicked_ctx_t *caller,
			int argc, char *argv[])
{
	ni_wicked_ctx_t ctx;
	int status;

	ni_assert(action && action->exec && argv);

	ni_wicked_ctx_init(&ctx, caller, action->name);
	status = action->exec(&ctx, argc, argv);
	ni_wicked_ctx_destroy(&ctx);
	return status;
}

int
ni_wicked_ctx_action_exec(const ni_wicked_ctx_t *ctx, int argc, char *argv[])
{
	const ni_wicked_action_t *action;

	ni_assert(ctx && argv);

	if (argc < 1 || ni_string_empty(argv[0])) {
		ni_wicked_ctx_hint_print(stderr, ctx, "missing action");
		return NI_WICKED_RC_USAGE;
	}

	action = ni_wicked_action_find(ctx->actions, argv[0]);
	if (!action) {
		ni_wicked_ctx_hint_print(stderr, ctx,
				"unrecognized action '%s'", argv[0]);
		return NI_WICKED_RC_NOT_IMPLEMENTED;
	}
	return ni_wicked_action_exec(action, ctx, argc, argv);
}

int
ni_wicked_ctx_getopt(ni_wicked_ctx_t *ctx, int argc, char *argv[], const ni_wicked_option_t **option)
{
	char *command;
	int opt = EOF;

	if (!ctx || !ctx->options || !ctx->opts_short || !ctx->opts_table)
		return opt;

	command = argv[0];
	argv[0] = ctx->command ? ctx->command : command;
	opt = getopt_long(argc, argv, ctx->opts_short, ctx->opts_table, NULL);
	argv[0] = command;
	if (option)
		*option = ni_wicked_ctx_get_option(ctx, opt);
	return opt;
}

size_t
ni_wicked_ctx_help_format_usage(ni_stringbuf_t *obuf, const ni_wicked_ctx_t *ctx, const char *section)
{
	size_t olen;

	if (!obuf || !ctx || !ctx->command)
		return 0;

	olen = obuf->len;

	if (section) {
		ni_stringbuf_printf(obuf, "%s:\n%-*s", section,
				NI_WICKED_HELP_USAGE_INDENT, "");
	}

	ni_stringbuf_printf(obuf, "%s", ctx->command);
	if (ctx->doc_args) {
		ni_stringbuf_printf(obuf, " %s", ctx->doc_args);
	} else {
		if (ctx->options)
			ni_stringbuf_printf(obuf, " %s", NI_WICKED_HELP_USAGE_OPTIONS);
		if (ctx->actions)
			ni_stringbuf_printf(obuf, " %s", NI_WICKED_HELP_USAGE_ACTIONS);
	}
	ni_stringbuf_putc(obuf, '\n');

	return obuf->len - olen;
}

size_t
ni_wicked_ctx_help_format_option(ni_stringbuf_t *obuf, const ni_wicked_option_t *option)
{
	ni_stringbuf_t buf = NI_STRINGBUF_INIT_DYNAMIC;
	size_t olen, nlen, slen, npad;
	const char *beg, *eol;

	if (!obuf || !option || !option->name || !option->doc_info)
		return 0;

	olen = obuf->len;
	slen = isalnum((unsigned char)option->value) ? 2 : 0;
	nlen = ni_string_len(option->name);

	/*
	 * 1st column: -<Short-Option> + ", "
	 */
	ni_stringbuf_printf(&buf, "%-*s", NI_WICKED_HELP_OPTION_INDENT, "");
	if (slen) {
		ni_stringbuf_printf(&buf, "-%c", option->value);
		if (nlen)
			ni_stringbuf_puts(&buf, ", ");
	} else {
		ni_stringbuf_printf(&buf, "%-*s", NI_WICKED_HELP_OPTION_SHORT_LEN, "");
	}

	/*
	 * 2st column: --<Long-Option name> [+ argument]
	 */
	if (nlen)
		ni_stringbuf_printf(&buf, "--%s", option->name);
	else
		ni_stringbuf_printf(&buf, "%-*s", NI_WICKED_HELP_OPTION_SHORT_LEN, "");

	switch (option->has_arg) {
		case required_argument:
			if (buf.len < NI_WICKED_HELP_OPTION_LEN) {
				npad = NI_WICKED_HELP_OPTION_LEN - buf.len - 1;
				ni_stringbuf_printf(&buf, "%-*s", npad, "");
			}
			ni_stringbuf_printf(&buf, " <%s>",
						option->doc_args ?
						option->doc_args : "...");
			break;
		case optional_argument:
			if (buf.len < NI_WICKED_HELP_OPTION_LEN) {
				npad = NI_WICKED_HELP_OPTION_LEN - buf.len - 1;
				ni_stringbuf_printf(&buf, "%-*s", npad, "");
			}
			ni_stringbuf_printf(&buf, " [=%s]",
						option->doc_args ?
						option->doc_args : "...");
			break;
		default:
			break;
	}

	/*
	 * 3rd column: Documentation
	 */
	if (!ni_string_empty(option->doc_info)) {
		/*
		 * pad the 1st + 2nd column with spaces and when
		 * longer than 39 + 1 space or break into next line
		 *
		 *   |-X, --too-long-option <and-also-argument>\n
		 *   |<--  pad 40           -->| doc[0] in next |
		 */
		if (buf.len < NI_WICKED_HELP_COLUMN_WIDTH)
			ni_stringbuf_printf(&buf,   "%-*s",
					NI_WICKED_HELP_COLUMN_WIDTH - buf.len, "");
		else
			ni_stringbuf_printf(&buf,   "\n%-*s",
					NI_WICKED_HELP_COLUMN_WIDTH, "");

		/*
		 * handle multiple lines in doc_info:
		 *   | -X, --opt arg  | doc[0] .................\n
		 *   |<--  pad 40  -->| doc[n]                  |
		 */
		beg = option->doc_info;
		while ((eol = strchr(beg, '\n'))) {
			ni_stringbuf_printf(&buf, "%.*s%-*s", eol - beg + 1,
					beg, NI_WICKED_HELP_COLUMN_WIDTH, "");
			beg = ++eol;
		}
		if (!ni_string_empty(beg))
			ni_stringbuf_puts(&buf, beg);
	}

	ni_stringbuf_put(obuf, buf.string, buf.len);
	ni_stringbuf_destroy(&buf);
	return obuf->len - olen;
}

size_t
ni_wicked_ctx_help_format_options(ni_stringbuf_t *obuf, const ni_wicked_ctx_t *ctx, const char *section)
{
	const ni_wicked_option_t *option;
	size_t olen;

	if (!obuf || !ctx || !ctx->options)
		return 0;

	olen = obuf->len;

	if (section) {
		ni_stringbuf_printf(obuf, "%s:\n", section);
	}

	for (option = ctx->options; option; ++option) {
		if (!option->name)
			break;

		if (ni_wicked_ctx_help_format_option(obuf, option))
			ni_stringbuf_putc(obuf, '\n');
	}

	return obuf->len - olen;
}

size_t
ni_wicked_ctx_help_format_action(ni_stringbuf_t *obuf, const ni_wicked_action_t *action)
{
	ni_stringbuf_t buf = NI_STRINGBUF_INIT_DYNAMIC;
	size_t olen, npad;
	const char *beg, *eol;

	if (!obuf || !action || ni_string_empty(action->name) || !action->doc_info)
		return 0;

	olen = obuf->len;

	/*
	 * 1st column: Action name (mandatory)
	 */
	ni_stringbuf_printf(&buf, "%-*s%s", NI_WICKED_HELP_ACTION_INDENT, "",
						action->name);

	/*
	 * 2nd column: Action usage/synopis
	 */
	if (!ni_string_empty(action->doc_args)) {
		if (buf.len < NI_WICKED_HELP_ACTION_LEN) {
			npad = NI_WICKED_HELP_ACTION_LEN - buf.len - 1;
			ni_stringbuf_printf(&buf, "%-*s", npad);
		}
		ni_stringbuf_printf(&buf, " %s", action->doc_args);
	}

	/*
	 * 3rd column: Documentation
	 */
	if (!ni_string_empty(action->doc_info)) {
		/*
		 * pad the 1st + 2nd column with spaces and when
		 * longer than 39 + space, break into next line.
		 *
		 *   |______too-long-action <and-also-argument>\n
		 *   |<--  pad 40           -->| doc[0] in next |
		 */
		if (buf.len < NI_WICKED_HELP_COLUMN_WIDTH)
			ni_stringbuf_printf(&buf,   "%-*s",
					NI_WICKED_HELP_COLUMN_WIDTH - buf.len, "");
		else
			ni_stringbuf_printf(&buf,   "\n%-*s",
					NI_WICKED_HELP_COLUMN_WIDTH, "");

		/*
		 * handle multiple lines in doc_info:
		 *   | -X, --opt arg  | doc[0] .................\n
		 *   |<--  pad 40  -->| doc[n]                  |
		 */
		beg = action->doc_info;
		while ((eol = strchr(beg, '\n'))) {
			ni_stringbuf_printf(&buf, "%.*s%-*s", eol - beg + 1,
					beg, NI_WICKED_HELP_COLUMN_WIDTH, "");
			beg = ++eol;
		}
		if (!ni_string_empty(beg))
			ni_stringbuf_puts(&buf, beg);
	}

	ni_stringbuf_put(obuf, buf.string, buf.len);
	ni_stringbuf_destroy(&buf);
	return obuf->len - olen;
}

size_t
ni_wicked_ctx_help_format_actions(ni_stringbuf_t *obuf, const ni_wicked_ctx_t *ctx, const char *section)
{
	const ni_wicked_action_t *action;
	size_t olen;

	if (!obuf || !ctx || !ctx->actions)
		return 0;

	olen = obuf->len;

	if (section) {
		ni_stringbuf_printf(obuf, "%s:\n", section);
	}

	for (action = ctx->actions; action; ++action) {
		if (!action->name)
			break;

		if (ni_wicked_ctx_help_format_action(obuf, action))
			ni_stringbuf_putc(obuf, '\n');
	}

	return obuf->len - olen;
}

size_t
ni_wicked_ctx_help_format(ni_stringbuf_t *obuf, const ni_wicked_ctx_t *ctx)
{
	size_t olen;

	if (!obuf || !ctx)
		return 0;

	olen = obuf->len;

	if (ctx->doc_info)
		ni_stringbuf_printf(obuf, "%s\n\n", ctx->doc_info);

	if (ni_wicked_ctx_help_format_usage(obuf, ctx, NI_WICKED_HELP_USAGE_SECTION))
		ni_stringbuf_putc(obuf, '\n');

	if (ni_wicked_ctx_help_format_options(obuf, ctx, NI_WICKED_HELP_OPTIONS_SECTION))
		ni_stringbuf_putc(obuf, '\n');

	if (ni_wicked_ctx_help_format_actions(obuf, ctx, NI_WICKED_HELP_ACTIONS_SECTION))
		ni_stringbuf_putc(obuf, '\n');

	return obuf->len - olen;
}

size_t
ni_wicked_ctx_hint_format(ni_stringbuf_t *obuf, const ni_wicked_ctx_t *ctx)
{
	size_t olen;

	if (!obuf || !ctx)
		return 0;

	olen = obuf->len;

	ni_stringbuf_printf(obuf, "Try '%s --help' for more information.", ctx->command);
	return obuf->len - olen;
}

size_t
ni_wicked_ctx_help_print(FILE *output, const ni_wicked_ctx_t *ctx)
{
	ni_stringbuf_t obuf = NI_STRINGBUF_INIT_DYNAMIC;
	size_t len;

	if (ni_wicked_ctx_help_format(&obuf, ctx) && obuf.string) {
		fputs(obuf.string, output);
		fflush(output);
	}

	len = obuf.len;
	ni_stringbuf_destroy(&obuf);
	return len;
}

size_t
ni_wicked_ctx_hint_print(FILE *output, const ni_wicked_ctx_t *ctx, const char *err, ...)
{
	ni_stringbuf_t obuf = NI_STRINGBUF_INIT_DYNAMIC;
	size_t len;

	if (err) {
		va_list ap;
		va_start(ap, err);
		ni_stringbuf_vprintf(&obuf, err, ap);
		va_end(ap);
	}

	/* getopt and NULL or a custom err were printed, insert new line */
	ni_stringbuf_putc(&obuf, '\n');

	if (ni_wicked_ctx_hint_format(&obuf, ctx) && obuf.string)
 		ni_stringbuf_putc(&obuf, '\n');

	fputs(obuf.string, output);
	fflush(output);

	len = obuf.len;
	ni_stringbuf_destroy(&obuf);
	return len;
}
