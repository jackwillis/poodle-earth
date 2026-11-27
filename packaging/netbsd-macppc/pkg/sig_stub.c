/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Jack Willis. Build-host stub for the Linux-built pkg_create; never installed. */
/* Stub: GPG signing/verification is not built; pkg_create is used without -S/signing. */
#include <stddef.h>
int detached_gpg_sign(const char *a, const char *b, const char *c, const char *d, const char *e) { (void)a;(void)b;(void)c;(void)d;(void)e; return -1; }
int gpg_verify(const char *a, size_t b, const char *c, const char *d, size_t e) { (void)a;(void)b;(void)c;(void)d;(void)e; return -1; }
