#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Assembles a pkgsrc-format binary package for NetBSD 11.1/macppc (or, with the overrides below, another
# NetBSD 11.1 port such as amd64) on a Linux host from the verified
# cross-built executable. This is NOT a pkgsrc `make package` build: it is a package assembled on Linux
# by pkg_create built from the pinned NetBSD 11.1 sources, and is labelled that way in its +BUILD_INFO.
# No G4 access, no installation, no publication.
#   packaging/netbsd-macppc/netbsd-macppc-package.sh SRC TOOLDIR X11_OUT OUT
#     SRC      NetBSD 11.1 source tree (usr/src) extracted by netbsd-macppc-tools.sh
#     TOOLDIR  tooldir/bin of that build (nbcompat headers and library live beside it)
#     X11_OUT  output directory of netbsd-macppc-x11-build.sh (build/ and bin/poodle-earth)
#     OUT      new, empty directory
# Environment: PKG_VERSION (default 0.1.3), SOURCE_DATE_EPOCH (default: the build's source commit time).
# Port overrides, default macppc: NETBSD_TRIPLET (powerpc--netbsd), NETBSD_MACHINE_LABEL (macppc, in the package
# file name and labels), PKG_MACHINE_ARCH (powerpc), PKG_MACHINE_GNU_ARCH (powerpc), X11_EXPECT_CLASS (ELF32),
# X11_EXPECT_DATA (big) and X11_EXPECT_MACHINE (PowerPC), the same variables netbsd-macppc-x11-build.sh reads.
set -euo pipefail
cd "$(dirname "$0")/../.."
repo=$PWD
here=$repo/packaging/netbsd-macppc
src=$(cd "${1:?usage: netbsd-macppc-package.sh SRC TOOLDIR X11_OUT OUT}" && pwd)
tooldir=$(cd "${2:?}/.." && pwd)
x11=$(cd "${3:?}" && pwd)
out=${4:?}
if [ -e "$out" ] && [ -n "$(ls -A "$out")" ]; then echo "refusing: $out is not empty" >&2; exit 2; fi
mkdir -p "$out"/{logs,pkgtool}
out=$(cd "$out" && pwd)
log=$out/logs
version=${PKG_VERSION:-0.1.3}
epoch=${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct "${PR_HEAD_SHA:-HEAD}" 2>/dev/null || git log -1 --format=%ct HEAD)}
exe=$x11/bin/poodle-earth
triplet=${NETBSD_TRIPLET:-powerpc--netbsd}
label=${NETBSD_MACHINE_LABEL:-macppc}
mach_arch=${PKG_MACHINE_ARCH:-powerpc}
mach_gnu_arch=${PKG_MACHINE_GNU_ARCH:-powerpc}
expect_class=${X11_EXPECT_CLASS:-ELF32}
expect_data=${X11_EXPECT_DATA:-big}
expect_machine=${X11_EXPECT_MACHINE:-PowerPC}
strip=$tooldir/bin/$triplet-strip
fails=0
ok() { echo "ok   $*"; }
bad() { echo "FAIL $*"; fails=$((fails + 1)); }
pi=$src/external/bsd/pkg_install
la=$src/external/bsd/libarchive/dist/libarchive
[ -f "$pi/dist/create/main.c" ] || { echo "no pkg_install sources under $src" >&2; exit 1; }

# 1. pkg_create and pkg_info for the Linux host, from the NetBSD 11.1 sources. libfetch and GPG are
#    stubbed (never reached by create/info). libarchive is the host's: headers from the system when
#    installed, else the NetBSD copy. These tools only write and read the archive; they are not pkg_add.
t=$out/pkgtool; mkdir -p "$t"/{obj,info}
if [ -f /usr/include/archive.h ]; then la_inc=; la_ver=$(dpkg-query -W -f='${Version}' libarchive-dev 2>/dev/null || echo system); else la_inc=-I$la; la_ver="NetBSD 11.1 headers"; fi
la_lib=$(ldconfig -p | sed -n 's/.*libarchive\.so\.13 .*=> //p' | head -n 1)
cflags="-g -O1 -w -include $here/pkg/compat-linux.h -DNETBSD -DHAVE_CONFIG_H -DPREFIX=\"/usr/pkg\" -DBINDIR=\"/usr/sbin\" -DSYSCONFDIR=\"/etc\" \
  -I$pi/lib -I$pi/dist/lib -I$pi/dist/create -I$pi/dist/info $la_inc -I$tooldir/include/nbinclude -I$tooldir/include \
  -I$here/pkg -I$tooldir/include/compat -I$src/tools/compat -include $tooldir/include/compat/nbtool_config.h -D_GNU_SOURCE"
