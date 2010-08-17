/* -*-pgsql-c-*- */
/*
 *
 * $Header: /cvsroot/pgpool/pgpool-II/pool_session_context.h,v 1.18 2010/08/17 09:23:18 kitagawa Exp $
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

#ifndef POOL_SESSION_CONTEXT_H
#define POOL_SESSION_CONTEXT_H
#define INIT_LIST_SIZE 8

#include "pool.h"
#include "pool_process_context.h"
#include "pool_session_context.h"
#include "pool_query_context.h"
#include "parser/pool_memory.h"

/*
 * Prepared Statement:
 */
typedef struct {
	char *name;	/* prepared statement name */
	int num_tsparams;
	int parse_len;	/* the length of parse message which is 
					   not network byte order */
	char *parse_contents;	/* contents of parse message */
	POOL_QUERY_CONTEXT *qctxt;
} PreparedStatement;

/*
 * Prepared statement list:
 */
typedef struct {
	int capacity;	/* capacity of list */
	int size;		/* number of PreparedStatement */
	PreparedStatement **pstmts;	/* prepared statement list */
} PreparedStatementList;

/* 
 * Portal:
 */
typedef struct {
	char *name;		/* portal name */
	int num_tsparams;
	PreparedStatement *pstmt;
} Portal;

/*
 * Portal list:
 */
typedef struct {
	int capacity;	/* capacity of list */
	int size;		/* number of portal */
	Portal **portals;	/* portal list */
} PortalList;

/*
 * Transaction isolation mode
 */
typedef enum {
	POOL_UNKNOWN,				/* Unknown. Need to ask backend */
	POOL_READ_COMMITTED,		/* Read committed */
	POOL_SERIALIZABLE			/* Serializable */
} POOL_TRANSACTION_ISOLATION;

/*
 * where to send map for PREPARE/EXECUTE/DEALLOCATE
 */
#define POOL_MAX_PREPARED_STATEMENTS 128
#define POOL_MAX_PREPARED_NAME 64

typedef struct {
	int nelem;	/* Number of elements */
	char name[POOL_MAX_PREPARED_STATEMENTS][POOL_MAX_PREPARED_NAME];		/* Prepared statement name */
	bool where_to_send[POOL_MAX_PREPARED_STATEMENTS][MAX_NUM_BACKENDS];
} POOL_PREPARED_SEND_MAP;
	
/*
 * Per session context:
 */
typedef struct {
	POOL_PROCESS_CONTEXT *process_context;		/* belonging process */
	POOL_CONNECTION *frontend;	/* connection to frontend */
	POOL_CONNECTION_POOL *backend;		/* connection to backends */

	/* If true, we are waiting for backend response.  For SELECT this
	 * flags should be kept until all responses are returned from
	 * backend. i.e. until "Read for Query" packet.
	 */
	bool in_progress;

	/* If true, we are doing extended query message */
	bool doing_extended_query_message;

	/* If true, the command in progress has finished sucessfully. */
	bool command_success;

	/* If true, write query has been appeared in this transaction */
	bool writing_transaction;

	/* If true, error occurred in this transaction */
	bool failed_transaction;

	/* If true, we skip reading from backends */
	bool skip_reading_from_backends;

	/* ignore any command until Sync message */
	bool ignore_till_sync;

	/*
	 * Transaction isolation mode.
	 */
	POOL_TRANSACTION_ISOLATION transaction_isolation;

	/*
	 * Associated query context, only used for non-extended
	 * protocol. In extended protocol, the query context resides in
	 * "PreparedStatementList *pstmt_list" (see below).
	 */
	POOL_QUERY_CONTEXT *query_context;

	/* where to send map for PREPARE/EXECUTE/DEALLOCATE */
	POOL_PREPARED_SEND_MAP prep_where;

	POOL_MEMORY_POOL *memory_context;	/* memory context for session */
	PreparedStatement *unnamed_pstmt;	/* unnamed statement */
	PreparedStatement *pending_pstmt;	/* used until receive backend response */
	Portal *unnamed_portal;	/* unnamed portal */
	Portal *pending_portal;	/* used until receive backend response */
	PreparedStatementList pstmt_list;	/* named statement list */
	PortalList portal_list;	/* named portal list */

	int load_balance_node_id;	/* selected load balance node id */

	/*
	 * If true, UPDATE/DELETE caused difference in number of affected
	 * tuples in backends.
	*/
	bool mismatch_ntuples;

	/*
	 * If mismatch_ntuples true, this array holds the number of
	 * affected tuples of each node.
	 * -1 for down nodes.
	 */
	int ntuples[MAX_NUM_BACKENDS];

	/*
	 * If true, we are executing reset query list.
	 */
	bool reset_context;

#ifdef NOT_USED
/* We need to override these gotchas... */
	int internal_transaction_started;		/* to issue table lock command a transaction
											   has been started internally */
	int mismatch_ntuples;	/* number of updated tuples */
	char *copy_table = NULL;  /* copy table name */
	char *copy_schema = NULL;  /* copy table name */
	char copy_delimiter; /* copy delimiter char */
	char *copy_null = NULL; /* copy null string */

/* non 0 if "BEGIN" query with extended query protocol received */
	int receive_extended_begin = 0;

/*
 * Non 0 if allow to close internal transaction.  This variable was
 * introduced on 2008/4/3 not to close an internal transaction when
 * Sync message is received after receiving Parse message. This hack
 * is for PHP-PDO.
 */
	static int allow_close_transaction = 1;

	PreparedStatementList prepared_list; /* prepared statement name list */

	int is_select_pgcatalog = 0;
	int is_select_for_update = 0; /* 1 if SELECT INTO or SELECT FOR UPDATE */
	bool is_parallel_table = false;

/*
 * last query string sent to simpleQuery()
 */
	char query_string_buffer[QUERY_STRING_BUFFER_LEN];

/*
 * query string produced by nodeToString() in simpleQuery().
 * this variable only usefull when enable_query_cache is true.
 */
	char *parsed_query = NULL;


	int load_balance_node_id;	/* selected load balance node id */
	bool received_write_query;	/* have we recived a write query in this transaction? */
	bool send_ready_for_query;	/* ok to send ReadyForQuery */
	LOAD_BALANCE_STATUS	load_balance_status[MAX_NUM_BACKENDS];	/* to remember which DB node is selected for load balancing */
#endif

} POOL_SESSION_CONTEXT;

