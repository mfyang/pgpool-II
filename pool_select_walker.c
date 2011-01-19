/* -*-pgsql-c-*- */
/*
 * $Header: /cvsroot/pgpool/pgpool-II/pool_select_walker.c,v 1.8 2011/01/19 02:52:59 t-ishii Exp $
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
#include <stdlib.h>
#include <string.h>

#include "pool.h"
#include "pool_config.h"
#include "pool_select_walker.h"
#include "pool_relcache.h"
#include "parser/parsenodes.h"
#include "pool_session_context.h"

typedef struct {
	bool	has_system_catalog;		/* True if system catalog table is used */
	bool	has_temp_table;		/* True if temporary table is used */
	bool	has_function_call;	/* True if write function call is used */	

} SelectContext;

static bool function_call_walker(Node *node, void *context);
static bool system_catalog_walker(Node *node, void *context);
static bool is_system_catalog(char *table_name);
static bool temp_table_walker(Node *node, void *context);
static bool is_temp_table(char *table_name);

/*
 * Return true if this SELECT has function calls.
 */
bool pool_has_function_call(Node *node)
{

	SelectContext	ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_function_call = false;

	raw_expression_tree_walker(node, function_call_walker, &ctx);

	return ctx.has_function_call;
}

/*
 * Return true if this SELECT has system catalog table.
 */
bool pool_has_system_catalog(Node *node)
{

	SelectContext	ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_system_catalog = false;

	raw_expression_tree_walker(node, system_catalog_walker, &ctx);

	return ctx.has_system_catalog;
}

/*
 * Return true if this SELECT has temporary table.
 */
bool pool_has_temp_table(Node *node)
{

	SelectContext	ctx;

	if (!IsA(node, SelectStmt))
		return false;

	ctx.has_temp_table = false;

	raw_expression_tree_walker(node, temp_table_walker, &ctx);

	return ctx.has_temp_table;
}

/*
 * Search function name in whilelist or blacklist regex array
 * Return 1 on success (found in list)
 * Return 0 when not found in list
 * Return -1 if the given search type doesn't exist.
 * Search type supported are: WHITELIST and BLACKLIST 
 */
int pattern_compare(char *str, const int type)
{
	int i = 0;

	/* pass througth all regex pattern unless pattern is found */
	for (i = 0; i < pool_config->pattc; i++) {
		if ( (pool_config->lists_patterns[i].type == type) && (regexec(&pool_config->lists_patterns[i].regexv, str, 0, 0, 0) == 0) ) {
			switch(type) {
			/* return 1 if string matches whitelist pattern */
			case WHITELIST:
				if (pool_config->debug_level > 0)
					pool_debug("pattern_compare: white_function_list (%s) matched: %s", pool_config->lists_patterns[i].pattern, str);
				return 1;
			/* return 1 if string matches blacklist pattern */
			case BLACKLIST:
				if (pool_config->debug_level > 0)
					pool_debug("pattern_compare: black_function_list (%s) matched: %s", pool_config->lists_patterns[i].pattern, str);
				return 1;
			default:
				pool_error("pattern_compare: unknown pattern match type: %s", str);
				return -1;
			}
		}
	}

	/* return 0 otherwise */
	return 0;
}

/*
 * Walker function to find a function call
 */
static bool function_call_walker(Node *node, void *context)
{
	SelectContext	*ctx = (SelectContext *) context;

	if (node == NULL)
		return false;

	if (IsA(node, FuncCall))
	{
		FuncCall *fcall = (FuncCall *)node;
		char *fname;
		int length = list_length(fcall->funcname);

		if (length > 0)
		{
			if (length == 1)	/* no schema qualification? */
			{
				fname = strVal(linitial(fcall->funcname));
			}
			else
			{
				fname = strVal(lsecond(fcall->funcname));		/* with schema qualification */
			}

			pool_debug("function_call_walker: function name: %s", fname);

			/*
			 * Check white list if any.
			 */
			if (pool_config->num_white_function_list > 0)
			{
				/* Search function in the white list regex patterns */
				if (pattern_compare(fname, WHITELIST) == 1) {
					/* If the function is found in the white list, we can ignore it */
					return raw_expression_tree_walker(node, function_call_walker, context);
				}
				/*
				 * Since the function was not found in white list, we
				 * have found a writing function.
				 */
				ctx->has_function_call = true;
				return false;
			}

			/*
			 * Check black list if any.
			 */
			if (pool_config->num_black_function_list > 0)
			{
				/* Search function in the black list regex patterns */
				if (pattern_compare(fname, BLACKLIST) == 1) {
					/* Found. */
					ctx->has_function_call = true;
					return false;
				}
			}
		}
	}
	return raw_expression_tree_walker(node, function_call_walker, context);
}

