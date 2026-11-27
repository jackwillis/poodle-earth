/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Jack Willis. Build-host stub for the Linux-built pkg_create; never installed. */
/* Stub: libfetch is not built. pkg_create never reaches network code; every stub fails. */
#ifndef PE_STUB_FETCH_H
#define PE_STUB_FETCH_H
#include <sys/types.h>
#include <stdio.h>
#include <time.h>
struct url { char scheme[16]; char user[256]; char pwd[256]; char host[256]; int port; char *doc; off_t offset; size_t length; time_t last_modified; };
struct url_stat { int  eof; off_t size; time_t atime; time_t mtime; };
struct url_ent { char name[256]; struct url_stat stat; };
struct url_list { size_t length, alloc_size; struct url *urls; };
typedef struct fetchconn fetchIO;
extern char fetchLastErrString[];
extern int fetchLastErrCode;
void fetchConnectionCacheInit(int, int);
struct url *fetchParseURL(const char *);
struct url *fetchCopyURL(const struct url *);
char *fetchStringifyURL(const struct url *);
void fetchFreeURL(struct url *);
void fetchInitURLList(struct url_list *);
void fetchFreeURLList(struct url_list *);
int fetchList(struct url_list *, struct url *, const char *, const char *);
fetchIO *fetchXGet(struct url *, struct url_stat *, const char *);
ssize_t fetchIO_read(fetchIO *, void *, size_t);
void fetchIO_close(fetchIO *);
char *fetchUnquoteFilename(struct url *);
#endif
