/*
 * $Header: /cvsroot/pgpool/pgpool-II/pcp/pcp_stream.c,v 1.4.2.1 2010/05/24 07:29:28 t-ishii Exp $
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
 * PCP buffer management module.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "pcp.h"
#include "pcp_stream.h"

static int consume_pending_data(PCP_CONNECTION *pc, void *data, int len);
static int save_pending_data(PCP_CONNECTION *pc, void *data, int len);
static int pcp_check_fd(PCP_CONNECTION *pc, int notimeout);

/* --------------------------------
 * pcp_open - allocate read & write buffers for PCP_CONNECION
 *
 * return newly allocated PCP_CONNECTION on success, NULL if malloc() fails
 * --------------------------------
 */
PCP_CONNECTION *
pcp_open(int fd)
{
    PCP_CONNECTION *pc;

    pc = (PCP_CONNECTION *)malloc(sizeof(PCP_CONNECTION));
    if (pc == NULL)
	{
		errorcode = NOMEMERR;
        return NULL;
	}
    memset(pc, 0, sizeof(*pc));

    /* initialize write buffer */
    pc->wbuf = malloc(WRITEBUFSZ);
    if (pc->wbuf == NULL)
        return NULL;
    pc->wbufsz = WRITEBUFSZ;
    pc->wbufpo = 0;

    /* initialize pending data buffer */
    pc->hp = malloc(READBUFSZ);
    if (pc->hp == NULL)
	{
		errorcode = NOMEMERR;
        return NULL;
	}
    pc->bufsz = READBUFSZ;
    pc->po = 0;
    pc->len = 0;

    pc->fd = fd;

    return pc;
}

/* --------------------------------
 * pcp_close - deallocate read & write buffers for PCP_CONNECION
 * --------------------------------
 */
void
pcp_close(PCP_CONNECTION *pc)
{
    close(pc->fd);
    free(pc->wbuf);
    free(pc->hp);
    free(pc);
}

/* --------------------------------
 * pcp_read - read 'len' bytes from 'pc'
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
int
pcp_read(PCP_CONNECTION *pc, void *buf, int len)
{
    static char readbuf[READBUFSZ];

    int consume_size;
    int readlen;
    int notimeout = 0;

    consume_size = consume_pending_data(pc, buf, len);
    len -= consume_size;
    buf += consume_size;

    while (len > 0)
    {
        if (pcp_check_fd(pc, notimeout))
		{
			errorcode = TIMEOUTERR;
			return -1;
		}

        readlen = read(pc->fd, readbuf, READBUFSZ);
        if (readlen == -1)
        {
            if (errno == EAGAIN || errno == EINTR)
                continue;

			errorcode = READERR;
			return -1;
        }
        else if (readlen == 0)
		{
			errorcode = EOFERR;
			return -1;
		}

        if (len < readlen)
        {
            /* overrun. we need to save remaining data to pending buffer */
            if (save_pending_data(pc, readbuf+len, readlen-len))
			{
				errorcode = NOMEMERR;
                return -1;
			}
            memmove(buf, readbuf, len);
            break;
        }

        memmove(buf, readbuf, readlen);
        buf += readlen;
        len -= readlen;
    }

    return 0;
}

/* --------------------------------
 * pcp_write - write 'len' bytes to 'pc' buffer
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
int
pcp_write(PCP_CONNECTION *pc, void *buf, int len)
{
    int reqlen;

    if (len < 0)
	{
		errorcode = INVALERR;
        return -1;
	}

    /* check buffer size */
    reqlen = pc->wbufpo + len;

    if (reqlen > pc->wbufsz)
    {
        char *p;

        reqlen = (reqlen/WRITEBUFSZ+1)*WRITEBUFSZ;
        p = realloc(pc->wbuf, reqlen);
        if (p == NULL)
		{
			errorcode = NOMEMERR;
			return -1;
		}

        pc->wbuf = p;
        pc->wbufsz = reqlen;
    }

    memcpy(pc->wbuf+pc->wbufpo, buf, len);
    pc->wbufpo += len;

    return 0;
}

