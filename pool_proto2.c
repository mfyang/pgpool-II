/* -*-pgsql-c-*- */
/*
 * $Header: /cvsroot/pgpool/pgpool-II/pool_proto2.c,v 1.6 2010/08/17 04:21:35 kitagawa Exp $
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
 *---------------------------------------------------------------------
 * pool_proto2.c: modules corresponding to protocol 2.0.
 * used by pool_process_query()
 *---------------------------------------------------------------------
 */
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "pool.h"
#include "pool_config.h"
#include "pool_proto_modules.h"
#include "pool_stream.h"

POOL_STATUS AsciiRow(POOL_CONNECTION *frontend,
					 POOL_CONNECTION_POOL *backend,
					 short num_fields)
{
	static char nullmap[8192], nullmap1[8192];
	int nbytes;
	int i, j;
	unsigned char mask;
	int size, size1 = 0;
	char *buf = NULL, *sendbuf = NULL;
	char msgbuf[1024];

	pool_write(frontend, "D", 1);

	nbytes = (num_fields + 7)/8;

	if (nbytes <= 0)
		return POOL_CONTINUE;

	/* NULL map */
	pool_read(MASTER(backend), nullmap, nbytes);
	memcpy(nullmap1, nullmap, nbytes);
	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i) && !IS_MASTER_NODE_ID(i))
		{
			pool_read(CONNECTION(backend, i), nullmap, nbytes);
			if (memcmp(nullmap, nullmap1, nbytes))
			{
				/* XXX: NULLMAP maybe different among
				   backends. If we were a paranoid, we have to treat
				   this as a fatal error. However in the real world
				   we'd better to adapt this situation. Just throw a
				   log... */
				pool_debug("AsciiRow: NULLMAP differ between master and %d th backend", i);
			}
		}
	}

	if (pool_write(frontend, nullmap1, nbytes) < 0)
		return POOL_END;

	mask = 0;

	for (i = 0;i<num_fields;i++)
	{
		if (mask == 0)
			mask = 0x80;

		/* NOT NULL? */
		if (mask & nullmap[i/8])
		{
			/* field size */
			if (pool_read(MASTER(backend), &size, sizeof(int)) < 0)
				return POOL_END;

			size1 = ntohl(size) - 4;

			/* read and send actual data only when size > 0 */
			if (size1 > 0)
			{
				sendbuf = pool_read2(MASTER(backend), size1);
				if (sendbuf == NULL)
					return POOL_END;
			}

			/* forward to frontend */
			pool_write(frontend, &size, sizeof(int));
			pool_write(frontend, sendbuf, size1);
			snprintf(msgbuf, Min(sizeof(msgbuf), size1+1), "%s", sendbuf);
			pool_debug("AsciiRow: len: %d data: %s", size1, msgbuf);

			for (j=0;j<NUM_BACKENDS;j++)
			{
				if (VALID_BACKEND(j) && !IS_MASTER_NODE_ID(j))
				{
					/* field size */
					if (pool_read(CONNECTION(backend, j), &size, sizeof(int)) < 0)
						return POOL_END;

					buf = NULL;
					size = ntohl(size) - 4;

					/* XXX: field size maybe different among
					   backends. If we were a paranoid, we have to treat
					   this as a fatal error. However in the real world
					   we'd better to adapt this situation. Just throw a
					   log... */
					if (size != size1)
						pool_debug("AsciiRow: %d th field size does not match between master(%d) and %d th backend(%d)",
								   i, ntohl(size), j, ntohl(size1));

					/* read and send actual data only when size > 0 */
					if (size > 0)
					{
						buf = pool_read2(CONNECTION(backend, j), size);
						if (buf == NULL)
							return POOL_END;
					}
				}
			}
		}

		mask >>= 1;
	}

	if (pool_flush(frontend))
		return POOL_END;

	return POOL_CONTINUE;
}