extern void pool_init_session_context(POOL_CONNECTION *frontend, POOL_CONNECTION_POOL *backend);
extern void pool_session_context_destroy(void);
extern POOL_SESSION_CONTEXT *pool_get_session_context(void);
extern int pool_get_local_session_id(void);
extern bool pool_is_query_in_progress(void);
extern void pool_set_query_in_progress(void);
extern void pool_unset_query_in_progress(void);
extern bool pool_is_skip_reading_from_backends(void);
extern void pool_set_skip_reading_from_backends(void);
extern void pool_unset_skip_reading_from_backends(void);
extern bool pool_is_doing_extended_query_message(void);
extern void pool_set_doing_extended_query_message(void);
extern void pool_unset_doing_extended_query_message(void);
extern bool pool_is_ignore_till_sync(void);
extern void pool_set_ignore_till_sync(void);
extern void pool_unset_ignore_till_sync(void);
extern void pool_remove_prepared_statement_by_pstmt_name(const char *name);
extern void pool_remove_prepared_statement(void);
extern void pool_remove_portal(void);
extern void pool_remove_pending_objects(void);
extern void pool_clear_prepared_statement_list(void);
extern PreparedStatement *pool_create_prepared_statement(const char *name, int num_tsparams,
														 int len, char *contents,
														 POOL_QUERY_CONTEXT *qc);
extern Portal *pool_create_portal(const char *name, int num_tsparams, PreparedStatement *pstmt);
extern void pool_add_prepared_statement(void);
extern void pool_add_portal(void);
extern PreparedStatement *pool_get_prepared_statement_by_pstmt_name(const char *name);
extern Portal *pool_get_portal_by_portal_name(const char *name);

extern void pool_unset_writing_transaction(void);
extern void pool_set_writing_transaction(void);
extern bool pool_is_writing_transaction(void);
extern void pool_unset_failed_transaction(void);
extern void pool_set_failed_transaction(void);
extern bool pool_is_failed_transaction(void);
extern void pool_unset_transaction_isolation(void);
extern void pool_set_transaction_isolation(POOL_TRANSACTION_ISOLATION isolation_level);
extern POOL_TRANSACTION_ISOLATION pool_get_transaction_isolation(void);
extern void pool_unset_command_success(void);
extern void pool_set_command_success(void);
extern bool pool_is_command_success(void);
extern void pool_copy_prep_where(bool *src, bool *dest);
extern void pool_add_prep_where(char *name, bool *map);
extern bool *pool_get_prep_where(char *name);
extern void pool_delete_prep_where(char *name);

#endif /* POOL_SESSION_CONTEXT_H */
