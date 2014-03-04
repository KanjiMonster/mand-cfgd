/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <syslog.h>
#include <getopt.h>

#define USE_DEBUG

#include <event.h>

#include <mand/logx.h>

#ifdef HAVE_TALLOC_TALLOC_H
# include <talloc/talloc.h>
#else
# include <talloc.h>
#endif

#include "cfgd.h"
#include "comm.h"

static const char _ident[] = "cfgd v" VERSION;
static const char _build[] = "build on " __DATE__ " " __TIME__ " with gcc " __VERSION__;

struct event_base *ev_base;

void set_value(char *path, const char *str)
{
	char *s;

	fprintf(stderr, "Parameter \"%s\" changed to \"%s\"\n", path, str);

	if (strncmp(path, "system.ntp.", 11) == 0) {
		long id = strtol(path + 11, &s, 10);

		if (s && strncmp(s, ".udp.address", 12) == 0) {
			fprintf(stderr, "exec: uci set system.ntp.@server[%ld]=%s\n", id - 1, str);
		}
	}
}

static void sig_usr1(int fd, short event, void *arg)
{
}

static void sig_usr2(int fd, short event, void *arg)
{
	logx_level = logx_level == LOG_DEBUG ? LOG_INFO : LOG_DEBUG;
}

static void sig_pipe(int fd, short event, void *arg)
{
	logx(LOG_DEBUG, "sig_pipe");
}

static void usage(void)
{
	printf("cfgd, Version: .....\n\n"
	       "Usage: cfg [OPTION...]\n\n"
	       "Options:\n\n"
	       "  -h                        this help\n"
	       "  -l, --log=IP              write log to syslog at this IP\n"
	       "  -x                        debug logging\n\n");

	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	const struct rlimit rlim = {
		.rlim_cur = RLIM_INFINITY,
		.rlim_max = RLIM_INFINITY
	};

	struct event signal_usr1;
	struct event signal_usr2;
	struct event signal_pipe;

	int c;

	/* unlimited size for cores */
	setrlimit(RLIMIT_CORE, &rlim);

	logx_level = LOG_INFO;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"log",       1, 0, 'l'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "hl:x",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			usage();
			break;

		case 'l': {
			struct in_addr addr;

			if (inet_aton(optarg, &addr) == 0) {
				fprintf(stderr, "Invalid IP address: '%s'\n", optarg);
				exit(EXIT_FAILURE);
			} else
				logx_remote(addr);
			break;
		}

		case 'x':
			logx_level = LOG_DEBUG;
			break;

		default:
			printf("?? getopt returned character code 0%o ??\n", c);
		}
	}

	logx_open(basename(argv[0]), 0, LOG_DAEMON);

        ev_base = event_init();
        if (!ev_base)
		return 1;

        signal_set(&signal_usr1, SIGUSR1, sig_usr1, &signal_usr1);
        signal_add(&signal_usr1, NULL);
        signal_set(&signal_usr2, SIGUSR2, sig_usr2, &signal_usr2);
        signal_add(&signal_usr2, NULL);
        signal_set(&signal_pipe, SIGPIPE, sig_pipe, &signal_pipe);
        signal_add(&signal_pipe, NULL);

	init_comm(ev_base);

	logx(LOG_NOTICE, "startup %s %s (pid %d)\n", _ident, _build, getpid());

        event_base_loop(ev_base, 0);

        return 0;
}