POOL_STATUS BinaryRow(POOL_CONNECTION *frontend,
					  POOL_CONNECTION_POOL *backend,
					  short num_fields)
{
	static char nullmap[8192], nullmap1[8192];
	int nbytes;
	int i, j;
	unsigned char mask;
	int size, size1 = 0;
	char *buf = NULL;

	pool_write(frontend, "B", 1);

	nbytes = (num_fields + 7)/8;

	if (nbytes <= 0)
		return POOL_CONTINUE;

	/* NULL map */
	pool_read(MASTER(backend), nullmap, nbytes);
	if (pool_write(frontend, nullmap, nbytes) < 0)
		return POOL_END;
	memcpy(nullmap1, nullmap, nbytes);
	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i) && !IS_MASTER_NODE_ID(i))
		{
			pool_read(CONNECTION(backend, i), nullmap, nbytes);
			if (memcmp(nullmap, nullmap1, nbytes))
			{
				/* XXX: NULLMAP maybe different among
				   backends. If we were a paranoid, we have to treat
				   this as a fatal error. However in the real world
				   we'd better to adapt this situation. Just throw a
				   log... */
				pool_debug("BinaryRow: NULLMAP differ between master and %d th backend", i);
			}
		}
	}

	mask = 0;

	for (i = 0;i<num_fields;i++)
	{
		if (mask == 0)
			mask = 0x80;

		/* NOT NULL? */
		if (mask & nullmap[i/8])
		{
			/* field size */
			if (pool_read(MASTER(backend), &size, sizeof(int)) < 0)
				return POOL_END;
			for (j=0;j<NUM_BACKENDS;j++)
			{
				if (VALID_BACKEND(j) && !IS_MASTER_NODE_ID(j))
				{
					/* field size */
					if (pool_read(CONNECTION(backend, i), &size, sizeof(int)) < 0)
						return POOL_END;

					/* XXX: field size maybe different among
					   backends. If we were a paranoid, we have to treat
					   this as a fatal error. However in the real world
					   we'd better to adapt this situation. Just throw a
					   log... */
					if (size != size1)
						pool_debug("BinaryRow: %d th field size does not match between master(%d) and %d th backend(%d)",
								   i, ntohl(size), j, ntohl(size1));
				}

				buf = NULL;

				/* forward to frontend */
				if (IS_MASTER_NODE_ID(j))
					pool_write(frontend, &size, sizeof(int));
				size = ntohl(size) - 4;

				/* read and send actual data only when size > 0 */
				if (size > 0)
				{
					buf = pool_read2(CONNECTION(backend, j), size);
					if (buf == NULL)
						return POOL_END;

					if (IS_MASTER_NODE_ID(j))
					{
						pool_write(frontend, buf, size);
					}
				}
			}

			mask >>= 1;
		}
	}

	if (pool_flush(frontend))
		return POOL_END;

	return POOL_CONTINUE;
}

POOL_STATUS CompletedResponse(POOL_CONNECTION *frontend,
							  POOL_CONNECTION_POOL *backend)
{
	int i;
	char *string = NULL;
	char *string1 = NULL;
	int len, len1 = 0;

	/* read command tag */
	string = pool_read_string(MASTER(backend), &len, 0);
	if (string == NULL)
		return POOL_END;
	else if (!strncmp(string, "BEGIN", 5))
		TSTATE(backend, MASTER_NODE_ID) = 'T';
	else if (!strncmp(string, "COMMIT", 6) || !strncmp(string, "ROLLBACK", 8))
		TSTATE(backend, MASTER_NODE_ID) = 'I';

	len1 = len;
	string1 = strdup(string);

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (!VALID_BACKEND(i) || IS_MASTER_NODE_ID(i))
			continue;

		/* read command tag */
		string = pool_read_string(CONNECTION(backend, i), &len, 0);
		if (string == NULL)
			return POOL_END;
		else if (!strncmp(string, "BEGIN", 5))
			TSTATE(backend, i) = 'T';
		else if (!strncmp(string, "COMMIT", 6) || !strncmp(string, "ROLLBACK", 8))
			TSTATE(backend, i) = 'I';
 
		if (len != len1)
		{
			pool_debug("CompletedResponse: message length does not match between master(%d \"%s\",) and %d th server (%d \"%s\",)",
					   len, string, i, len1, string1);

			/* we except INSERT, because INSERT response has OID */
			if (strncmp(string1, "INSERT", 6))
			{
				free(string1);
				return POOL_END;
			}
		}
	}
	/* forward to the frontend */
	pool_write(frontend, "C", 1);
	pool_debug("CompletedResponse: string: \"%s\"", string1);
	if (pool_write(frontend, string1, len1) < 0)
	{
		free(string1);
		return POOL_END;
	}

	free(string1);
	return pool_flush(frontend);
}

