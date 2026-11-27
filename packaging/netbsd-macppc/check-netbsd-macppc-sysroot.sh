#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Checks that an assembled NetBSD 11.1/macppc sysroot has what a later dynamic-X11 cross build
# needs: target headers, startup objects, C/C++ runtime libraries and the X11 libraries, all
# big-endian ELF32 PowerPC marked NetBSD 11.1. Needs readelf. Prints ok/FAIL lines; exits 1 on any FAIL.
#   packaging/netbsd-macppc/check-netbsd-macppc-sysroot.sh SYSROOT
# The defaults describe macppc. The amd64 workflow sets SYSROOT_EXPECT_CLASS=ELF64, SYSROOT_EXPECT_DATA=little,
# SYSROOT_EXPECT_MACHINE="Advanced Micro Devices X86-64" and SYSROOT_MD_DIR=amd64.
set -uo pipefail
root=$(cd "${1:?usage: check-netbsd-macppc-sysroot.sh SYSROOT}" && pwd)
fails=0
expect_class=${SYSROOT_EXPECT_CLASS:-ELF32}
expect_data=${SYSROOT_EXPECT_DATA:-big}
expect_machine=${SYSROOT_EXPECT_MACHINE:-PowerPC}
md_dir=${SYSROOT_MD_DIR:-powerpc}
ok() { echo "ok   $*"; }
bad() { echo "FAIL $*"; fails=$((fails + 1)); }

# Resolve a sysroot-relative path as the target would: absolute symlinks are rooted at $root.
resolve() {
  local p=$1 n=0 t
  while [ -L "$root/$p" ]; do
    n=$((n + 1)); [ "$n" -gt 20 ] && return 1
    t=$(readlink "$root/$p")
    case $t in
      /*) p=${t#/} ;;
      *) p=$(dirname "$p")/$t ;;
    esac
    p=$(realpath -m --relative-to="$root" --no-symlinks "$root/$p")
  done
  [ -e "$root/$p" ] && echo "$p"
}
# Every path component may itself be a symlink (usr/include/machine -> macppc).
real() { local out; out=$(chroot_resolve "$1") && echo "$out"; }
chroot_resolve() {
  local rest=${1#/} cur="" part
  while [ -n "$rest" ]; do
    part=${rest%%/*}; [ "$part" = "$rest" ] && rest="" || rest=${rest#*/}
    cur=${cur:+$cur/}$part
    if [ -L "$root/$cur" ]; then cur=$(resolve "$cur") || return 1; fi
  done
  [ -e "$root/$cur" ] && echo "$cur"
}

elf_ok() { # file: the expected class, byte order and machine
  local h; h=$(readelf -h "$root/$1" 2>&1) || { bad "$1: not readable ELF"; return; }
  echo "$h" | grep -q "Class: *$expect_class\$" && echo "$h" | grep -q "$expect_data endian" && echo "$h" | grep -q "Machine: *$expect_machine\$" \
    && ok "$expect_class $expect_data-endian $expect_machine: $1" || bad "$1: not $expect_class $expect_data-endian $expect_machine"
}
note_ok() { # file carries NetBSD IDENT 1101000000 (11.1.0)
  readelf -n "$root/$1" 2>/dev/null | grep -q 'IDENT 1101000000 (11.1.0)' && ok "NetBSD 11.1.0 note: $1" || bad "$1: no NetBSD 11.1.0 IDENT note"
}
need() { # path [elf] [note]
  local r; r=$(real "$1") || { bad "missing: $1"; return; }
  ok "present: $1$([ "$r" != "$1" ] && echo " -> $r")"
  [ -n "${2:-}" ] && elf_ok "$r"
  [ -n "${3:-}" ] && note_ok "$r"
  return 0
}

# Release identity.
grep -q '#define[[:space:]]*__NetBSD_Version__[[:space:]]*1101000000' "$root/usr/include/sys/param.h" \
  && ok "usr/include/sys/param.h: __NetBSD_Version__ 1101000000" || bad "param.h is not NetBSD 11.1"

# Target headers: C, machine-dependent, C++ library, X11.
for h in stdio.h stdlib.h string.h unistd.h pthread.h sys/types.h sys/param.h machine/types.h $md_dir/types.h g++/vector g++/array; do
  need "usr/include/$h"
done
for h in Xlib.h Xutil.h Xatom.h keysym.h cursorfont.h extensions/XShm.h; do
  need "usr/X11R7/include/X11/$h"
done

# Startup objects.
for o in crt0.o crti.o crtn.o crtbegin.o crtend.o crtbeginS.o crtendS.o crtbeginT.o; do
  need "usr/lib/$o" elf
done

# Link-time and run-time libraries. .so dev links must resolve inside the sysroot.
for l in usr/lib/libc.so usr/lib/libm.so usr/lib/libpthread.so usr/lib/libstdc++.so usr/lib/libgcc_s.so \
         usr/X11R7/lib/libX11.so usr/X11R7/lib/libXext.so usr/X11R7/lib/libSM.so usr/X11R7/lib/libICE.so \
         usr/X11R7/lib/libxcb.so usr/X11R7/lib/libXau.so usr/X11R7/lib/libXdmcp.so; do
  need "$l" elf note
done
for a in usr/lib/libc.a usr/lib/libm.a usr/lib/libstdc++.a usr/lib/libgcc.a usr/lib/libsupc++.a usr/X11R7/lib/libX11.a; do
  need "$a"
done
need libexec/ld.elf_so elf note

# Every DT_NEEDED of the X11 libraries resolves inside the sysroot, and records the run-time path.
for l in usr/X11R7/lib/libX11.so usr/X11R7/lib/libXext.so usr/X11R7/lib/libSM.so usr/X11R7/lib/libICE.so usr/lib/libstdc++.so; do
  r=$(real "$l") || continue
  for n in $(readelf -d "$root/$r" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
    found=""
    for d in usr/lib usr/X11R7/lib lib; do [ -e "$root/$d/$n" ] && found=$d && break; done
    [ -n "$found" ] && ok "needed by $(basename "$l"): $n in $found" || bad "needed by $(basename "$l"): $n not found"
  done
done
readelf -d "$root/$(real usr/X11R7/lib/libX11.so)" 2>/dev/null | grep -q 'RPATH.*\[/usr/X11R7/lib\]' \
  && ok "libX11 RPATH /usr/X11R7/lib" || echo "note libX11 has no RPATH /usr/X11R7/lib"

echo "note absolute symlinks are rooted at the sysroot; $(find "$root" -type l -lname '/*' | wc -l) of $(find "$root" -type l | wc -l) symlinks are absolute"
if [ "$fails" -eq 0 ]; then echo "ok   sysroot check passed"; else echo "FAIL $fails check(s) failed"; fi
[ "$fails" -eq 0 ]
