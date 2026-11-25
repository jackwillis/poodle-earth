/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Jack Willis
 * LD_PRELOAD shim that makes pkg_create's output independent of the build host's clock and groups:
 *  - time() returns $SOURCE_DATE_EPOCH (required);
 *  - getgrnam("wheel") returns gid 0 (NetBSD numbering); other names pass through to libc.
 * It changes only what pkg_create records, never the staged file contents. */
#define _GNU_SOURCE
#include <time.h>
#include <grp.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
time_t time(time_t *t) {
    const char *e = getenv("SOURCE_DATE_EPOCH");
    time_t v = e ? (time_t)strtoll(e, NULL, 10) : 0;
    if (t) *t = v;
    return v;
}
struct group *getgrnam(const char *name) {
    static struct group g; static char *mem[1] = { NULL };
    if (name && strcmp(name, "wheel") == 0) { g.gr_name = (char *)"wheel"; g.gr_passwd = (char *)"*"; g.gr_gid = 0; g.gr_mem = mem; return &g; }
    struct group *(*real)(const char *) = dlsym(RTLD_NEXT, "getgrnam");
    return real(name);
}
