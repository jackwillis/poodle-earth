/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Jack Willis. Build-host stub for the Linux-built pkg_create; never installed. */
/* Stub: all network operations fail. */
#include <stdlib.h>
#include <string.h>
#include "fetch.h"
char fetchLastErrString[256] = "network access not built in this pkg_create";
int fetchLastErrCode = 1;
void fetchConnectionCacheInit(int a, int b) { (void)a; (void)b; }
struct url *fetchParseURL(const char *s) { (void)s; return NULL; }
struct url *fetchCopyURL(const struct url *u) { (void)u; return NULL; }
char *fetchStringifyURL(const struct url *u) { (void)u; return NULL; }
void fetchFreeURL(struct url *u) { free(u); }
void fetchInitURLList(struct url_list *l) { memset(l, 0, sizeof *l); }
void fetchFreeURLList(struct url_list *l) { free(l->urls); memset(l, 0, sizeof *l); }
int fetchList(struct url_list *l, struct url *u, const char *p, const char *f) { (void)l; (void)u; (void)p; (void)f; return -1; }
fetchIO *fetchXGet(struct url *u, struct url_stat *s, const char *f) { (void)u; (void)s; (void)f; return NULL; }
ssize_t fetchIO_read(fetchIO *io, void *b, size_t n) { (void)io; (void)b; (void)n; return -1; }
void fetchIO_close(fetchIO *io) { (void)io; }
char *fetchUnquoteFilename(struct url *u) { (void)u; return NULL; }
