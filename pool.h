/* -*-pgsql-c-*- */
/*
 *
 * $Header: /cvsroot/pgpool/pgpool-II/pool.h,v 1.72 2010/06/10 06:44:03 t-ishii Exp $
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
 * pool.h.: master definition header file
 *
 */

#ifndef POOL_H
#define POOL_H

#include "config.h"
#include "pool_type.h"
#include "pool_signal.h"
#include "parser/nodes.h"

#include "libpq-fe.h"
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <limits.h>

#ifdef USE_SSL
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

/* undef this if you have problems with non blocking accept() */
#define NONE_BLOCK

#define POOLMAXPATHLEN 8192

/* configuration file name */
#define POOL_CONF_FILE_NAME "pgpool.conf"

/* PCP user/password file name */
#define PCP_PASSWD_FILE_NAME "pcp.conf"

/* HBA configuration file name */
#define HBA_CONF_FILE_NAME "pool_hba.conf"

/* pid file directory */
#define DEFAULT_LOGDIR "/tmp"

/* Unix domain socket directory */
#define DEFAULT_SOCKET_DIR "/tmp"

/* pid file name */
#define DEFAULT_PID_FILE_NAME "/var/run/pgpool/pgpool.pid"

/* status file name */
#define STATUS_FILE_NAME "pgpool_status"

typedef enum {
	POOL_CONTINUE = 0,
	POOL_IDLE,
	POOL_END,
	POOL_ERROR,
	POOL_FATAL,
	POOL_DEADLOCK
} POOL_STATUS;

/* protocol major version numbers */
#define PROTO_MAJOR_V2	2
#define PROTO_MAJOR_V3	3

/* Cancel packet proto major */
#define PROTO_CANCEL	80877102

/*
 * In protocol 3.0 and later, the startup packet length is not fixed, but
 * we set an arbitrary limit on it anyway.	This is just to prevent simple
 * denial-of-service attacks via sending enough data to run the server
 * out of memory.
 */
#define MAX_STARTUP_PACKET_LENGTH 10000


typedef struct StartupPacket_v2
{
	int			protoVersion;		/* Protocol version */
	char		database[SM_DATABASE];	/* Database name */
	char		user[SM_USER];	/* User name */
	char		options[SM_OPTIONS];	/* Optional additional args */
	char		unused[SM_UNUSED];		/* Unused */
	char		tty[SM_TTY];	/* Tty for debug output */
} StartupPacket_v2;

/* startup packet info */
typedef struct
{
	char *startup_packet;		/* raw startup packet without packet length (malloced area) */
	int len;					/* raw startup packet length */
	int major;	/* protocol major version */
	int minor;	/* protocol minor version */
	char *database;	/* database name in startup_packet (malloced area) */
	char *user;	/* user name in startup_packet (malloced area) */
} StartupPacket;

typedef struct CancelPacket
{
	int			protoVersion;		/* Protocol version */
	int			pid;	/* bcckend process id */
	int			key;	/* cancel key */
} CancelPacket;

#define MAX_PASSWORD_SIZE		1024

typedef struct {
	int num;	/* number of entries */
	char **names;		/* parameter names */
	char **values;		/* values */
} ParamStatus;

/*
 * stream connection structure
 */
typedef struct {
	int fd;		/* fd for connection */

	char *wbuf;	/* write buffer for the connection */
	int wbufsz;	/* write buffer size */
	int wbufpo;	/* buffer offset */

#ifdef USE_SSL
	SSL_CTX *ssl_ctx; /* SSL connection context */
	SSL *ssl;	/* SSL connection */
#endif
	int ssl_active; /* SSL is failed if < 0, off if 0, on if > 0 */

	char *hp;	/* pending data buffer head address */
	int po;		/* pending data offset */
	int bufsz;	/* pending data buffer size */
	int len;	/* pending data length */

	char *sbuf;	/* buffer for pool_read_string */
	int sbufsz;	/* its size in bytes */

	char *buf2;	/* buffer for pool_read2 */
	int bufsz2;	/* its size in bytes */

	int isbackend;		/* this connection is for backend if non 0 */
	int db_node_id;		/* DB node id for this connection */

	char tstate;		/* transaction state (V3 only) */

	/*
	 * following are used to remember when re-use the authenticated connection
	 */
	int auth_kind;		/* 3: clear text password, 4: crypt password, 5: md5 password */
	int pwd_size;		/* password (sent back from frontend) size in host order */
	char password[MAX_PASSWORD_SIZE];		/* password (sent back from frontend) */
	char salt[4];		/* password salt */

	/*
	 * following are used to remember current session paramter status.
	 * re-used connection will need them (V3 only)
	 */
	ParamStatus params;

	int no_forward;		/* if non 0, do not write to frontend */

	char kind;	/* kind cache */

	/*
	 * frontend info needed for hba
	 */
	int protoVersion;
	SockAddr raddr;
	UserAuth auth_method;
	char *auth_arg;
	char *database;
	char *username;
} POOL_CONNECTION;

