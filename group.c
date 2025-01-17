/*
Based on parts of the GNU C Library:

   Common code for file-based database parsers in nss_files module.
   Copyright (C) 1996, 1997, 1998, 1999, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
*/
#define _GNU_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <nss.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <grp.h>

#include "config.h"
#include "redis_client.h"

enum nss_status _nss_github_setgrent(void);
enum nss_status _nss_github_endgrent(void);
enum nss_status _nss_github_getgrent_r(struct group *gr, char *buffer, size_t buflen, int *errnop);
enum nss_status _nss_github_getgrnam_r(const char *name, struct group *gr, char *buffer, size_t buflen, int *errnop);
enum nss_status _nss_github_getgrgid_r(const gid_t gid, struct group *gr, char *buffer, size_t buflen, int *errnop);


/* from clib/nss */
static inline char **parse_list(char *line, char *data, size_t datalen, int *errnop) {
	char *eol, **list, **p;

	if (line >= data && line < (char *) data + datalen)
		/* Find the end of the line buffer, we will use the space in DATA after
		 *        it for storing the vector of pointers.  */
		eol = strchr(line, '\0') + 1;
	else
		/* LINE does not point within DATA->linebuffer, so that space is
		 *        not being used for scratch space right now.  We can use all of
		 *               it for the pointer vector storage.  */
		eol = data;
	/* Adjust the pointer so it is aligned for storing pointers.  */
	eol += __alignof__(char *) - 1;
	eol -= (eol - (char *)0) % __alignof__(char *);
	/* We will start the storage here for the vector of pointers.  */
	list = (char **)eol;

	p = list;
	while (1)
	{
		char *elt;

		if ((size_t) ((char *)&p[1] - (char *)data) > datalen)
		{
			/* We cannot fit another pointer in the buffer.  */
			*errnop = ERANGE;
			return NULL;
		}
		if (*line == '\0')
			break;

		/* Skip leading white space.  This might not be portable but useful.  */
		while (isspace(*line))
			++line;

		elt = line;
		while (1) {
			if (*line == '\0' || *line == ',' ) {
				/* End of the next entry.  */
				if (line > elt)
					/* We really found some data.  */
					*p++ = elt;

				/* Terminate string if necessary.  */
				if (*line != '\0')
					*line++ = '\0';
				break;
			}
			++line;
		}
	}
	*p = NULL;

	return list;
}

static inline enum nss_status g_search(const char *name, const gid_t gid, struct group *gr, int *errnop, char *buffer, size_t buflen) {

	char *gr_data = buffer;
	char *token;
	const char *delim = ":";
	char *tenant = NULL;
	int res = 0;

#if USE_TENANT
    tenant = getenv("TENANT");
	if (tenant == NULL)
		return NSS_STATUS_UNAVAIL;
#endif


	if (gid != 0 && gid < MINGID) {
		*errnop = ENOENT;
		return NSS_STATUS_NOTFOUND;
	}

	memset(gr_data, 0, buflen);

    if (gid == 0) {
		if (name == NULL)
		    return NSS_STATUS_UNAVAIL; 
        res = redis_lookup("GROUP", tenant, name, gr_data);
    } else {
        char *_name = malloc(100);
        sprintf(_name, "%d", gid);
        res = redis_lookup("GROUP", tenant, _name, gr_data); 
		free(_name);
    }
       
	if (res > 0)
		return NSS_STATUS_NOTFOUND;
	
	*errnop = 0;

	// gr_name : string
    token = strtok(gr_data, delim);
	gr->gr_name = (char*) malloc(strlen(token)+1);
    strncpy(gr->gr_name, token, strlen(token)+1);

	// gr_passwd : string
    token = strtok(NULL, delim);
	gr->gr_passwd = (char*) malloc(strlen(token)+1);
    strncpy(gr->gr_passwd, token, strlen(token)+1);

	//  gr_gid : int
    token = strtok(NULL, delim);
	gr->gr_gid = atoi(token);

	// gr_mem : list of strings
    token = strtok(NULL, delim);
	char **list = parse_list (token, token, buflen, errnop);
	if (list)
	    gr->gr_mem = list;
	else
    	gr->gr_mem = NULL;

	return NSS_STATUS_SUCCESS;
}


enum nss_status _nss_github_setgrent(void) {
	return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_github_endgrent(void) {
	return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_github_getgrent_r(struct group *gr, char *buffer, size_t buflen, int *errnop) {
	*errnop = 0;
	return g_search(NULL, 0, gr, errnop, buffer, buflen);
}


enum nss_status _nss_github_getgrnam_r(const char *name, struct group *gr, char *buffer, size_t buflen, int *errnop) {
	enum nss_status e;
	*errnop = 0;

	if (gr == NULL || name == NULL)
		return NSS_STATUS_UNAVAIL;

	e = g_search(name, 0, gr, errnop, buffer, buflen);
	return e;
}

enum nss_status _nss_github_getgrgid_r(const gid_t gid, struct group *gr, char *buffer, size_t buflen, int *errnop) {
	enum nss_status e;
	*errnop = 0;
	
	if (gr == NULL)
		return NSS_STATUS_UNAVAIL;
	if (gid == 0 || gid < MINGID)
		return NSS_STATUS_NOTFOUND;

	e = g_search(NULL, gid, gr, errnop, buffer, buflen);
	return e;
}