/*
 * Walker function to find a system catalog
 */
static bool
system_catalog_walker(Node *node, void *context)
{
	SelectContext	*ctx = (SelectContext *) context;

	if (node == NULL)
		return false;

	if (IsA(node, RangeVar))
	{
		RangeVar *rgv = (RangeVar *)node;

		pool_debug("system_catalog_walker: relname: %s", rgv->relname);

		if (is_system_catalog(rgv->relname))
		{
			ctx->has_system_catalog = true;
			return false;
		}
	}
	return raw_expression_tree_walker(node, system_catalog_walker, context);
}

/*
 * Walker function to find a temp table
 */
static bool
temp_table_walker(Node *node, void *context)
{
	SelectContext	*ctx = (SelectContext *) context;

	if (node == NULL)
		return false;

	if (IsA(node, RangeVar))
	{
		RangeVar *rgv = (RangeVar *)node;

		pool_debug("temp_table_walker: relname: %s", rgv->relname);

		if (is_temp_table(rgv->relname))
		{
			ctx->has_temp_table = true;
			return false;
		}
	}
	return raw_expression_tree_walker(node, temp_table_walker, context);
}

/*
 * Judge the table used in a query represented by node is a system
 * catalog or not.
 */
static bool is_system_catalog(char *table_name)
{
/*
 * Query to know if pg_namespace exists. PostgreSQL 7.2 or before doesn't have.
 */
#define HASPGNAMESPACEQUERY "SELECT count(*) FROM pg_catalog.pg_class AS c WHERE c.relname = '%s'"

/*
 * Query to know if the target table belongs pg_catalog schema.
 */
#define ISBELONGTOPGCATALOGQUERY "SELECT count(*) FROM pg_class AS c, pg_namespace AS n WHERE c.relname = '%s' AND c.relnamespace = n.oid AND n.nspname = 'pg_catalog'"

#define ISBELONGTOPGCATALOGQUERY2 "SELECT count(*) FROM pg_class AS c, pg_namespace AS n WHERE c.oid = pgpool_regclass('%s') AND c.relnamespace = n.oid AND n.nspname = 'pg_catalog'"

	int hasreliscatalog;
	bool result;
	static POOL_RELCACHE *hasreliscatalog_cache;
	static POOL_RELCACHE *relcache;
	POOL_CONNECTION_POOL *backend;

	if (table_name == NULL)
	{
			return false;
	}

	backend = pool_get_session_context()->backend;

	/*
	 * Check if pg_namespace exists
	 */
	if (!hasreliscatalog_cache)
	{
		char *query;

		/* pgpool_regclass has been installed */
		if (pool_has_pgpool_regclass())
		{
			query = ISBELONGTOPGCATALOGQUERY2;
		}
		else
		{
			query = ISBELONGTOPGCATALOGQUERY;
		}

		hasreliscatalog_cache = pool_create_relcache(32, query,
										int_register_func, int_unregister_func,
										false);
		if (hasreliscatalog_cache == NULL)
		{
			pool_error("is_system_catalog: pool_create_relcache error");
			return false;
		}
	}

	hasreliscatalog = pool_search_relcache(hasreliscatalog_cache, backend, "pg_namespace")==0?0:1;

	if (hasreliscatalog)
	{
		/*
		 * If relcache does not exist, create it.
		 */
		if (!relcache)
		{
			relcache = pool_create_relcache(32, ISBELONGTOPGCATALOGQUERY,
											int_register_func, int_unregister_func,
											true);
			if (relcache == NULL)
			{
				pool_error("is_system_catalog: pool_create_relcache error");
				return false;
			}
		}
		/*
		 * Search relcache.
		 */
		result = pool_search_relcache(relcache, backend, table_name)==0?false:true;
		return result;
	}

	/*
	 * Pre 7.3. Just check whether the table starts with "pg_".
	 */
	return (strcasecmp(table_name, "pg_") == 0);
}