/*
 * connection pool structure
 */
typedef struct {
	StartupPacket *sp;	/* startup packet info */
    int pid;	/* backend pid */
    int key;	/* cancel key */
    POOL_CONNECTION	*con;
	time_t closetime;	/* absolute time in second when the connection closed
						 * if 0, that means the connection is under use.
						 */
} POOL_CONNECTION_POOL_SLOT;

typedef struct {
	int pool_index;		/* currentlt used index(0 start) of pool */
	ConnectionInfo *info;		/* connection info on shmem */
    POOL_CONNECTION_POOL_SLOT	*slots[MAX_NUM_BACKENDS];
} POOL_CONNECTION_POOL;

typedef struct {
	SystemDBInfo *info;
	PGconn *pgconn;
	/* persistent connection to the system DB */
	POOL_CONNECTION_POOL_SLOT *connection;
	BACKEND_STATUS *system_db_status;
} POOL_SYSTEMDB_CONNECTION_POOL;

/* 
 * for pool_clear_cache() in pool_query_cache.c 
 *
 * used to specify the time which cached data created before it to be deleted.
 */
typedef enum {
	second, seconds,
	minute, minutes,
	hour, hours,
	day, days,
	week, weeks,
	month, months,
	year, years,
	decade, decades,
	century, centuries,
	millennium, millenniums
} UNIT;

typedef struct {
	int quantity;
	UNIT unit;
} Interval;


/* NUM_BACKENDS now always returns actual number of backends if not in_load_balance */
#define NUM_BACKENDS (in_load_balance ? (selected_slot+1) : pool_config->backend_desc->num_backends)
#define BACKEND_INFO(backend_id) (pool_config->backend_desc->backend_info[(backend_id)])
#define LOAD_BALANCE_STATUS(backend_id) (pool_config->load_balance_status[(backend_id)])

/*
 * This macro returns true if:
 *   current query is in progress and the DB node is healthy OR
 *   no query is in progress and the DB node is healthy
 */
extern bool pool_is_node_to_be_sent_in_current_query(int node_id);

#define VALID_BACKEND(backend_id) \
	(pool_is_node_to_be_sent_in_current_query((backend_id)) && \
    ((BACKEND_INFO((backend_id)).backend_status == CON_UP) || \
	 (BACKEND_INFO((backend_id)).backend_status == CON_CONNECT_WAIT)))

#define CONNECTION_SLOT(p, slot) ((p)->slots[(slot)])
#define CONNECTION(p, slot) (CONNECTION_SLOT(p, slot)->con)
#define MASTER_CONNECTION(p) ((p)->slots[MASTER_NODE_ID])
#define MASTER_NODE_ID (in_load_balance? selected_slot : Req_info->master_node_id)
#define IS_MASTER_NODE_ID(node_id) (MASTER_NODE_ID == (node_id))
#define REPLICATION (pool_config->replication_mode)
#define MASTER_SLAVE (pool_config->master_slave_mode)
#define DUAL_MODE (REPLICATION || MASTER_SLAVE)
#define PARALLEL_MODE (pool_config->parallel_mode)
#define RAW_MODE (!REPLICATION && !PARALLEL_MODE && !MASTER_SLAVE)
#define MASTER(p) MASTER_CONNECTION(p)->con
#define MAJOR(p) MASTER_CONNECTION(p)->sp->major
#define TSTATE(p) MASTER(p)->tstate
#define SYSDB_INFO (system_db_info->info)
#define SYSDB_CONNECTION (system_db_info->connection)
#define SYSDB_STATUS (*system_db_info->system_db_status)

