/* -*-pgsql-c-*- */
/*
 *
 * $Header: /cvsroot/pgpool/pgpool-II/pool_session_context.c,v 1.26 2011/03/07 05:56:57 kitagawa Exp $
 *
 * pgpool: a language independent connection pool server for PostgreSQL 
 * written by Tatsuo Ishii
 *
 * Copyright (c) 2003-2011	PgPool Global Development Group
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "pool.h"
#include "pool_config.h"
#include "pool_session_context.h"

static POOL_SESSION_CONTEXT session_context_d;
static POOL_SESSION_CONTEXT *session_context = NULL;

static void init_sent_message_list(void);
static bool can_query_context_destroy(POOL_QUERY_CONTEXT *qc);

/*
 * Initialize per session context
 */
void pool_init_session_context(POOL_CONNECTION *frontend, POOL_CONNECTION_POOL *backend)
{
	session_context = &session_context_d;

	/* Get Process context */
	session_context->process_context = pool_get_process_context();
	if (!session_context->process_context)
	{
		pool_error("pool_init_session_context: cannot get process context");
		return;
	}

	/* Set connection info */
	session_context->frontend = frontend;
	session_context->backend = backend;

	/* Initialize query context */
	session_context->query_context = NULL;

	/* Initialize local session id */
	pool_incremnet_local_session_id();

	/* Initialize sent message list */
	init_sent_message_list();

	/* Create memory context */
	session_context->memory_context = pool_memory_create(PREPARE_BLOCK_SIZE);

	/* Choose load balancing node if neccessary */
	if (pool_config->load_balance_mode)
	{
		ProcessInfo *process_info = pool_get_my_process_info();
		if (!process_info)
		{
			pool_error("pool_init_session_context: pool_get_my_process_info failed");
			return;
		}

		session_context->load_balance_node_id = 
			process_info->connection_info->load_balancing_node =
			select_load_balancing_node();

		pool_debug("selected load balancing node: %d", backend->info->load_balancing_node);
	}

	/* Unset query is in progress */
	pool_unset_query_in_progress();

	/* The command in progress has not succeeded yet */
	pool_unset_command_success();

	/* We don't have a write query in this transaction yet */
	pool_unset_writing_transaction();

	/* Error doesn't occur in this transaction yet */
	pool_unset_failed_transaction();

	/* Forget transaction isolation mode */
	pool_unset_transaction_isolation();

	/* We don't skip reading from backends */
	pool_unset_skip_reading_from_backends();

	/* Backends have not ignored messages yet */
	pool_unset_ignore_till_sync();

	/* Initialize where to send map for PREPARE statemets */
#ifdef NOT_USED
	memset(&session_context->prep_where, 0, sizeof(session_context->prep_where));
	session_context->prep_where.nelem = POOL_MAX_PREPARED_STATEMENTS;
#endif /* NOT_USED */
	/* Reset flag to indicate difference in number of affected tuples
	 * in UPDATE/DELETE.
	 */
	session_context->mismatch_ntuples = false;
}

/*
 * Destroy session context.
 */
void pool_session_context_destroy(void)
{
	if (session_context)
	{
		pool_clear_sent_message_list();
		free(session_context->message_list.sent_messages);
		pool_memory_delete(session_context->memory_context, 0);
	}
	/* XXX For now, just zap memory */
	memset(&session_context_d, 0, sizeof(session_context_d));
	session_context = NULL;
}

/*
 * Return session context
 */
POOL_SESSION_CONTEXT *pool_get_session_context(void)
{
	if (!session_context)
	{
		return NULL;
	}

	return session_context;
}

/*
 * Return local session id
 */
int pool_get_local_session_id(void)
{
	if (!session_context)
	{
		pool_error("pool_get_local_session_id: session context is not initialized");
		return -1;
	}

	return session_context->process_context->local_session_id;
}

/*
 * Return true if query is in progress
 */
bool pool_is_query_in_progress(void)
{
	if (!session_context)
	{
		pool_error("pool_is_query_in_progress: session context is not initialized");
		return false;
	}

	return session_context->in_progress;
}

/*
 * Set query is in progress
 */
void pool_set_query_in_progress(void)
{
	if (!session_context)
	{
		pool_error("pool_set_query_in_progress: session context is not initialized");
		return;
	}

	pool_debug("pool_set_query_in_progress: done");

	session_context->in_progress = true;
}

