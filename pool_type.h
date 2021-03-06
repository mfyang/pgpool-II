/* -*-pgsql-c-*- */
/*
 *
 * $Header: /cvsroot/pgpool/pgpool-II/pool_type.h,v 1.17 2011/04/23 07:49:02 t-ishii Exp $
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
 * pool_type.h.: type definition header file
 *
 */

#ifndef POOL_TYPE_H
#define POOL_TYPE_H

#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "pcp/libpcp_ext.h"

/* Define common boolean type. C++ and BEOS already has it so exclude them. */
#ifdef c_plusplus
#ifndef __cplusplus
#define __cplusplus
#endif /* __cplusplus */
#endif /* c_plusplus */

#ifndef __BEOS__
#ifndef __cplusplus
#ifndef bool
typedef char bool;
#endif
#ifndef true
#define true ((bool) 1)
#endif
#ifndef TRUE
#define TRUE ((bool) 1)
#endif
#ifndef false
#define false ((bool) 0)
#endif
#ifndef FALSE
#define FALSE ((bool) 0)
#endif
#endif /* not C++ */
#endif /* __BEOS__ */

typedef signed char int8;		/* == 8 bits */
typedef signed short int16;		/* == 16 bits */
typedef signed int int32;		/* == 32 bits */

/*
 * uintN
 *		Unsigned integer, EXACTLY N BITS IN SIZE,
 *		used for numerical computations and the
 *		frontend/backend protocol.
 */
typedef unsigned char uint8;	/* == 8 bits */
typedef unsigned short uint16;	/* == 16 bits */
typedef unsigned int uint32;	/* == 32 bits */

typedef enum {
	LOAD_UNSELECTED = 0,
	LOAD_SELECTED
} LOAD_BALANCE_STATUS;

/*
 * Backend status record file
 */
typedef struct {
	BACKEND_STATUS status[MAX_NUM_BACKENDS];
} BackendStatusRecord;

/*
 *  It seems that sockaddr_storage is now commonly used in place of sockaddr.
 *  So, define it if it is not define yet, and create new SockAddr structure
 *  that uses sockaddr_storage.
 */
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE___SS_FAMILY
#define ss_family __ss_family
#else
#error struct sockaddr_storage does not provide an ss_family member
#endif /* HAVE_STRUCT_SOCKADDR_STORAGE___SS_FAMILY */
#endif /* HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY */

#ifdef HAVE_STRUCT_SOCKADDR_STORAGE___SS_LEN
#define ss_len __ss_len
#define HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN 1
#endif /* HAVE_STRUCT_SOCKADDR_STORAGE___SS_LEN */

#else /* !HAVE_STRUCT_SOCKADDR_STORAGE */

/* Define a struct sockaddr_storage if we don't have one. */
struct sockaddr_storage
{
	union
	{
		struct sockaddr sa;		/* get the system-dependent fields */
		long int ss_align; /* ensures struct is properly aligned. original uses int64 */
		char ss_pad[128];		/* ensures struct has desired size */
	}
	ss_stuff;
};

#define ss_family   ss_stuff.sa.sa_family
/* It should have an ss_len field if sockaddr has sa_len. */
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
#define ss_len      ss_stuff.sa.sa_len
#define HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN 1
#endif
#endif /* HAVE_STRUCT_SOCKADDR_STORAGE */

typedef struct
{
	struct sockaddr_storage addr;
	/* ACCEPT_TYPE_ARG3 - Third argument type of accept().
	 * It is defined in ac_func_accept_argtypes.m4
	 */
	ACCEPT_TYPE_ARG3 salen;
}
SockAddr;

/* UserAuth type used for HBA which indicates the authentication method */
typedef enum UserAuth
{
	uaReject,
	/*  uaKrb4, */
	/*  uaKrb5, */
	uaTrust,
	/*  uaIdent, */
	/*  uaPassword, */
	/*  uaCrypt, */
	uaMD5
#ifdef USE_PAM
	,uaPAM
#endif /* USE_PAM */
}
UserAuth;

#define AUTH_REQ_OK         0   /* User is authenticated  */
#define AUTH_REQ_KRB4       1   /* Kerberos V4 */
#define AUTH_REQ_KRB5       2   /* Kerberos V5 */
#define AUTH_REQ_PASSWORD   3   /* Password */
#define AUTH_REQ_CRYPT      4   /* crypt password */
#define AUTH_REQ_MD5        5   /* md5 password */
#define AUTH_REQ_SCM_CREDS  6   /* transfer SCM credentials */

typedef unsigned int AuthRequest;


#endif /* POOL_TYPE_H */