trap 'echo "pkg tool build failed:"; tail -n 30 "$log/pkgtool-build.log"' ERR
{
  for f in automatic conflicts dewey fexec file global iterate license lpkg opattern parse-config pkgdb pkg_io plist remove str version var xwrapper pkg_signature; do
    gcc $cflags -c "$pi/dist/lib/$f.c" -o "$t/obj/$f.o"
  done
  gcc $cflags -c "$here/pkg/fetch_stub.c" -o "$t/obj/fetch_stub.o"
  gcc $cflags -c "$here/pkg/sig_stub.c" -o "$t/obj/sig_stub.o"
  for f in main perform pl util build; do gcc $cflags -c "$pi/dist/create/$f.c" -o "$t/obj/c_$f.o"; done
  for f in main perform show; do gcc $cflags -c "$pi/dist/info/$f.c" -o "$t/info/$f.o"; done
  libs="$tooldir/lib/libnbcompat.a $la_lib -lz"
  gcc -o "$t/pkg_create" "$t"/obj/*.o $libs
  gcc -o "$t/pkg_info" "$t"/info/*.o $(ls "$t"/obj/*.o | grep -v '/c_') $libs
  gcc -shared -fPIC -o "$t/pe-epoch.so" "$here/pkg/pe-epoch.c" -ldl
} > "$log/pkgtool-build.log" 2>&1
trap - ERR
ok "pkg_create and pkg_info built (libarchive $la_lib, headers $la_ver)"

# 2. Stage with the project's own install rule at the pkgsrc prefix, then strip as pkgsrc would.
destdir=$out/destdir; stage=$destdir/usr/pkg
DESTDIR=$destdir cmake --install "$x11/build" --prefix /usr/pkg > "$log/install.log" 2>&1
cmp "$stage/bin/poodle-earth" "$exe" && ok "installed executable is byte-identical to the verified one" || bad "installed executable differs from the verified one"
exe_sha=$(sha256sum "$exe" | cut -d' ' -f1)
"$strip" "$stage/bin/poodle-earth"
stripped_sha=$(sha256sum "$stage/bin/poodle-earth" | cut -d' ' -f1)
ok "stripped with $triplet-strip: $exe_sha -> $stripped_sha"
{ echo '@comment $NetBSD$'; ( cd "$stage" && find . -type f | sed 's#^\./##' | LC_ALL=C sort ); } > "$out/PLIST"
find "$stage" -mindepth 1 -type d -empty | grep -q . && bad "empty directory in the stage" || ok "no empty directories in the stage"
if [ -f "$repo/packaging/pkgsrc/geography/poodle-earth/PLIST" ]; then
  cmp "$out/PLIST" "$repo/packaging/pkgsrc/geography/poodle-earth/PLIST" && ok "PLIST equals the pkgsrc recipe's" || bad "PLIST differs from the pkgsrc recipe's"
else
  echo "note no pkgsrc recipe in this tree (#216 unmerged): PLIST generated from the install rule only"
fi
cp "$here/pkg/DESCR" "$out/DESCR"

# 3. +BUILD_INFO. The labelling is deliberate: this is not a pkgsrc-built package.
host="$(. /etc/os-release && echo "$PRETTY_NAME"), $(uname -m), runner image ${ImageVersion:-n/a}"
revision=${PR_HEAD_SHA:-$(git rev-parse HEAD)}
cat > "$out/buildinfo" <<EOB
OPSYS=NetBSD
OS_VERSION=11.1
MACHINE_ARCH=$mach_arch
MACHINE_GNU_ARCH=$mach_gnu_arch
OBJECT_FMT=ELF
LOCALBASE=/usr/pkg
PKGPATH=geography/poodle-earth
PKGTOOLS_VERSION=20091115
CATEGORIES=geography x11
HOMEPAGE=https://github.com/jackwillis/poodle-earth/
LICENSE=gnu-gpl-v3 AND cc0-1.0-universal AND cc-by-v4.0 AND public-domain
CC_VERSION=gcc-$("$tooldir/bin/$triplet-g++" -dumpversion)
BUILD_HOST=$host
EXPERIMENTAL=pkgsrc-format package assembled on Linux by pkg_create built from NetBSD 11.1 sources, from a cross-built executable (source ${revision:0:12}); not a pkgsrc make-package build and not an official package
REQUIRES=/usr/X11R7/lib/libX11.so.7
REQUIRES=/usr/X11R7/lib/libXext.so.7
REQUIRES=/usr/X11R7/lib/libICE.so.7
REQUIRES=/usr/X11R7/lib/libSM.so.7
REQUIRES=/usr/lib/libc.so.12
REQUIRES=/usr/lib/libm.so.0
REQUIRES=/usr/lib/libstdc++.so.9
REQUIRES=/usr/lib/libgcc_s.so.1
EOB

# 4. pkg_create, twice into separate directories. Clock and group database are fixed (pe-epoch.so,
#    SOURCE_DATE_EPOCH) and inputs get the same mtime, so the two archives can be compared.
comment="-Small X11 globe and atlas viewer with built-in data (EXPERIMENTAL $label, Linux-assembled)"
pkgname=poodle-earth-$version-NetBSD-11.1-$label-linux-assembled.tgz
for run in 1 2; do
  d=$out/run$run; mkdir -p "$d"
  find "$stage" "$out/buildinfo" "$out/PLIST" "$out/DESCR" -exec touch -h -d "@$epoch" {} +
  ( cd "$d" && LD_PRELOAD=$t/pe-epoch.so SOURCE_DATE_EPOCH=$epoch "$t/pkg_create" -v -u root -g wheel -c "$comment" \
      -d "$out/DESCR" -f "$out/PLIST" -I /usr/pkg -p "$stage" -B "$out/buildinfo" -F gzip "$d/poodle-earth-$version.tgz" ) > "$log/pkg_create-$run.log" 2>&1 \
    || { tail -n 20 "$log/pkg_create-$run.log"; exit 1; }
done
mkdir -p "$out/package"
cp "$out/run1/poodle-earth-$version.tgz" "$out/package/$pkgname"
pkg=$out/package/$pkgname
cmp "$out/run1/poodle-earth-$version.tgz" "$out/run2/poodle-earth-$version.tgz" && ok "two pkg_create runs are byte-identical" || bad "two pkg_create runs differ"
rm -rf "$out/run1" "$out/run2"
sha=$(sha256sum "$pkg" | cut -d' ' -f1)

# 5. Inspect the package itself.
tarq() { tar --warning=no-unknown-keyword "$@"; }
{ echo "== tar -tvzf"; tarq -tvzf "$pkg"; echo "== +CONTENTS"; tarq -xzOf "$pkg" +CONTENTS; echo "== +COMMENT"; tarq -xzOf "$pkg" +COMMENT; echo
  echo "== pkg_info -L"; "$t/pkg_info" -L "$pkg"; echo "== pkg_info -B"; "$t/pkg_info" -B "$pkg"; } > "$log/package-inspection.txt" 2>&1 || bad "pkg_info could not read the package"
q() { grep -q -E -- "$1" "$log/package-inspection.txt"; }
q '^-r--r--r-- root/wheel .* \+COMMENT$' && ok "+COMMENT is root/wheel 0444" || bad "+COMMENT mode or owner"
q '^-rwxr-xr-x root/wheel .* bin/poodle-earth$' && ok "bin/poodle-earth is root/wheel 0755" || bad "bin/poodle-earth mode or owner"
q '^-rw-r--r-- root/wheel .* share/doc/poodle-earth/LICENSE$' && ok "documents are root/wheel 0644" || bad "document mode or owner"
q "^@name poodle-earth-$version\$" && ok "@name poodle-earth-$version" || bad "@name"
q '^@cwd /usr/pkg$' && ok "@cwd /usr/pkg" || bad "@cwd"
! q '^@pkgdep' && ok "no pkgsrc dependencies (base libraries only)" || bad "unexpected @pkgdep"
q "^MACHINE_ARCH=$mach_arch\$" && q '^OS_VERSION=11\.1$' && q '^OPSYS=NetBSD$' && ok "build info: NetBSD 11.1 $mach_arch" || bad "build info"
q '^EXPERIMENTAL=pkgsrc-format package assembled on Linux' && ok "labelled experimental, Linux-assembled" || bad "label"
files=$(sed -n '/^== pkg_info -L/,/^== pkg_info -B/p' "$log/package-inspection.txt" | grep -v '^==' | grep -v '^Information for' | grep -v '^$' | grep -v '^Files:' | sed 's#^/usr/pkg/##' | LC_ALL=C sort)
[ "$files" = "$(grep -v '^@' "$out/PLIST" | LC_ALL=C sort)" ] && ok "pkg_info -L equals the PLIST" || bad "pkg_info -L differs from the PLIST"
x=$out/extract; mkdir -p "$x"; tarq -xzf "$pkg" -C "$x" bin/poodle-earth
[ "$(sha256sum "$x/bin/poodle-earth" | cut -d' ' -f1)" = "$stripped_sha" ] && ok "packaged executable is the stripped verified executable ($stripped_sha)" || bad "packaged executable hash"
{ readelf -h "$x/bin/poodle-earth"; readelf -l "$x/bin/poodle-earth"; readelf -d "$x/bin/poodle-earth"; readelf -n "$x/bin/poodle-earth"; } > "$log/packaged-readelf.txt"
pr() { grep -q -E -- "$1" "$log/packaged-readelf.txt"; }
pr "Class:[[:space:]]+$expect_class\$" && pr "$expect_data endian" && pr "Machine:[[:space:]]+$expect_machine\$" && pr 'interpreter: /usr/libexec/ld\.elf_so\]' && ok "packaged ELF: $expect_class $expect_data-endian $expect_machine, /usr/libexec/ld.elf_so" || bad "packaged ELF header"
pr 'IDENT 1101000000 \(11\.1\.0\)' && ok "packaged executable carries the NetBSD 11.1.0 note" || bad "NetBSD note"
dyn() { grep -E 'NEEDED|RPATH' "$1" | sed 's/^ *0x[0-9a-f]* //' | LC_ALL=C sort; }
[ "$(dyn "$log/packaged-readelf.txt")" = "$(dyn "$x11/logs/readelf.txt")" ] && ok "NEEDED and RPATH unchanged by strip" || bad "NEEDED/RPATH changed by strip"
rm -rf "$x"

{
  echo "## NetBSD 11.1/$label package (Linux-assembled, pkgsrc format)"
  echo
  echo "- result: $([ "$fails" -eq 0 ] && echo PASS || echo "FAIL ($fails checks)")"
  echo "- revision: $(git rev-parse HEAD) (PR head ${PR_HEAD_SHA:-n/a}); SOURCE_DATE_EPOCH $epoch"
  echo "- package: $(basename "$pkg"), $(stat -c %s "$pkg") bytes, sha256 $sha"
  echo "- verified executable (input): sha256 $exe_sha; packaged (stripped): sha256 $stripped_sha"
  echo "- pkg_create: built from NetBSD 11.1 pkg_install sources; libarchive $la_lib ($la_ver); libfetch and GPG stubbed"
  echo "- label: not a pkgsrc make-package build; not installed or run anywhere"
  echo
  echo '```'
  sed -n '/^== tar -tvzf/,/^== +CONTENTS/p' "$log/package-inspection.txt" | grep -v '^=='
  echo '```'
} | tee "$out/summary.md"
[ "$fails" -eq 0 ] || echo "$fails checks failed" >&2
exit $([ "$fails" -eq 0 ] && echo 0 || echo 1)