/*
 * Unset query is in progress
 */
void pool_unset_query_in_progress(void)
{
	if (!session_context)
	{
		pool_error("pool_unset_query_in_progress: session context is not initialized");
		return;
	}

	pool_debug("pool_unset_query_in_progress: done");

	session_context->in_progress = false;
}

/*
 * Return true if we skip reading from backends
 */
bool pool_is_skip_reading_from_backends(void)
{
	if (!session_context)
	{
		pool_error("pool_is_skip_reading_from_backends: session context is not initialized");
		return false;
	}

	return session_context->skip_reading_from_backends;
}

/*
 * Set skip_reading_from_backends
 */
void pool_set_skip_reading_from_backends(void)
{
	if (!session_context)
	{
		pool_error("pool_set_skip_reading_from_backends: session context is not initialized");
		return;
	}

	pool_debug("pool_set_skip_reading_from_backends: done");

	session_context->skip_reading_from_backends = true;
}

/*
 * Unset skip_reading_from_backends
 */
void pool_unset_skip_reading_from_backends(void)
{
	if (!session_context)
	{
		pool_error("pool_unset_skip_reading_from_backends: session context is not initialized");
		return;
	}

	pool_debug("pool_unset_skip_reading_from_backends: done");

	session_context->skip_reading_from_backends = false;
}

/*
 * Return true if we are doing extended query message
 */
bool pool_is_doing_extended_query_message(void)
{
	if (!session_context)
	{
		pool_error("pool_is_doing_extended_query_message: session context is not initialized");
		return false;
	}

	return session_context->doing_extended_query_message;
}

/*
 * Set doing_extended_query_message
 */
void pool_set_doing_extended_query_message(void)
{
	if (!session_context)
	{
		pool_error("pool_set_doing_extended_query_message: session context is not initialized");
		return;
	}

	pool_debug("pool_set_doing_extended_query_message: done");

	session_context->doing_extended_query_message = true;
}

/*
 * Unset doing_extended_query_message
 */
void pool_unset_doing_extended_query_message(void)
{
	if (!session_context)
	{
		pool_error("pool_unset_doing_extended_query_message: session context is not initialized");
		return;
	}

	pool_debug("pool_unset_doing_extended_query_message: done");

	session_context->doing_extended_query_message = false;
}

/*
 * Return true if backends ignore extended query message
 */
bool pool_is_ignore_till_sync(void)
{
	if (!session_context)
	{
		pool_error("pool_is_ignore_till_sync: session context is not initialized");
		return false;
	}

	return session_context->ignore_till_sync;
}

/*
 * Set ignore_till_sync
 */
void pool_set_ignore_till_sync(void)
{
	if (!session_context)
	{
		pool_error("pool_set_ignore_till_sync: session context is not initialized");
		return;
	}

	pool_debug("pool_set_ignore_till_sync: done");

	session_context->ignore_till_sync = true;
}

/*
 * Unset ignore_till_sync
 */
void pool_unset_ignore_till_sync(void)
{
	if (!session_context)
	{
		pool_error("pool_unset_ignore_till_sync: session context is not initialized");
		return;
	}

	pool_debug("pool_unset_ignore_till_sync: done");

	session_context->ignore_till_sync = false;
}

/*
 * Remove a sent message
 */
bool pool_remove_sent_message(char kind, const char *name)
{
	int i;
	POOL_SENT_MESSAGE_LIST *msglist;

	if (!session_context)
	{
		pool_error("pool_remove_sent_message: session context is not initialized");
		return false;
	}

	msglist = &session_context->message_list;

	for (i = 0; i < msglist->size; i++)
	{
		if (msglist->sent_messages[i]->kind == kind &&
			!strcmp(msglist->sent_messages[i]->name, name))
		{
			pool_sent_message_destroy(msglist->sent_messages[i]);
			break;
		}
	}

	/* sent message not found */
	if (i == msglist->size)
		return false;

	if (i != msglist->size - 1)
	{
		memmove(&msglist->sent_messages[i], &msglist->sent_messages[i+1],
				sizeof(POOL_SENT_MESSAGE *) * (msglist->size - i - 1));
	}

	msglist->size--;

	return true;
}

/*
 * Remove same kind of sent messages
 */
