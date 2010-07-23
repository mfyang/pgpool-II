/* -*-pgsql-c-*- */
/*
 * $Header: /cvsroot/pgpool/pgpool-II/pool_worker_child.c,v 1.5 2010/07/23 06:22:34 t-ishii Exp $
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2010	PgPool Global Development Group
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of the
 * author not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. The author makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * child.c: worker child process main
 *
 */
#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <signal.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include "pool.h"
#include "pool_process_context.h"
#include "pool_session_context.h"
#include "pool_config.h"
#include "pool_ip.h"
#include "md5.h"
#include "pool_stream.h"

extern int myargc;
extern char **myargv;

char remote_ps_data[NI_MAXHOST];		/* used for set_ps_display */
static POOL_CONNECTION_POOL_SLOT	*slots[MAX_NUM_BACKENDS];
static volatile sig_atomic_t reload_config_request = 0;

static void establish_persistent_connection(void);
static void check_replication_time_lag(void);
static long text_to_lsn(char *text);
static RETSIGTYPE my_signal_handler(int sig);
static RETSIGTYPE reload_config_handler(int sig);
static void reload_config(void);

#define CHECK_REQUEST \
	do { \
		if (reload_config_request) \
		{ \
			reload_config(); \
			reload_config_request = 0; \
		} \
    } while (0)

/*
* worker child main loop
*/
void do_worker_child(void)
{
	pool_debug("I am %d", getpid());

	/* Identify myself via ps */
	init_ps_display("", "", "", "");
	set_ps_display("worker process", false);

	/* set up signal handlers */
	signal(SIGALRM, SIG_DFL);
	signal(SIGTERM, my_signal_handler);
	signal(SIGINT, my_signal_handler);
	signal(SIGHUP, reload_config_handler);
	signal(SIGQUIT, my_signal_handler);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	/* Initialize per process context */
	pool_init_process_context();

	for (;;)
	{
		CHECK_REQUEST;

		if (pool_config->health_check_period <= 0)
		{
			sleep(30);
		}

		/*
		 * If streaming replication mode, do time lag checking
		 */

		if (MASTER_SLAVE && !strcmp(pool_config->master_slave_sub_mode, MODE_STREAMREP))
		{
			/* Check and establish persistent connections to the backend */
			establish_persistent_connection();

			/* Do replication time lag checking */
			check_replication_time_lag();
		}
		sleep(pool_config->health_check_period);
	}
	exit(0);
}

/*
 * Establish persistent connection to backend
 */
static void establish_persistent_connection(void)
{
	int i;
	BackendInfo *bkinfo;
	POOL_CONNECTION_POOL_SLOT *s;

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (!VALID_BACKEND(i))
			continue;

		if (slots[i] == NULL)
		{
			bkinfo = pool_get_node_info(i);
			s = make_persistent_db_connection(bkinfo->backend_hostname, 
											  bkinfo->backend_port,
											  "postgres",
											  pool_config->health_check_user,
											  "");
			if (s)
				slots[i] = s;
			else
				slots[i] = NULL;
		}
	}
}

/*
 * Check replicaton time lag
 */
static void check_replication_time_lag(void)
{
	int i;
	int active_nodes = 0;
	POOL_STATUS sts;
	POOL_SELECT_RESULT *res;
	unsigned long long int lsn[MAX_NUM_BACKENDS];
	char *query;
	BackendInfo *bkinfo;
	unsigned long long int lag;

	if (NUM_BACKENDS <= 1)
	{
		/* If there's only one node, there's no point to do checking */
		return;
	}

	/* Count healthy nodes */
	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i))
			active_nodes++;
	}

	if (active_nodes <= 1)
	{
		/* If there's only one or less active node, there's no point
		 * to do checking */
		return;
	}

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (!VALID_BACKEND(i))
			continue;

		if (!slots[i])
		{
			pool_error("check_replication_time_lag: DB node is valid but no persistent connection");
			return;
		}

		if (REAL_MASTER_NODE_ID == i)
		{
			query = "SELECT pg_current_xlog_location()";
		}
		else
		{
			query = "SELECT pg_last_xlog_receive_location()";
		}

		sts = do_query(slots[i]->con, query, &res, PROTO_MAJOR_V3);
		if (sts != POOL_CONTINUE)
		{
			pool_error("check_replication_time_lag: %s failed", query);
			slots[i] = NULL;
			return;
		}
		if (!res)
		{
			pool_error("check_replication_time_lag: %s result is null", query);
			return;
		}
		if (res->numrows <= 0)
		{
			pool_error("check_replication_time_lag: %s returns no rows", query);
			free_select_result(res);
			return;
		}
		if (res->data[0] == NULL)
		{
			pool_error("check_replication_time_lag: %s returns no data", query);
			free_select_result(res);
			return;
		}

		if (res->nullflags[0] == -1)
		{
			pool_log("check_replication_time_lag: %s returns NULL", query);
			free_select_result(res);
			lsn[i] = 0;
		}
		else
		{
			lsn[i] = text_to_lsn(res->data[0]);
			free_select_result(res);
		}
	}

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (!VALID_BACKEND(i))
			continue;

		/* Set standby delay value */
		bkinfo = pool_get_node_info(i);
		lag = lsn[REAL_MASTER_NODE_ID] - lsn[i];

		if (REAL_MASTER_NODE_ID == i)
		{
			bkinfo->standby_delay = 0;
		}
		else
		{
			bkinfo->standby_delay = lag;

			/* Log delay if necessary */
			if (!strcmp(pool_config->log_standby_delay, "always") ||
				(pool_config->delay_threshold &&
				 !strcmp(pool_config->log_standby_delay, "if_over_threshold") &&
				 lag > pool_config->delay_threshold))
			{
				pool_log("Replication of node:%d is behind %lld bytes from the primary server (node:%d)", i, lsn[REAL_MASTER_NODE_ID] - lsn[i], REAL_MASTER_NODE_ID);
			}
		}
	}
}

/*
 * Convert logid/recoff style text to 64bit log location (LSN)
 */
static long text_to_lsn(char *text)
{
	unsigned int xlogid;
	unsigned int xrecoff;
	unsigned long long int lsn;

	if (sscanf(text, "%X/%X", &xlogid, &xrecoff) != 2)
	{
		pool_error("text_to_lsn: wrong log location format: %s", text);
		return 0;
	}
	lsn = xlogid * 16 * 1024 * 1024 * 255 + xrecoff;
	return lsn;
}

static RETSIGTYPE my_signal_handler(int sig)
{
	POOL_SETMASK(&BlockSig);

	switch (sig)
	{
		case SIGTERM:
		case SIGINT:
		case SIGQUIT:
			exit(0);
		default:
			exit(1);
			break;
	}

	POOL_SETMASK(&UnBlockSig);
}

static RETSIGTYPE reload_config_handler(int sig)
{
	POOL_SETMASK(&BlockSig);
	reload_config_request = 1;
	POOL_SETMASK(&UnBlockSig);
}

static void reload_config(void)
{
	pool_log("reload config files.");
	pool_get_config(get_config_file_name(), RELOAD_CONFIG);
	if (pool_config->enable_pool_hba)
		load_hba(get_hba_file_name());
	reload_config_request = 0;
}
