#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <signal.h>

#include <wicked/logging.h>
#include <wicked/netinfo.h>
#include <wicked/socket.h>
#include <wicked/util.h>
#include "process.h"

static int
callit(const char *binary, int *rv)
{
	ni_string_array_t argv = NI_STRING_ARRAY_INIT;
	ni_shellcmd_t *cmd = NULL;
	ni_process_t *pi = NULL;

	ni_string_array_append(&argv, binary);
	if (!(cmd = ni_shellcmd_new(&argv))) {
		ni_string_array_destroy(&argv);
		return -1;
	}
	ni_string_array_destroy(&argv);

	if (!(pi = ni_process_new(cmd))) {
		ni_shellcmd_release(cmd);
		return -1;
	}
	ni_shellcmd_release(cmd);

	if (rv)
		*rv = ni_process_run_and_wait(pi);
	else
		ni_process_run_and_wait(pi);

	ni_process_free(pi);
	return 0;
}

int main(int argc, char **argv)
{
	const char *program;
	int n, rv;

	if (argc <= 1) {
		printf("Usage: %s <./script1 ...>\n", argv[0]);
		return 2;
	}

        program = ni_basename(argv[0]);
        ni_enable_debug("all");
        ni_log_level_set("debug2");

        if (ni_init(program) < 0)
                return 1;

	for (n = 1; n < argc; ++n) {
		if (callit(argv[n], &rv) < 0) {
			fprintf(stderr, "failed to execute '%s'\n", argv[n]);
			break;
		} else {
			fprintf(stdout,  "executed %s => %d\n", argv[n], rv);
		}
	}
	return 0;
}