#define Max(x, y)		((x) > (y) ? (x) : (y))
#define Min(x, y)		((x) < (y) ? (x) : (y))

#define LOCK_COMMENT "/*INSERT LOCK*/"
#define LOCK_COMMENT_SZ (sizeof(LOCK_COMMENT)-1)
#define NO_LOCK_COMMENT "/*NO INSERT LOCK*/"
#define NO_LOCK_COMMENT_SZ (sizeof(NO_LOCK_COMMENT)-1)
#define NO_LOAD_BALANCE "/*NO LOAD BALANCE*/"
#define NO_LOAD_BALANCE_COMMENT_SZ (sizeof(NO_LOAD_BALANCE)-1)

#define MAX_NUM_SEMAPHORES		3
#define CONN_COUNTER_SEM 0
#define REQUEST_INFO_SEM 1

/*
 * number specified when semaphore is locked/unlocked
 */
typedef enum SemNum
{
	SEMNUM_CONFIG,
	SEMNUM_NODES,
	SEMNUM_PROCESSES
} SemNum;

/*
 * up/down request info area in shared memory
 */
typedef enum {
	NODE_UP_REQUEST = 0,
	NODE_DOWN_REQUEST,
	NODE_RECOVERY_REQUEST,
	CLOSE_IDLE_REQUEST
} POOL_REQUEST_KIND;

typedef struct {
	POOL_REQUEST_KIND	kind;	/* request kind */
	int node_id[MAX_NUM_BACKENDS];		/* request node id */
	int master_node_id;	/* the youngest node id which is not in down status */
	int conn_counter;
} POOL_REQUEST_INFO;

/* description of row. corresponding to RowDescription message */
typedef struct {
	char *attrname;		/* attribute name */
	int oid;	/* 0 or non 0 if it's a table oblect */
	int attrnumber;		/* attribute number starting with 1. 0 if it's not a table */
	int typeoid;		/* data type oid */
	int	size;	/* data length minus means variable data type */
	int mod;	/* data type modifier */
} AttrInfo;

typedef struct {
	int num_attrs;		/* number of attributes */
	AttrInfo *attrinfo;
} RowDesc;

typedef struct {
	RowDesc *rowdesc;	/* attribute info */
	int numrows;		/* number of rows */
	int *nullflags;	/* if NULL, -1 or length of the string excluding termination null */
	char **data;		/* actual row character data terminated with null */
} POOL_SELECT_RESULT;

/*
 * global variables
 */
extern pid_t mypid; /* parent pid */
extern bool run_as_pcp_child;

extern POOL_CONNECTION_POOL *pool_connection_pool;	/* connection pool */
extern volatile sig_atomic_t backend_timer_expired; /* flag for connection closed timer is expired */
extern long int weight_master;	/* normalized weight of master (0-RAND_MAX range) */
extern int my_proc_id;  /* process table id (!= UNIX's PID) */
extern POOL_SYSTEMDB_CONNECTION_POOL *system_db_info; /* systemdb */
extern ProcessInfo *process_info; /* shmem process information table */
extern ConnectionInfo *con_info; /* shmem connection info table */
extern int in_load_balance;		/* non 0 if in load balance mode */
extern int selected_slot;		/* selected DB node for load balance */
extern int master_slave_dml;	/* non 0 if master/slave mode is specified in config file */
extern POOL_REQUEST_INFO *Req_info;
extern volatile sig_atomic_t *InRecovery;
extern char remote_ps_data[];		/* used for set_ps_display */
extern volatile sig_atomic_t got_sighup;
extern volatile sig_atomic_t exit_request;

#define QUERY_STRING_BUFFER_LEN 1024
extern char query_string_buffer[];		/* last query string sent to simpleQuery() */

/*
 * public functions
 */
extern char *get_config_file_name(void);
extern char *get_hba_file_name(void);
#ifdef __GNUC__
extern void pool_error(const char *fmt,...)
   	__attribute__((format (printf, 1, 2)));
extern void pool_debug(const char *fmt,...)
   	__attribute__((format (printf, 1, 2)));
extern void pool_log(const char *fmt,...)
   	__attribute__((format (printf, 1, 2)));
