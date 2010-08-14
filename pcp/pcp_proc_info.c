/*
 * $Header: /cvsroot/pgpool/pgpool-II/pcp/pcp_proc_info.c,v 1.9 2010/08/14 02:05:26 gleu Exp $
 *
 * pgpool: a language independent connection pool server for PostgreSQL 
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2008	PgPool Global Development Group
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
 * Client program to send "process info" command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "pcp.h"

static void usage(void);
static void myexit(ErrorCode e);

int
main(int argc, char **argv)
{
	long timeout;
	char host[MAX_DB_HOST_NAMELEN];
	int port;
	char user[MAX_USER_PASSWD_LEN];
	char pass[MAX_USER_PASSWD_LEN];
	int processID;
	ProcessInfo *process_info;
	int array_size;
	int ch;
	int	optindex;
    bool verbose = false;

	static struct option long_options[] = {
		{"debug", no_argument, NULL, 'd'},
		{"help", no_argument, NULL, 'h'},
		{"verbose", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};
	
    while ((ch = getopt_long(argc, argv, "hdv", long_options, &optindex)) != -1) {
		switch (ch) {
		case 'd':
			pcp_enable_debug();
			break;

		case 'v':
			verbose = true;
			break;

		case 'h':
		case '?':
		default:
			usage();
			exit(0);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 6)
	{
		errorcode = INVALERR;
		pcp_errorstr(errorcode);
		myexit(errorcode);
	}

	timeout = atol(argv[0]);
	if (timeout < 0) {
		errorcode = INVALERR;
		pcp_errorstr(errorcode);
		myexit(errorcode);
	}

	if (strlen(argv[1]) >= MAX_DB_HOST_NAMELEN)
	{
		errorcode = INVALERR;
		pcp_errorstr(errorcode);
		myexit(errorcode);
	}
	strcpy(host, argv[1]);

	port = atoi(argv[2]);
	if (port <= 1024 || port > 65535)
	{
		errorcode = INVALERR;
		pcp_errorstr(errorcode);
		myexit(errorcode);
	}

	if (strlen(argv[3]) >= MAX_USER_PASSWD_LEN)
	{
		errorcode = INVALERR;
		pcp_errorstr(errorcode);
		myexit(errorcode);
	}
	strcpy(user, argv[3]);

	if (strlen(argv[4]) >= MAX_USER_PASSWD_LEN)
	{
		errorcode = INVALERR;
		pcp_errorstr(errorcode);
		myexit(errorcode);
	}
	strcpy(pass, argv[4]);

	processID = atoi(argv[5]);
	if (processID < 0)
	{
		errorcode = INVALERR;
		pcp_errorstr(errorcode);
		myexit(errorcode);
	}

	pcp_set_timeout(timeout);

	if (pcp_connect(host, port, user, pass))
	{
		pcp_errorstr(errorcode);
		myexit(errorcode);
	}

	if ((process_info = pcp_process_info(processID, &array_size)) == NULL)
	{
		pcp_errorstr(errorcode);
		pcp_disconnect();
		myexit(errorcode);
	} else {
		int i;
		for (i = 0; i < array_size; i++)
		{
			if (process_info->connection_info[i].database[0] == '\0')
				continue;
			
            if (verbose)
            {
		    	printf("Database     : %s\nUsername     : %s\nStart time   : %ld\nCreation time: %ld\nMajor        : %d\nMinor        : %d\nCounter      : %d\nPID          : %d\nConnected    : %d\n",
		    		   process_info->connection_info[i].database,
		    		   process_info->connection_info[i].user,
		    		   process_info->start_time,
		    		   process_info->connection_info[i].create_time,
		    		   process_info->connection_info[i].major,
		    		   process_info->connection_info[i].minor,
		    		   process_info->connection_info[i].counter,
		    		   process_info->connection_info[i].pid,
		    		   process_info->connection_info[i].connected);
            } else {
		    	printf("%s %s %ld %ld %d %d %d %d %d\n",
		    		   process_info->connection_info[i].database,
		    		   process_info->connection_info[i].user,
		    		   process_info->start_time,
		    		   process_info->connection_info[i].create_time,
		    		   process_info->connection_info[i].major,
		    		   process_info->connection_info[i].minor,
		    		   process_info->connection_info[i].counter,
		    		   process_info->connection_info[i].pid,
		    		   process_info->connection_info[i].connected);
		    }
        }
		free(process_info);
	}

	pcp_disconnect();

	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "pcp_proc_info - display a pgpool-II child process' information\n\n");
	fprintf(stderr, "Usage: pcp_proc_info [-d] timeout hostname port# username password PID\n");
	fprintf(stderr, "Usage: pcp_proc_info -h\n\n");
	fprintf(stderr, "  -d, --debug : enable debug message (optional)\n");
	fprintf(stderr, "  timeout     : connection timeout value in seconds. command exits on timeout\n");
	fprintf(stderr, "  hostname    : pgpool-II hostname\n");
	fprintf(stderr, "  port#       : PCP port number\n");
	fprintf(stderr, "  username    : username for PCP authentication\n");
	fprintf(stderr, "  password    : password for PCP authentication\n");
	fprintf(stderr, "  PID         : PID of a child process to get information for\n");
	fprintf(stderr, "  -h, --help  : print this help\n");
}

static void
myexit(ErrorCode e)
{
	if (e == INVALERR)
	{
		usage();
		exit(e);
	}

	exit(e);
}
