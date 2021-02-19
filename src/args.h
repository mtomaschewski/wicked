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
#ifndef   WICKED_CLIENT_ARGS_H
#define   WICKED_CLIENT_ARGS_H

#include <getopt.h>


typedef struct ni_wicked_ctx		ni_wicked_ctx_t;
typedef struct ni_wicked_action 	ni_wicked_action_t;
typedef int				ni_wicked_action_exec_fn_t(ni_wicked_ctx_t *,
							int argc, char *argv[]);
typedef struct ni_wicked_option		ni_wicked_option_t;

typedef enum {
	NI_VERBOSITY_QUIET,
	NI_VERBOSITY_BRIEF,
	NI_VERBOSITY_EVIDENT,
	NI_VERBOSITY_VERBOSE,
	NI_VERBOSITY_UNSET = -1U
} ni_wicked_verbosity_t;


#define	NI_WICKED_OPT_SHORT(c)		c
#define NI_WICKED_OPT_NUMBER(n)		(0x100 + n)


#define NI_WICKED_COMMON_OPT_HELP	NI_WICKED_OPT_SHORT('h')

#define NI_WICKED_COMMON_OPTION_HELP {				\
	"help",	no_argument, NULL, NI_WICKED_COMMON_OPT_HELP,	\
		NULL,	"Show this help text and exit."		\
}

#define NI_WICKED_COMMON_OPTIONS				\
	NI_WICKED_COMMON_OPTION_HELP


struct ni_wicked_option {
	/*
	 * Note: wicked is parsing options POSIXLY_CORRECT,
	 * thus don't use optional_argument (GNU extension)!
	 *
	 * Enrolled getopt_long struct option:
	 */
	const char *			name;		/* long option name         */
	int				has_arg;	/* no/required(optional) arg*/
	int *				ret_var;	/* NULL/variable set to var */
	int				value;		/* short option or numeric  */

	const char *			doc_args;	/* option argument name     */
	const char *			doc_info;	/* option list description  */
};

struct ni_wicked_action {
	const char *			name;		/* action name              */
	ni_wicked_action_exec_fn_t *	exec;		/* action "main" function   */

	const char *			doc_args;	/* action argument synopis  */
	const char *			doc_info;	/* action list description  */
};

struct ni_wicked_ctx {
	const char *			name;		/* current action name      */
	const ni_wicked_ctx_t *		caller;		/* action caller context    */

	const ni_wicked_option_t *	options;	/* current action options   */
	const ni_wicked_action_t *	actions;	/* child actions            */

	const char *			doc_args;	/* action argument synopis  */
	const char *			doc_info;	/* head action description  */

	char *				command;	/* complete command actions */
	ni_wicked_verbosity_t		verbosity;	/* std. output verbosity    */

	char *				opts_short;	/* short getopt options     */
	struct option *			opts_table;	/* long getopt option table */
};

extern int				ni_wicked_ctx_init(ni_wicked_ctx_t *ctx,
							const ni_wicked_ctx_t *caller,
							const char *name);
extern void				ni_wicked_ctx_destroy(ni_wicked_ctx_t *ctx);

extern int				ni_wicked_ctx_set_options(ni_wicked_ctx_t *ctx,
							const ni_wicked_option_t *options);
extern int				ni_wicked_ctx_set_actions(ni_wicked_ctx_t *ctx,
							const ni_wicked_action_t *actions);

extern const ni_wicked_option_t *	ni_wicked_ctx_get_option(const ni_wicked_ctx_t *ctx,
							int opt);

extern const char *			ni_wicked_ctx_command(ni_stringbuf_t *buf,
							const ni_wicked_ctx_t *ctx);

extern int				ni_wicked_ctx_getopt(ni_wicked_ctx_t *ctx,
							int argc, char *argv[],
							const ni_wicked_option_t **);

extern int				ni_wicked_ctx_action_exec(const ni_wicked_ctx_t *ctx,
							int argc, char *argv[]);

extern void				ni_wicked_ctx_hint_print(FILE *output,
							const ni_wicked_ctx_t *ctx,
							const char *err, ...);
extern void				ni_wicked_ctx_help_print(FILE *output,
							const ni_wicked_ctx_t *ctx);

extern const ni_wicked_action_t *	ni_wicked_action_find(const ni_wicked_action_t *list,
							const char *name);
extern int				ni_wicked_action_exec(const ni_wicked_action_t *action,
							const ni_wicked_ctx_t *caller,
							int argc, char *argv[]);

#endif /* WICKED_CLIENT_ARGS_H */
