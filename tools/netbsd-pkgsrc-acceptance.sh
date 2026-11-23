#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Package acceptance for geography/poodle-earth, run INSIDE a NetBSD 11.1/amd64 guest (CI) from a
# checkout. Usage: tools/netbsd-pkgsrc-acceptance.sh DISTFILE.tar.gz OUTDIR
# DISTFILE: `git archive --prefix=poodle-earth-VERSION/ HEAD | gzip -n`, VERSION as in the recipe.
# Exit 0 only if every step passed. Package correctness and a headless readback oracle only:
# no real display, no timing claim, no hardware. Adapted from the local fixture runbook script.
set -u
PATH=/usr/pkg/bin:/usr/pkg/sbin:/usr/X11R7/bin:/sbin:/usr/sbin:/bin:/usr/bin
export PATH
[ $# = 2 ] || { echo "usage: $0 DISTFILE OUTDIR" >&2; exit 2; }
SRC=$(pwd); DIST=$SRC/$1; O=$SRC/$2; fail=0; mkdir -p "$O"
PKGSRC=${PKGSRC:-/usr/pkgsrc}; R=$PKGSRC/geography/poodle-earth; PK=$PKGSRC/packages/All
MAKE=$(command -v bmake || echo make)
# chk STATUS LABEL [LOG]: on failure, show the tail of LOG in the job output (the VM step's
# artifacts are not copied back when it fails).
chk() {
	if [ "$1" = 0 ]; then r=PASS; else r=FAIL; fail=1; fi
	echo "$r $2" | tee -a "$O/summary.txt"
	if [ "$r" = FAIL ] && [ -n "${3:-}" ]; then echo "---- tail of $3"; tail -n 40 "$3"; echo "----"; fi
}

{ uname -a; gcc --version | head -1; sha256 "$DIST"; pkg_info -E cmake python313 ninja-build pkglint; } > "$O/00-environment.log" 2>&1
rm -rf "$R"; mkdir -p "$R" "$PKGSRC/distfiles"; cp "$SRC"/packaging/pkgsrc/geography/poodle-earth/* "$R/"
DF=$(cd "$R" && $MAKE show-var VARNAME=DISTFILES)
cp "$DIST" "$PKGSRC/distfiles/$DF"
cd "$R" || exit 1
$MAKE makesum > "$O/01-makesum.log" 2>&1; chk $? "makesum ($DF)" "$O/01-makesum.log"
pkglint -Wall > "$O/02-pkglint.log" 2>&1; chk $? "pkglint -Wall (exit status; warnings are in the log)"
# Install dependencies first so the residue check sees only what poodle-earth itself leaves behind.
$MAKE install-dependencies > "$O/02b-dependencies.log" 2>&1; chk $? "install-dependencies" "$O/02b-dependencies.log"
find /usr/pkg -type f | sort > "$O/before.txt"
( time $MAKE package ) > "$O/03-package-build.log" 2>&1; chk $? "make package" "$O/03-package-build.log"
P=$(ls "$PK"/poodle-earth-*.tgz 2>/dev/null | head -1)
[ -n "$P" ] || { echo "FAIL no package built" | tee -a "$O/summary.txt"; echo "overall=1" | tee -a "$O/summary.txt"; exit 1; }
{ sha256 "$P"; pkg_info -B "$P"; pkg_info -L "$P"; pkg_info -N "$P"; } > "$O/04-package-metadata.log" 2>&1
pkg_info -qL "$P" | sed 's|^/usr/pkg/||' | sort > "$O/pl.actual"; grep -v '^@' PLIST | sort > "$O/pl.recipe"
diff "$O/pl.recipe" "$O/pl.actual" > "$O/05-plist.diff" 2>&1; chk $? "PLIST matches package contents" "$O/05-plist.diff"
pkg_add "$P" > "$O/06-pkg_add.log" 2>&1; chk $? "pkg_add" "$O/06-pkg_add.log"
B=/usr/pkg/bin/poodle-earth
{ file $B; readelf -d $B | grep -E 'NEEDED|RPATH'; ldd $B; } > "$O/07-binary.log" 2>&1
if ldd $B 2>&1 | grep -qi "not found"; then chk 1 "all shared libraries resolve"; else chk 0 "all shared libraries resolve"; fi
for d in 24 16; do
	Xvfb :7$d -screen 0 800x600x$d -nolisten tcp > "$O/xvfb$d.log" 2>&1 & XP=$!; sleep 3
	DISPLAY=:7$d python3.13 "$SRC/tests/test_x11_pixels.py" $d $B --size 161x121 \
	    --script "$SRC/tests/data/trace1.txt" --exit-when-done --verify-pixels --stats \
	    > "$O/08-trace-depth$d.json" 2> "$O/08-trace-depth$d.err"
	chk $? "semantic+readback oracle at depth $d (verified frames == presentations, frames > 100)" "$O/08-trace-depth$d.err"
	kill $XP; wait $XP 2>/dev/null
done
pkg_delete poodle-earth > "$O/09-pkg_delete.log" 2>&1; chk $? "pkg_delete" "$O/09-pkg_delete.log"
find /usr/pkg -type f | sort > "$O/after.txt"; diff "$O/before.txt" "$O/after.txt" > "$O/10-residual.diff"; chk $? "no files left behind" "$O/10-residual.diff"
pkg_admin check > "$O/11-pkg_admin-check.log" 2>&1; chk $? "pkg_admin check" "$O/11-pkg_admin-check.log"
echo "overall=$fail (0 = all pass)" | tee -a "$O/summary.txt"; exit $fail
