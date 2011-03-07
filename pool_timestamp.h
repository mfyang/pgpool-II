/* -*-pgsql-c-*- */
/*
 *
 * $Header: /cvsroot/pgpool/pgpool-II/pool_timestamp.h,v 1.3 2011/03/07 05:56:58 kitagawa Exp $
 *
 * pgpool: a language independent connection pool server for PostgreSQL
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2011  PgPool Global Development Group
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
 * pool_timestamp.h.: header file for pool_timestamp.c,
 * pool_process_query.c, pool_proto_modules.c
 *
 */

#ifndef POOL_TIMESTAMP_H
#define POOL_TIMESTAMP_H
#include "pool.h"
#include "pool_proto_modules.h"
#include "parser/nodes.h"
#include "pool_session_context.h"

char *rewrite_timestamp(POOL_CONNECTION_POOL *backend, Node *node, bool rewrite_to_params, POOL_SENT_MESSAGE *message);
char *bind_rewrite_timestamp(POOL_CONNECTION_POOL *backend, POOL_SENT_MESSAGE *message, const char *orig_msg, int *len);

#endif /* POOL_TIMESTAMP_H */
