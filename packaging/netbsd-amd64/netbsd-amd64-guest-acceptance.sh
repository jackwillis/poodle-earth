#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Guest acceptance of the Linux-built NetBSD 11.1/amd64 package, run INSIDE a NetBSD 11.1/amd64 guest
# from a checkout of the revision that built the package.
#   sh packaging/netbsd-amd64/netbsd-amd64-guest-acceptance.sh PACKAGE EXPECTED_SHA256 OUTDIR
# PACKAGE is the exact file the Linux job produced; EXPECTED_SHA256 is the digest that job recorded. Nothing
# is rebuilt here: the package is verified, installed, smoke-tested with the repository's X11 pixel test under
# Xvfb at depth 24 and 16, removed, and the package database checked. Exit 0 only if every step passed.
# Package correctness and a headless readback oracle only: no real display, no timing, no hardware.
# Prerequisites (installed by the workflow's prepare step): the X11 sets, python313.
set -u
PATH=/usr/pkg/bin:/usr/pkg/sbin:/usr/X11R7/bin:/sbin:/usr/sbin:/bin:/usr/bin
export PATH
[ $# = 3 ] || { echo "usage: $0 PACKAGE EXPECTED_SHA256 OUTDIR" >&2; exit 2; }
SRC=$(pwd); PKG=$SRC/$1; WANT=$2; O=$SRC/$3; fail=0
rm -rf "$O"; mkdir -p "$O"
# chk STATUS LABEL [LOG]: on failure, show the tail of LOG in the job output (the VM step's files are not
# copied back when it fails).
chk() {
	if [ "$1" = 0 ]; then r=PASS; else r=FAIL; fail=1; fi
	echo "$r $2" | tee -a "$O/summary.txt"
	if [ "$r" = FAIL ] && [ -n "${3:-}" ]; then echo "---- tail of $3"; tail -n 40 "$3"; echo "----"; fi
}
sha() { sha256 "$1" | sed 's/^.*= //'; }

{ uname -a; uname -s; uname -r; uname -m; gcc --version | head -n 1; python3.13 --version; pkg_info -E python313; ls -l /usr/X11R7/lib/libX11.so.7; } > "$O/00-environment.log" 2>&1
cat "$O/00-environment.log"
[ "$(uname -s)" = NetBSD ] && [ "$(uname -m)" = amd64 ] && case $(uname -r) in 11.1*) true ;; *) false ;; esac
chk $? "guest is NetBSD 11.1 amd64 ($(uname -srm))" "$O/00-environment.log"

GOT=$(sha "$PKG")
echo "package $(basename "$PKG") sha256 $GOT (expected $WANT)" | tee "$O/01-package-sha256.log"
[ "$GOT" = "$WANT" ]; chk $? "package sha256 in the guest equals the Linux job's"

{ pkg_info -B "$PKG"; pkg_info -L "$PKG"; } > "$O/02-package-metadata.log" 2>&1
grep -q '^OPSYS=NetBSD$' "$O/02-package-metadata.log" && grep -q '^OS_VERSION=11\.1$' "$O/02-package-metadata.log" \
    && grep -q '^MACHINE_ARCH=x86_64$' "$O/02-package-metadata.log"
chk $? "package build info is NetBSD 11.1 x86_64" "$O/02-package-metadata.log"

pkg_info -qL "$PKG" | sed 's|^/usr/pkg/||' | sort > "$O/pl.package"
cat > "$O/pl.expected" <<'EOP'
bin/poodle-earth
share/doc/poodle-earth/DATA-CREDITS.txt
share/doc/poodle-earth/LANDCLASS-NOTICE.txt
share/doc/poodle-earth/LICENSE
share/doc/poodle-earth/README.md
EOP
sort -o "$O/pl.expected" "$O/pl.expected"
diff "$O/pl.expected" "$O/pl.package" > "$O/03-plist.diff" 2>&1; chk $? "package contents are the five expected files" "$O/03-plist.diff"

pkg_info -E poodle-earth > /dev/null 2>&1; [ $? != 0 ]; chk $? "poodle-earth is not installed before the test"
find /usr/pkg -type f | sort > "$O/before.txt"
pkg_add "$PKG" > "$O/04-pkg_add.log" 2>&1; chk $? "pkg_add of the exact package" "$O/04-pkg_add.log"
B=/usr/pkg/bin/poodle-earth
{ pkg_info -e poodle-earth; pkg_info -E poodle-earth; sha "$B"; file "$B"; readelf -h "$B"; readelf -d "$B" | grep -E 'NEEDED|RPATH'; readelf -p .comment "$B"; ldd "$B"; } > "$O/05-binary.log" 2>&1
if ldd "$B" 2>&1 | grep -qi "not found"; then chk 1 "all shared libraries resolve" "$O/05-binary.log"; else chk 0 "all shared libraries resolve"; fi

for d in 24 16; do
	Xvfb ":7$d" -screen 0 "800x600x$d" -nolisten tcp > "$O/xvfb$d.log" 2>&1 & XP=$!; sleep 3
	DISPLAY=":7$d" python3.13 "$SRC/tests/test_x11_pixels.py" "$d" "$B" --size 161x121 \
	    --script "$SRC/tests/data/trace1.txt" --exit-when-done --verify-pixels --stats \
	    > "$O/06-trace-depth$d.json" 2> "$O/06-trace-depth$d.err"
	chk $? "X11 pixel test at depth $d (visual depth, frames > 100, verified frames == presentations)" "$O/06-trace-depth$d.err"
	kill $XP; wait $XP 2>/dev/null
done

pkg_delete poodle-earth > "$O/07-pkg_delete.log" 2>&1; chk $? "pkg_delete" "$O/07-pkg_delete.log"
find /usr/pkg -type f | sort > "$O/after.txt"; diff "$O/before.txt" "$O/after.txt" > "$O/08-residual.diff" 2>&1; chk $? "no files left behind in /usr/pkg" "$O/08-residual.diff"
[ ! -e "$B" ] && [ ! -e /usr/pkg/share/doc/poodle-earth ]; chk $? "executable and document directory are gone"
pkg_admin check > "$O/09-pkg_admin-check.log" 2>&1; chk $? "pkg_admin check" "$O/09-pkg_admin-check.log"
{ pkg_info -E python313; pkg_info -E poodle-earth || echo "poodle-earth not installed"; } > "$O/10-final-state.log" 2>&1
echo "overall=$fail (0 = all pass)" | tee -a "$O/summary.txt"
exit $fail
