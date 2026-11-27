/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Jack Willis */
/* Linux shim for NetBSD pkg_install sources: cdefs macros that glibc lacks. */
#ifndef PE_LINUX_SHIM
#define PE_LINUX_SHIM
#define __RCSID(x)
#define __COPYRIGHT(x)
#define __KERNEL_RCSID(a,b)
#define __dead __attribute__((noreturn))
#define __printflike(a,b) __attribute__((format(printf,a,b)))
#define __unused __attribute__((unused))
#define __UNCONST(a) ((void *)(unsigned long)(const void *)(a))
#endif