void pool_remove_sent_messages(char kind)
{
	int i;
	POOL_SENT_MESSAGE_LIST *msglist;

	if (!session_context)
	{
		pool_error("pool_remove_sent_messages: session context is not initialized");
		return;
	}

	msglist = &session_context->message_list;

	for (i = 0; i < msglist->size; i++)
	{
		if (msglist->sent_messages[i]->kind == kind)
		{
			if (pool_remove_sent_message(kind, msglist->sent_messages[i]->name))
				i--;	/* for relocation by removing */
		}
	}
}

/*
 * Destroy sent message
 */
void pool_sent_message_destroy(POOL_SENT_MESSAGE *message)
{
	bool in_progress;
	POOL_QUERY_CONTEXT *qc = NULL;

	if (!session_context)
	{
		pool_error("pool_sent_message_destroy: session context is not initialized");
		return;
	}

	in_progress = pool_is_query_in_progress();

	if (message)
	{
		if (message->contents)
			pool_memory_free(session_context->memory_context, message->contents);
		
		if (message->name)
			pool_memory_free(session_context->memory_context, message->name);

		if (message->query_context)
		{
			if (session_context->query_context != message->query_context)
				qc = session_context->query_context;

			if (can_query_context_destroy(message->query_context))
			{
				pool_query_context_destroy(message->query_context);
				/*
				 * set in_progress flag, because pool_query_context_destroy()
				 * unsets in_progress flag
				 */
				if (in_progress)
					pool_set_query_in_progress();
				/*
				 * set query_context of session_context, because
				 * pool_query_context_destroy() sets it to NULL.
				 */
				if (qc)
					session_context->query_context = qc;
			}
		}

		if (session_context->memory_context)
			pool_memory_free(session_context->memory_context, message);
	}
}

/*
 * Clear sent message list
 */
void pool_clear_sent_message_list(void)
{
	POOL_SENT_MESSAGE_LIST *msglist;

	if (!session_context)
	{
		pool_error("pool_clear_sent_message_list: session context is not initialized");
		return;
	}

	msglist = &session_context->message_list;

	while (msglist->size > 0)
	{
		pool_remove_sent_messages(msglist->sent_messages[0]->kind);
	}
}

/*
 * Create a sent message
 * kind: one of 'P':Parse, 'B':Bind or'Q':Query(PREPARE)
 * len: message length that is not network byte order
 * contents: message contents
 * num_tsparams: number of timestamp parameters
 * name: prepared statement name or portal name
 */
POOL_SENT_MESSAGE *pool_create_sent_message(char kind, int len, char *contents,
											int num_tsparams, const char *name,
											POOL_QUERY_CONTEXT *query_context)
{
	POOL_SENT_MESSAGE *msg;

	if (!session_context)
	{
		pool_error("pool_create_sent_message: session context is not initialized");
		return NULL;
	}

	msg = pool_memory_alloc(session_context->memory_context,
							sizeof(POOL_SENT_MESSAGE));
	msg->kind = kind;
	msg->len = len;
	msg->contents = pool_memory_alloc(session_context->memory_context, len);
	memcpy(msg->contents, contents, len);
	msg->num_tsparams = num_tsparams;
	msg->name = pool_memory_strdup(session_context->memory_context, name);
	msg->query_context = query_context;

	return msg;
}

/*
 * Add a sent message to sent message list
 */
void pool_add_sent_message(POOL_SENT_MESSAGE *message)
{
	POOL_SENT_MESSAGE *old_msg;
	POOL_SENT_MESSAGE_LIST *msglist;

	if (!session_context)
	{
		pool_error("pool_add_sent_message: session context is not initialized");
		return;
	}

	if (!message)
	{
		pool_debug("pool_add_sent_message: message is NULL");
		return;
	}

	old_msg = pool_get_sent_message(message->kind, message->name);
	msglist = &session_context->message_list;

	if (old_msg)
	{
		if (message->kind == 'B')
			pool_debug("pool_add_sent_message: portal \"%s\" already exists",
					   message->name);
		else
			pool_debug("pool_add_sent_message: prepared statement \"%s\" already exists",
					   message->name);

		if (*message->name == '\0')
			pool_remove_sent_message(old_msg->kind, old_msg->name);
		else
			return;
	}

	if (msglist->size == msglist->capacity)
	{
		msglist->capacity *= 2;
		msglist->sent_messages = realloc(msglist->sent_messages,
										 sizeof(POOL_SENT_MESSAGE *) * msglist->capacity);
		if (!msglist->sent_messages)
		{
			pool_error("pool_add_sent_message: realloc failed: %s", strerror(errno));
			exit(1);
		}
	}

	msglist->sent_messages[msglist->size++] = message;
}