#else
extern void pool_error(const char *fmt,...);
extern void pool_debug(const char *fmt,...);
extern void pool_log(const char *fmt,...);
#endif
extern void do_child(int unix_fd, int inet_fd);
extern void pcp_do_child(int unix_fd, int inet_fd, char *pcp_conf_file);
extern int select_load_balancing_node(void);
extern int pool_init_cp(void);
extern POOL_STATUS pool_process_query(POOL_CONNECTION *frontend,
									  POOL_CONNECTION_POOL *backend,
									  int reset_request);

extern int pool_do_auth(POOL_CONNECTION *frontend, POOL_CONNECTION_POOL *backend);
extern int pool_do_reauth(POOL_CONNECTION *frontend, POOL_CONNECTION_POOL *cp);

extern int pool_init_cp(void);
extern POOL_CONNECTION_POOL *pool_create_cp(void);
extern POOL_CONNECTION_POOL *pool_get_cp(char *user, char *database, int protoMajor, int check_socket);
extern void pool_discard_cp(char *user, char *database, int protoMajor);
extern void pool_backend_timer(void);

/* SSL functionality */
extern void pool_ssl_negotiate_serverclient(POOL_CONNECTION *cp);
extern void pool_ssl_negotiate_clientserver(POOL_CONNECTION *cp);
extern void pool_ssl_close(POOL_CONNECTION *cp);
extern int pool_ssl_read(POOL_CONNECTION *cp, void *buf, int size);
extern int pool_ssl_write(POOL_CONNECTION *cp, const void *buf, int size);
extern bool pool_ssl_pending(POOL_CONNECTION *cp);

extern POOL_STATUS ErrorResponse(POOL_CONNECTION *frontend, 
								  POOL_CONNECTION_POOL *backend);

extern POOL_STATUS NoticeResponse(POOL_CONNECTION *frontend, 
								  POOL_CONNECTION_POOL *backend);

extern void notice_backend_error(int node_id);
extern void degenerate_backend_set(int *node_id_set, int count);
extern void send_failback_request(int node_id);

extern void pool_connection_pool_timer(POOL_CONNECTION_POOL *backend);
extern RETSIGTYPE pool_backend_timer_handler(int sig);

extern int connect_inet_domain_socket(int slot, bool retry);
extern int connect_unix_domain_socket(int slot, bool retry);
extern int connect_inet_domain_socket_by_port(char *host, int port, bool retry);
extern int connect_unix_domain_socket_by_port(int port, char *socket_dir, bool retry);

extern void pool_set_timeout(int timeoutval);
extern int pool_check_fd(POOL_CONNECTION *cp);

extern void pool_send_frontend_exits(POOL_CONNECTION_POOL *backend);

extern int pool_read_message_length(POOL_CONNECTION_POOL *cp);
extern int *pool_read_message_length2(POOL_CONNECTION_POOL *cp);
extern signed char pool_read_kind(POOL_CONNECTION_POOL *cp);
extern int pool_read_int(POOL_CONNECTION_POOL *cp);

extern POOL_STATUS SimpleForwardToFrontend(char kind, POOL_CONNECTION *frontend, POOL_CONNECTION_POOL *backend);
extern POOL_STATUS SimpleForwardToBackend(char kind, POOL_CONNECTION *frontend, POOL_CONNECTION_POOL *backend);
extern POOL_STATUS ParameterStatus(POOL_CONNECTION *frontend, POOL_CONNECTION_POOL *backend);

extern int pool_init_params(ParamStatus *params);
extern void pool_discard_params(ParamStatus *params);
extern char *pool_find_name(ParamStatus *params, char *name, int *pos);
extern int pool_get_param(ParamStatus *params, int index, char **name, char **value);
extern int pool_add_param(ParamStatus *params, char *name, char *value);
extern void pool_param_debug_print(ParamStatus *params);

extern void pool_send_error_message(POOL_CONNECTION *frontend, int protoMajor,
							 char *code,
							 char *message,
							 char *detail,
							 char *hint,
							 char *file,
							 int line);
extern void pool_send_fatal_message(POOL_CONNECTION *frontend, int protoMajor,
							 char *code,
							 char *message,
							 char *detail,
							 char *hint,
							 char *file,
							 int line);