/*
 * Judge the table used in a query represented by node is a temporary
 * table or not.
 */
static bool is_temp_table(char *table_name)
{
/*
 * Query to know if pg_class has relistemp column or not.
 * PostgreSQL 8.4 or later has this.
 */
#define HASRELITEMPPQUERY "SELECT count(*) FROM pg_catalog.pg_class AS c, pg_attribute AS a WHERE c.relname = 'pg_class' AND a.attrelid = c.oid AND a.attname = 'relistemp'"

/*
 * Query to know if the target table is a temporary one.  This query
 * is valid through PostgreSQL 7.3 to 8.3.  We do not use regclass (or
 * its variant) here, because temporary tables never have schema
 * qualified name.
 */
#define ISTEMPQUERY83 "SELECT count(*) FROM pg_class AS c, pg_namespace AS n WHERE c.relname = '%s' AND c.relnamespace = n.oid AND n.nspname ~ '^pg_temp_'"

/*
 * Query to know if the target table is a temporary one.  This query
 * is valid PostgreSQL 8.4 or later. We do not use regclass (or its
 * variant) here, because temporary tables never have schema qualified
 * name.
 */
#define ISTEMPQUERY84 "SELECT count(*) FROM pg_catalog.pg_class AS c WHERE c.relname = '%s' AND c.relistemp"

	int hasrelistemp;
	bool result;
	static POOL_RELCACHE *hasrelistemp_cache;
	static POOL_RELCACHE *relcache;
	char *query;
	POOL_CONNECTION_POOL *backend;

	if (table_name == NULL)
	{
			return false;
	}

	backend = pool_get_session_context()->backend;

	/*
	 * Check backend version
	 */
	if (!hasrelistemp_cache)
	{
		hasrelistemp_cache = pool_create_relcache(32, HASRELITEMPPQUERY,
										int_register_func, int_unregister_func,
										false);
		if (hasrelistemp_cache == NULL)
		{
			pool_error("is_temp_table: pool_create_relcache error");
			return false;
		}
	}

	hasrelistemp = pool_search_relcache(hasrelistemp_cache, backend, "pg_class")==0?0:1;
	if (hasrelistemp)
		query = ISTEMPQUERY84;
	else
		query = ISTEMPQUERY83;

	/*
	 * If relcache does not exist, create it.
	 */
	if (!relcache)
	{
		relcache = pool_create_relcache(32, query,
										int_register_func, int_unregister_func,
										true);
		if (relcache == NULL)
		{
			pool_error("is_temp_table: pool_create_relcache error");
			return false;
		}
	}

	/*
	 * Search relcache.
	 */
	result = pool_search_relcache(relcache, backend, table_name)==0?false:true;
	return result;
}

/*
 * Judge if we have pgpool_regclass or not.
 */
bool pool_has_pgpool_regclass(void)
{
/*
 * Query to know if pgpool_regclass exists.
 */
#define HASPGPOOL_REGCLASSQUERY "SELECT count(*) FROM pg_catalog.pg_proc AS p WHERE p.proname = '%s'"
	bool result;
	static POOL_RELCACHE *relcache;
	POOL_CONNECTION_POOL *backend;

	backend = pool_get_session_context()->backend;

	if (!relcache)
	{
		relcache = pool_create_relcache(32, HASPGPOOL_REGCLASSQUERY,
										int_register_func, int_unregister_func,
										false);
		if (relcache == NULL)
		{
			pool_error("has_pgpool_regclass: pool_create_relcache error");
			return false;
		}
	}

	result = pool_search_relcache(relcache, backend, "pgpool_regclass")==0?0:1;
	return result;
}