/*
 * Get a sent message
 */
POOL_SENT_MESSAGE *pool_get_sent_message(char kind, const char *name)
{
	int i;
	POOL_SENT_MESSAGE_LIST *msglist;

	if (!session_context)
	{
		pool_error("pool_get_sent_message: session context is not initialized");
		return NULL;
	}

	msglist = &session_context->message_list;

	for (i = 0; i < msglist->size; i++)
	{
		if (msglist->sent_messages[i]->kind == kind &&
			!strcmp(msglist->sent_messages[i]->name, name))
			return msglist->sent_messages[i];
	}

	return NULL;
}

/*
 * We don't have a write query in this transaction yet.
 */
void pool_unset_writing_transaction(void)
{
	if (!session_context)
	{
		pool_error("pool_unset_writing_query: session context is not initialized");
		return;
	}
	session_context->writing_transaction = false;
}

/*
 * We have a write query in this transaction.
 */
void pool_set_writing_transaction(void)
{
	if (!session_context)
	{
		pool_error("pool_set_writing_query: session context is not initialized");
		return;
	}
	session_context->writing_transaction = true;
}

/*
 * Do we have a write query in this transaction?
 */
bool pool_is_writing_transaction(void)
{
	if (!session_context)
	{
		pool_error("pool_is_writing_query: session context is not initialized");
		return false;
	}
	return session_context->writing_transaction;
}

/*
 * Error doesn't occur in this transaction yet.
 */
void pool_unset_failed_transaction(void)
{
	if (!session_context)
	{
		pool_error("pool_unset_failed_query: session context is not initialized");
		return;
	}
	session_context->failed_transaction = false;
}

/*
 * Error occurred in this transaction.
 */
void pool_set_failed_transaction(void)
{
	if (!session_context)
	{
		pool_error("pool_set_failed_query: session context is not initialized");
		return;
	}
	session_context->failed_transaction = true;
}

/*
 * Did error occur in this transaction?
 */
bool pool_is_failed_transaction(void)
{
	if (!session_context)
	{
		pool_error("pool_is_failed_query: session context is not initialized");
		return false;
	}
	return session_context->failed_transaction;
}

/*
 * Forget transaction isolation mode
 */
void pool_unset_transaction_isolation(void)
{
	if (!session_context)
	{
		pool_error("pool_unset_transaction_isolation: session context is not initialized");
		return;
	}
	session_context->transaction_isolation = POOL_UNKNOWN;
}

/*
 * Set transaction isolation mode
 */
void pool_set_transaction_isolation(POOL_TRANSACTION_ISOLATION isolation_level)
{
	if (!session_context)
	{
		pool_error("pool_set_transaction_isolation: session context is not initialized");
		return;
	}
	session_context->transaction_isolation = isolation_level;
}

/*
 * Get or return cached transaction isolation mode
 */
POOL_TRANSACTION_ISOLATION pool_get_transaction_isolation(void)
{
	POOL_STATUS status;
	POOL_SELECT_RESULT *res;
	POOL_TRANSACTION_ISOLATION ret;

	if (!session_context)
	{
		pool_error("pool_get_transaction_isolation: session context is not initialized");
		return POOL_UNKNOWN;
	}

	/* It seems cached result is usable. Return it. */
	if (session_context->transaction_isolation != POOL_UNKNOWN)
		return session_context->transaction_isolation;

	/* No cached data is available. Ask backend. */
	status = do_query(MASTER(session_context->backend),
					  "SELECT current_setting('transaction_isolation')", &res, MAJOR(session_context->backend));

	if (res->numrows <= 0)
	{
		pool_error("pool_get_transaction_isolation: do_query returns no rows");
		free_select_result(res);
		return POOL_UNKNOWN;
	}
	if (res->data[0] == NULL)
	{
		pool_error("pool_get_transaction_isolation: do_query returns no data");
		free_select_result(res);
		return POOL_UNKNOWN;
	}
	if (res->nullflags[0] == -1)
	{
		pool_error("pool_get_transaction_isolation: do_query returns NULL");
		free_select_result(res);
		return POOL_UNKNOWN;
	}

	if (!strcmp(res->data[0], "read committed"))
		ret = POOL_READ_COMMITTED;
	else if (!strcmp(res->data[0], "serializable"))
		ret = POOL_SERIALIZABLE;
	else
	{
		pool_error("pool_get_transaction_isolation: unknown transaction isolation level:%s",
				   res->data[0]);
		ret = POOL_UNKNOWN;
	}   

	free_select_result(res);

	if (ret != POOL_UNKNOWN)
		session_context->transaction_isolation = ret;

	return ret;
}