extern void pool_send_severity_message(POOL_CONNECTION *frontend, int protoMajor,
							 char *code,
							 char *message,
							 char *detail,
							 char *hint,
							 char *file,
							 char *severity,
							 int line);
extern void pool_send_readyforquery(POOL_CONNECTION *frontend);
extern int send_startup_packet(POOL_CONNECTION_POOL_SLOT *cp);
extern void pool_free_startup_packet(StartupPacket *sp);
extern void child_exit(int code);

extern int health_check(void);
extern int system_db_health_check(void);

extern void init_prepared_list(void);

extern void *pool_shared_memory_create(size_t size);
extern void pool_shmem_exit(int code);

extern int pool_semaphore_create(int numSems);
extern void pool_semaphore_lock(int semNum);
extern void pool_semaphore_unlock(int semNum);

extern BackendInfo *pool_get_node_info(int node_number);
extern int pool_get_node_count(void);
extern int *pool_get_process_list(int *array_size);
extern ProcessInfo *pool_get_process_info(pid_t pid);
extern SystemDBInfo *pool_get_system_db_info(void);
extern POOL_STATUS OneNode_do_command(POOL_CONNECTION *frontend, POOL_CONNECTION *backend, char *query, char *database);

extern POOL_CONNECTION_POOL_SLOT *make_persistent_db_connection(
	char *hostname, int port, char *dbname, char *user, char *password);

/* define pool_system.c */
extern POOL_CONNECTION_POOL_SLOT *pool_system_db_connection(void);
extern DistDefInfo *pool_get_dist_def_info (char * dbname, char * schema_name, char * table_name);
extern RepliDefInfo *pool_get_repli_def_info (char * dbname, char * schema_name, char * table_name);
extern int pool_get_id (DistDefInfo *info, const char * value);
extern int system_db_connect (void);
extern int pool_memset_system_db_info (SystemDBInfo *info);
extern void pool_close_libpq_connection(void);

/* pool_query_cache.c */
extern POOL_STATUS pool_query_cache_lookup(POOL_CONNECTION *frontend, char *query, char *database, char tstate);
extern int pool_query_cache_register(char kind, POOL_CONNECTION *frontend, char *database, char *data, int data_len, char *query);
extern int pool_query_cache_table_exists(void);
extern int pool_clear_cache_by_time(Interval *interval, int size);
extern POOL_STATUS pool_execute_query_cache_lookup(POOL_CONNECTION *frontend, POOL_CONNECTION_POOL *backend, Node *node);

/* pool_hba.c */
extern void load_hba(char *hbapath);
extern void ClientAuthentication(POOL_CONNECTION *frontend);

/* pool_ip.c */
extern void pool_getnameinfo_all(SockAddr *saddr, char *remote_host, char *remote_port);

/* strlcpy.c */
extern size_t strlcpy(char *dst, const char *src, size_t siz);

/* ps_status.c */
extern bool update_process_title;
extern char **save_ps_display_args(int argc, char **argv);
extern void init_ps_display(const char *username, const char *dbname,
							const char *host_info, const char *initial_str);
extern void set_ps_display(const char *activity, bool force);
extern const char *get_ps_display(int *displen);

/* recovery.c */
extern int start_recovery(int recovery_node);
extern void finish_recovery(void);

/* child.c */
extern void cancel_request(CancelPacket *sp);
extern void check_stop_request(void);

/* pool_process_query.c */
extern void reset_variables(void);
extern void reset_connection(void);
extern void per_node_statement_log(POOL_CONNECTION_POOL *backend, int node_id, char *query);
extern void per_node_error_log(POOL_CONNECTION_POOL *backend, int node_id, char *query, char *prefix, bool unread);
extern int pool_extract_error_message(bool read_kind, POOL_CONNECTION *backend, int major, bool unread, char **message);
extern POOL_STATUS do_command(POOL_CONNECTION *frontend, POOL_CONNECTION *backend,
					   char *query, int protoMajor, int pid, int key, int no_ready_for_query);
extern POOL_STATUS do_query(POOL_CONNECTION *backend, char *query, POOL_SELECT_RESULT **result, int major);
extern void free_select_result(POOL_SELECT_RESULT *result);

/* pool_config.c */
extern int eval_logical(char *str);

#endif /* POOL_H */
