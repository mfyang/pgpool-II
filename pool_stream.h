/* -*-pgsql-c-*- */
/*
 *
 * $Header: /cvsroot/pgpool/pgpool-II/pool_stream.h,v 1.2 2010/06/06 10:17:32 t-ishii Exp $
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
 * pool_steam.h.: pool_stream.c related header file
 *
 */

#ifndef POOL_STREAM_H
#define POOL_STREAM_H

#define READBUFSZ 1024
#define WRITEBUFSZ 8192

/*
 * Return true if read buffer is empty. Argument is POOL_CONNECTION.
 */
#define pool_read_buffer_is_empty(connection) ((connection)->len <= 0)

/*
 * Discard read buffer contents
 */
#define pool_discard_read_buffer(connection) \
    do { \
       (connection)->len = 0; \
    } while (0)

extern POOL_CONNECTION *pool_open(int fd);
extern void pool_close(POOL_CONNECTION *cp);
extern int pool_read(POOL_CONNECTION *cp, void *buf, int len);
extern char *pool_read2(POOL_CONNECTION *cp, int len);
extern int pool_write(POOL_CONNECTION *cp, void *buf, int len);
extern int pool_flush(POOL_CONNECTION *cp);
extern int pool_flush_it(POOL_CONNECTION *cp);
extern int pool_write_and_flush(POOL_CONNECTION *cp, void *buf, int len);
extern char *pool_read_string(POOL_CONNECTION *cp, int *len, int line);
extern int pool_unread(POOL_CONNECTION *cp, void *data, int len);
extern void pool_set_nonblock(int fd);
extern void pool_unset_nonblock(int fd);

#endif /* POOL_STREAM_H */