/*
 * The command in progress has not succeeded yet.
 */
void pool_unset_command_success(void)
{
	if (!session_context)
	{
		pool_error("pool_unset_command_success: session context is not initialized");
		return;
	}
	session_context->command_success = false;
}

/*
 * The command in progress has succeeded.
 */
void pool_set_command_success(void)
{
	if (!session_context)
	{
		pool_error("pool_set_command_success: session context is not initialized");
		return;
	}
	session_context->command_success = true;
}

/*
 * Has the command in progress succeeded?
 */
bool pool_is_command_success(void)
{
	if (!session_context)
	{
		pool_error("pool_is_command_success: session context is not initialized");
		return false;
	}
	return session_context->command_success;
}

/*
 * Copy send map
 */
void pool_copy_prep_where(bool *src, bool *dest)
{
	memcpy(dest, src, sizeof(bool)*MAX_NUM_BACKENDS);
}
#ifdef NOT_USED
/*
 * Add to send map a PREPARED statement
 */
void pool_add_prep_where(char *name, bool *map)
{
	int i;

	if (!session_context)
	{
		pool_error("pool_add_prep_where: session context is not initialized");
		return;
	}

	for (i=0;i<POOL_MAX_PREPARED_STATEMENTS;i++)
	{
		if (*session_context->prep_where.name[i] == '\0')
		{
			strncpy(session_context->prep_where.name[i], name, POOL_MAX_PREPARED_NAME);
			pool_copy_prep_where(map, session_context->prep_where.where_to_send[i]);
			return;
		}
	}
	pool_error("pool_add_prep_where: no empty slot found");
}

/*
 * Search send map by PREPARED statement name
 */
bool *pool_get_prep_where(char *name)
{
	int i;

	if (!session_context)
	{
		pool_error("pool_get_prep_where: session context is not initialized");
		return NULL;
	}

	for (i=0;i<POOL_MAX_PREPARED_STATEMENTS;i++)
	{
		if (!strcmp(session_context->prep_where.name[i], name))
		{
			return session_context->prep_where.where_to_send[i];
		}
	}
	return NULL;
}

/*
 * Remove PREPARED statement by name
 */
void pool_delete_prep_where(char *name)
{
	int i;

	if (!session_context)
	{
		pool_error("pool_delete_prep_where: session context is not initialized");
		return;
	}

	for (i=0;i<POOL_MAX_PREPARED_STATEMENTS;i++)
	{
		if (!strcmp(session_context->prep_where.name[i], name))
		{
			memset(&session_context->prep_where.where_to_send[i], 0, sizeof(bool)*MAX_NUM_BACKENDS);
			*session_context->prep_where.name[i] = '\0';
			return;
		}
	}
}
#endif /* NOT_USED */
/*
 * Initialize sent message list
 */
static void init_sent_message_list(void)
{
	POOL_SENT_MESSAGE_LIST *msglist;

	msglist = &session_context->message_list;
	msglist->size = 0;
	msglist->capacity = INIT_LIST_SIZE;
	msglist->sent_messages = malloc(sizeof(POOL_SENT_MESSAGE *) * INIT_LIST_SIZE);
	if (!msglist->sent_messages)
	{
		pool_error("init_sent_message_list: malloc failed: %s", strerror(errno));
		exit(1);
	}
}

static bool can_query_context_destroy(POOL_QUERY_CONTEXT *qc)
{
	int i;
	int count = 0;
	POOL_SENT_MESSAGE_LIST *msglist;

	msglist = &session_context->message_list;

	for (i = 0; i < msglist->size; i++)
	{
		if (msglist->sent_messages[i]->query_context == qc)
			count++;
	}
	if (count > 1)
	{
		pool_debug("can_query_context_destroy: query context is still used.");
		return false;
	}

	return true;
}
