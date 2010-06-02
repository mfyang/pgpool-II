/* -*-pgsql-c-*- */
/*
 *
 * $Header: /cvsroot/pgpool/pgpool-II/pool_process_context.h,v 1.2 2010/06/02 08:51:19 t-ishii Exp $
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
 * pool_process_context.h.: process context information
 *
 */

#ifndef POOL_PROCESS_CONTEXT_H
#define POOL_PROCESS_CONTEXT_H
#include "pool.h"

/*
 * Child process context:
 * Manages per pgpool child process context
 */
typedef struct {
	/*
	 * process start time, info on connection to backend etc.
	 */
	ProcessInfo	*process_info;
	int proc_id;  /* Index to process table(ProcessInfo) (!= UNIX's PID) */

	/*
	 * PostgreSQL server description. Placed on shared memory.
	 * Includes backend up/down info, hostname, data directory etc.
	 */
	BackendDesc *backend_desc;

	int local_session_id;	/* local session id */

} POOL_PROCESS_CONTEXT;

extern void pool_init_process_context(void);
extern POOL_PROCESS_CONTEXT *pool_get_process_context(void);
extern ProcessInfo *pool_get_my_process_info(void);
extern void pool_incremnet_local_session_id(void);

#endif /* POOL_PROCESS_CONTEXT_H */