POOL_STATUS CursorResponse(POOL_CONNECTION *frontend,
						   POOL_CONNECTION_POOL *backend)
{
	char *string = NULL;
	char *string1 = NULL;
	int len, len1 = 0;
	int i;

	/* read cursor name */
	string = pool_read_string(MASTER(backend), &len, 0);
	if (string == NULL)
		return POOL_END;
	len1 = len;
	string1 = strdup(string);

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i) && !IS_MASTER_NODE_ID(i))
		{
			/* read cursor name */
			string = pool_read_string(CONNECTION(backend, i), &len, 0);
			if (string == NULL)
				return POOL_END;
			if (len != len1)
			{
				pool_error("CursorResponse: length does not match between master(%d) and %d th backend(%d)",
						   len, i, len1);
				pool_error("CursorResponse: master(%s) %d th backend(%s)", string1, i, string);
				free(string1);
				return POOL_END;
			}
		}
	}

	/* forward to the frontend */
	pool_write(frontend, "P", 1);
	if (pool_write(frontend, string1, len1) < 0)
	{
		free(string1);
		return POOL_END;
	}
	free(string1);

	if (pool_flush(frontend))
		return POOL_END;

	return POOL_CONTINUE;
}

POOL_STATUS EmptyQueryResponse(POOL_CONNECTION *frontend,
							   POOL_CONNECTION_POOL *backend)
{
	char c;
	int i;

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i))
		{
			if (pool_read(CONNECTION(backend, i), &c, sizeof(c)) < 0)
				return POOL_END;
		}
	}

	pool_write(frontend, "I", 1);
	return pool_write_and_flush(frontend, "", 1);
}

POOL_STATUS ErrorResponse(POOL_CONNECTION *frontend,
						  POOL_CONNECTION_POOL *backend)
{
	char *string = NULL;
	int len;
	int i;
	POOL_STATUS ret;

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i))
		{
			/* read error message */
			string = pool_read_string(CONNECTION(backend, i), &len, 0);
			if (string == NULL)
				return POOL_END;
		}
	}

	/* forward to the frontend */
	pool_write(frontend, "E", 1);
	if (pool_write_and_flush(frontend, string, len) < 0)
		return POOL_END;

	ret = raise_intentional_error_if_need(backend);

	/* change transaction state */
	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i))
		{
			if (TSTATE(backend, i) == 'T')
				TSTATE(backend, i) = 'E';
		}
	}

	return ret;
}

POOL_STATUS FunctionResultResponse(POOL_CONNECTION *frontend,
								   POOL_CONNECTION_POOL *backend)
{
	char dummy;
	int len;
	char *result = 0;
	int i;

	pool_write(frontend, "V", 1);

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i))
		{
			if (pool_read(CONNECTION(backend, i), &dummy, 1) < 0)
				return POOL_ERROR;
		}
	}
	pool_write(frontend, &dummy, 1);

	/* non empty result? */
	if (dummy == 'G')
	{
		for (i=0;i<NUM_BACKENDS;i++)
		{
			if (VALID_BACKEND(i))
			{
				/* length of result in bytes */
				if (pool_read(CONNECTION(backend, i), &len, sizeof(len)) < 0)
					return POOL_ERROR;
			}
		}
		pool_write(frontend, &len, sizeof(len));

		len = ntohl(len);

		for (i=0;i<NUM_BACKENDS;i++)
		{
			if (VALID_BACKEND(i))
			{
				/* result value itself */
				if ((result = pool_read2(MASTER(backend), len)) == NULL)
					return POOL_ERROR;
			}
		}
		pool_write(frontend, result, len);
	}

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i))
		{
			/* unused ('0') */
			if (pool_read(MASTER(backend), &dummy, 1) < 0)
				return POOL_ERROR;
		}
	}
	pool_write(frontend, "0", 1);

	return pool_flush(frontend);
}

POOL_STATUS NoticeResponse(POOL_CONNECTION *frontend,
						   POOL_CONNECTION_POOL *backend)
{
	char *string = NULL;
	int len;
	int i;

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i))
		{
			/* read notice message */
			string = pool_read_string(CONNECTION(backend, i), &len, 0);
			if (string == NULL)
				return POOL_END;
		}
	}

	/* forward to the frontend */
	pool_write(frontend, "N", 1);
	if (pool_write_and_flush(frontend, string, len) < 0)
	{
		return POOL_END;
	}
	return POOL_CONTINUE;
}

POOL_STATUS NotificationResponse(POOL_CONNECTION *frontend,
								 POOL_CONNECTION_POOL *backend)
{
	int pid, pid1;
	char *condition, *condition1 = NULL;
	int len, len1 = 0;
	int i;
	POOL_STATUS status;

	pool_write(frontend, "A", 1);

	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i))
		{
			if (pool_read(CONNECTION(backend, i), &pid, sizeof(pid)) < 0)
				return POOL_ERROR;
			condition = pool_read_string(CONNECTION(backend, i), &len, 0);
			if (condition == NULL)
				return POOL_END;

			if (IS_MASTER_NODE_ID(i))
			{
				pid1 = pid;
				len1 = len;
				condition1 = strdup(condition);
			}
		}
	}

	pool_write(frontend, &pid1, sizeof(pid1));
	status = pool_write_and_flush(frontend, condition1, len1);
	free(condition1);
	return status;
}

