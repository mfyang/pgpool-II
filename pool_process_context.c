/* -*-pgsql-c-*- */
/*
 *
 * $Header: /cvsroot/pgpool/pgpool-II/pool_process_context.c,v 1.1 2010/06/02 06:52:34 t-ishii Exp $
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
 */

#include "pool.h"
#include "pool_process_context.h"
#include "pool_config.h"		/* remove me afterwards */

static POOL_PROCESS_CONTEXT process_context_d;
static POOL_PROCESS_CONTEXT *process_context;

/*
 * Initialize per process context
 */
void pool_init_process_context(void)
{
	process_context = &process_context_d;

	if (!process_info)
	{
		pool_error("pool_init_process_context: process_info is not set");
		child_exit(1);
	}
	process_context->process_info = process_info;

	if (!pool_config->backend_desc)
	{
		pool_error("pool_init_process_context: backend_desc is not set");
		child_exit(1);
	}
	process_context->backend_desc = pool_config->backend_desc;
}

/*
 * Return process context
 */
POOL_PROCESS_CONTEXT *pool_get_process_context(void)
{
	if (!process_context)
	{
		pool_error("pool_get_process_context: Process context is not initialized");
	}
	return process_context;
}

/*
 * Return my process info
 */
ProcessInfo *pool_get_my_process_info(void)
{
	if (!process_context)
	{
		pool_error("pool_get_my_process_info: Process context is not initialized");
		return NULL;
	}

	return &process_context->process_info[process_context->proc_id];
}