/* --------------------------------
 * pcp_flush - send pending data in buffer to 'pc'
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
int
pcp_flush(PCP_CONNECTION *pc)
{
    int sts;
    int wlen;
    int offset;
    wlen = pc->wbufpo;

    if (wlen == 0)
    {
        return 0;
    }

    offset = 0;

    for (;;)
    {
        errno = 0;

        sts = write(pc->fd, pc->wbuf + offset, wlen);

        if (sts > 0)
        {
            wlen -= sts;

            if (wlen == 0)
            {
                /* write completed */
                break;
            }

            else if (wlen < 0)
            {
				errorcode = WRITEERR;
                return -1;
            }

            else
            {
                /* need to write remaining data */
                offset += sts;
                continue;
            }
        }
        else if (errno == EAGAIN || errno == EINTR)
        {
            continue;
        }
        else
		{
			errorcode = WRITEERR;
            return -1;
		}
    }

    pc->wbufpo = 0;

    return 0;
}

/* --------------------------------
 * consume_pending_data - read pending data from 'pc' buffer
 *
 * return the size of data read in
 * --------------------------------
 */
static int
consume_pending_data(PCP_CONNECTION *pc, void *data, int len)
{
    int consume_size;

    if (pc->len <= 0)
        return 0;

    consume_size = Min(len, pc->len);
    memmove(data, pc->hp + pc->po, consume_size);
    pc->len -= consume_size;

    if (pc->len <= 0)
        pc->po = 0;
    else
        pc->po += consume_size;

    return consume_size;
}

/* --------------------------------
 * save_pending_data - save excessively read data into 'pc' buffer
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
static int
save_pending_data(PCP_CONNECTION *pc, void *data, int len)
{
    int reqlen;
    size_t realloc_size;
    char *p;

    /* to be safe */
    if (pc->len == 0)
        pc->po = 0;

    reqlen = pc->po + pc->len + len;

    /* pending buffer is enough? */
    if (reqlen > pc->bufsz)
    {
        /* too small, enlarge it */
        realloc_size = (reqlen/READBUFSZ+1)*READBUFSZ;
        p = realloc(pc->hp, realloc_size);
        if (p == NULL)
		{
			errorcode = NOMEMERR;
            return -1;
		}
        pc->bufsz = realloc_size;
        pc->hp = p;
    }

    memmove(pc->hp + pc->po + pc->len, data, len);
    pc->len += len;

    return 0;
}

/* --------------------------------
 * pcp_check_fd - watch for fd which is ready to be read
 *
 * return 0 on success, -1 otherwise
 * --------------------------------
 */
static int
pcp_check_fd(PCP_CONNECTION *pc, int notimeout)
{
    fd_set readmask;
    fd_set exceptmask;
    int fd;
    int fds;
    struct timeval timeout;
    struct timeval *tp;

    fd = pc->fd;

    for (;;)
    {
        FD_ZERO(&readmask);
        FD_ZERO(&exceptmask);
        FD_SET(fd, &readmask);
        FD_SET(fd, &exceptmask);

        if (notimeout || (pcp_timeout.tv_sec + pcp_timeout.tv_usec) == 0)
            tp = NULL;
        else
        {
			/***** haven't got timeout option yet. hard-code it *****/
			timeout.tv_sec = pcp_timeout.tv_sec;
			timeout.tv_usec = pcp_timeout.tv_usec;
			tp = &timeout;
        }

        fds = select(fd+1, &readmask, NULL, &exceptmask, tp);

        if (fds == -1)
        {
            if (errno == EAGAIN || errno == EINTR)
                continue;

            break;
        }

        if (FD_ISSET(fd, &exceptmask))
            break;

        if (fds == 0)
            break;

        return 0;
    }

    return -1;
}