int RowDescription(POOL_CONNECTION *frontend,
				   POOL_CONNECTION_POOL *backend,
				   short *result)
{
	short num_fields, num_fields1 = 0;
	int oid, mod;
	int oid1, mod1;
	short size, size1;
	char *string;
	int len, len1;
	int i;

	pool_read(MASTER(backend), &num_fields, sizeof(short));
	num_fields1 = num_fields;
	for (i=0;i<NUM_BACKENDS;i++)
	{
		if (VALID_BACKEND(i) && !IS_MASTER_NODE_ID(i))
		{
			/* # of fields (could be 0) */
			pool_read(CONNECTION(backend, i), &num_fields, sizeof(short));
			if (num_fields != num_fields1)
			{
				pool_error("RowDescription: num_fields does not match between backends master(%d) and %d th backend(%d)",
						   num_fields, i, num_fields1);
				return POOL_FATAL;
			}
		}
	}

	/* forward it to the frontend */
	pool_write(frontend, "T", 1);
	pool_write(frontend, &num_fields, sizeof(short));
	num_fields = ntohs(num_fields);
	for (i = 0;i<num_fields;i++)
	{
		int j;

		/* field name */
		string = pool_read_string(MASTER(backend), &len, 0);
		if (string == NULL)
			return POOL_END;
		len1 = len;
		if (pool_write(frontend, string, len) < 0)
			return POOL_END;

		for (j=0;j<NUM_BACKENDS;j++)
		{
			if (VALID_BACKEND(j) && !IS_MASTER_NODE_ID(j))
			{
				string = pool_read_string(CONNECTION(backend, j), &len, 0);
				if (string == NULL)
					return POOL_END;

				if (len != len1)
				{
					pool_error("RowDescription: field length does not match between backends master(%d) and %d th backend(%d)",
							   ntohl(len), j, ntohl(len1));
					return POOL_FATAL;
				}
			}
		}

		/* type oid */
		pool_read(MASTER(backend), &oid, sizeof(int));
		oid1 = oid;
		pool_debug("RowDescription: type oid: %d", ntohl(oid));
		for (j=0;j<NUM_BACKENDS;j++)
		{
			if (VALID_BACKEND(j) && !IS_MASTER_NODE_ID(j))
			{
				pool_read(CONNECTION(backend, j), &oid, sizeof(int));

				/* we do not regard oid mismatch as fatal */
				if (oid != oid1)
				{
					pool_debug("RowDescription: field oid does not match between backends master(%d) and %d th backend(%d)",
							   ntohl(oid), j, ntohl(oid1));
				}
			}
		}
		if (pool_write(frontend, &oid1, sizeof(int)) < 0)
			return POOL_END;

		/* size */
		pool_read(MASTER(backend), &size, sizeof(short));
		size1 = size;
		for (j=0;j<NUM_BACKENDS;j++)
		{
			if (VALID_BACKEND(j) && !IS_MASTER_NODE_ID(j))
			{
				pool_read(CONNECTION(backend, j), &size, sizeof(short));
				if (size1 != size1)
				{
					pool_error("RowDescription: field size does not match between backends master(%d) and %d th backend(%d)",
							   ntohs(size), j, ntohs(size1));
					return POOL_FATAL;
				}
			}
		}
		pool_debug("RowDescription: field size: %d", ntohs(size));
		pool_write(frontend, &size1, sizeof(short));

		/* modifier */
		pool_read(MASTER(backend), &mod, sizeof(int));
		pool_debug("RowDescription: modifier: %d", ntohs(mod));
		mod1 = mod;
		for (j=0;j<NUM_BACKENDS;j++)
		{
			if (VALID_BACKEND(j) && !IS_MASTER_NODE_ID(j))
			{
				pool_read(CONNECTION(backend, j), &mod, sizeof(int));
				if (mod != mod1)
				{
					pool_debug("RowDescription: modifier does not match between backends master(%d) and %d th backend(%d)",
							   ntohl(mod), j, ntohl(mod1));
				}
			}
		}
		if (pool_write(frontend, &mod1, sizeof(int)) < 0)
			return POOL_END;
	}

	*result = num_fields;

	return pool_flush(frontend);
}
