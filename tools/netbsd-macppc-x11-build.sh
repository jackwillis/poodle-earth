#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# Cross-builds the dynamic X11 app (poodle-earth-x11) for NetBSD 11.1/macppc on a Linux host with the
# tools from tools/netbsd-macppc-tools.sh and the sysroot from tools/netbsd-macppc-sysroot.sh, then
# inspects the executable. No package assembly, no G4 access, no publication.
#   tools/netbsd-macppc-x11-build.sh TOOLS_BIN SYSROOT OUT
# OUT must not exist or must be empty; the executable lands in OUT/bin, logs in OUT/logs.
set -euo pipefail
cd "$(dirname "$0")/.."
repo=$PWD
tools=$(cd "${1:?usage: netbsd-macppc-x11-build.sh TOOLS_BIN SYSROOT OUT}" && pwd)
sysroot=$(cd "${2:?}" && pwd)
out=${3:?}
if [ -e "$out" ] && [ -n "$(ls -A "$out")" ]; then
  echo "refusing: $out is not empty (this job must start cold)" >&2
  exit 2
fi
mkdir -p "$out"/{logs,bin}
out=$(cd "$out" && pwd)
log=$out/logs
build=$out/build
python=$(command -v python3)
export GLOBE_NETBSD_TOOLS=$tools GLOBE_NETBSD_SYSROOT=$sysroot
fails=0
ok() { echo "ok   $*"; }
bad() { echo "FAIL $*"; fails=$((fails + 1)); }

# 1. Configure and build. -O2 -DNDEBUG are explicit: CMake's own Release default is -O3.
flags="-O2 -DNDEBUG"
{
  cmake -S . -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$repo/tools/netbsd-macppc-x11-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="$flags" \
    -DPython3_EXECUTABLE="$python" -DGLOBE_BUILD_TESTS=OFF
} > "$log/configure.log" 2>&1 || { tail -n 40 "$log/configure.log"; exit 1; }
set +e
ninja -C "$build" -v poodle-earth-x11 > "$log/build.log" 2>&1
status=$?
set -e
tail -n 5 "$log/build.log"
[ "$status" -eq 0 ] || exit "$status"
exe=$build/poodle-earth
cp "$exe" "$out/bin/poodle-earth"
exe=$out/bin/poodle-earth

# 2. Flags and host leakage in the compile and link lines.
grep -q 'CMAKE_CXX_FLAGS_RELEASE:STRING=-O2 -DNDEBUG$' "$build/CMakeCache.txt" && ok "cache has -O2 -DNDEBUG" || bad "cache flags"
if grep -E -- ' -O[13s0] ' "$log/build.log" | grep -q -- '-DNDEBUG'; then bad "another -O level on a compile line"; else ok "no other -O level on compile lines"; fi
n=$(grep -c -- ' -O2 -DNDEBUG ' "$log/build.log" || true); [ "$n" -gt 0 ] && ok "$n compile lines carry -O2 -DNDEBUG" || bad "no compile line carries -O2 -DNDEBUG"
n=$(grep -c -- "--sysroot=$sysroot" "$log/build.log" || true); ok "$n lines pass --sysroot=<sysroot>"
if grep -E -- '-I/usr/(include|X11R7)|-L/usr/|-I/usr/local|-I/opt' "$log/build.log" | grep -v -- "$sysroot" | grep -q .; then bad "host include or library path on a build line"; else ok "no host -I/-L paths"; fi
grep -q "^Python3_EXECUTABLE:[A-Z]*=$python\$" "$build/CMakeCache.txt" && ok "build-time generator is the runner's Python ($python)" || bad "Python"
grep -q "X11_X11_INCLUDE_PATH:PATH=$sysroot/" "$build/CMakeCache.txt" && ok "X11 headers from the sysroot" || bad "X11 headers not from the sysroot"
grep -E '^X11_X11_LIB' "$build/CMakeCache.txt" | grep -q "=$sysroot/" && ok "libX11 from the sysroot" || bad "libX11 not from the sysroot"

# 3. Absolute symlinks. The link libraries must not need the host to resolve one: list every absolute
#    symlink in the sysroot, then re-run the link with -Wl,-t and require every input to sit under the
#    sysroot with no absolute symlink on the way.
find "$sysroot" -type l -lname '/*' | sort > "$log/absolute-symlinks.txt"
echo "$(wc -l < "$log/absolute-symlinks.txt") absolute symlinks in the sysroot (see absolute-symlinks.txt)"
grep -E "^$sysroot/(usr/lib|lib|usr/X11R7/lib)/" "$log/absolute-symlinks.txt" | grep -E '\.(so|a)(\.[0-9.]+)?$' > "$log/absolute-library-symlinks.txt" || true
[ -s "$log/absolute-library-symlinks.txt" ] && bad "absolute symlinks among library files (absolute-library-symlinks.txt)" || ok "no absolute symlinks among the libraries in lib, usr/lib, usr/X11R7/lib"
linkcmd=$(ninja -C "$build" -t commands poodle-earth-x11 | tail -n 1)
linkcmd=${linkcmd#: && }; linkcmd=${linkcmd% && :}
( cd "$build" && eval "$linkcmd -Wl,-t -Wl,--verbose -o $out/relink-probe" ) > "$log/link-trace.log" 2>&1 || bad "relink with -Wl,-t failed"
rm -f "$out/relink-probe"
grep -E '^(/|\(/|-l)' "$log/link-trace.log" | sed -E 's/^-l[^ ]+ \((.*)\)$/\1/; s/^\((.*)\)$/\1/' | grep -E '\.(a|so[0-9.]*|o)$' | sort -u > "$log/link-inputs.txt" || true
while read -r f; do
  case $f in
    "$sysroot"/*|"$build"/*) ;;
    *) bad "link input outside sysroot and build dir: $f" ;;
  esac
  d=$f
  while [ "$d" != / ] && [ "$d" != "$sysroot" ]; do
    [ -L "$d" ] && case $(readlink "$d") in /*) bad "absolute symlink on link input path: $d" ;; esac
    d=$(dirname "$d")
  done
done < "$log/link-inputs.txt"
n=$(wc -l < "$log/link-inputs.txt")
[ "$n" -gt 10 ] && ok "$n link inputs checked, all under the sysroot or build dir" || bad "link trace found only $n inputs"

# 4. The executable.
{
  echo "== readelf -h"; readelf -h "$exe"
  echo "== readelf -l"; readelf -l "$exe"
  echo "== readelf -d"; readelf -d "$exe"
  echo "== readelf -n"; readelf -n "$exe"
  echo "== readelf -p .comment"; readelf -p .comment "$exe"
  echo "== sha256 / size"; sha256sum "$exe"; stat -c %s "$exe"
} > "$log/readelf.txt" 2>&1
r() { grep -q -E "$1" "$log/readelf.txt"; }
r 'Class:[[:space:]]+ELF32' && ok "ELF32" || bad "not ELF32"
r "Data:[[:space:]]+2's complement, big endian" && ok "big endian" || bad "not big endian"
r 'Machine:[[:space:]]+PowerPC$' && ok "machine PowerPC" || bad "not PowerPC"
r 'Type:[[:space:]]+EXEC' && ok "EXEC" || bad "not EXEC"
r 'NetBSD.*IDENT 1101000000 \(11\.1\.0\)' && ok "NetBSD 11.1.0 ident note" || bad "no NetBSD 11.1.0 ident note"
r 'interpreter: /usr/libexec/ld\.elf_so\]' && ok "interpreter /usr/libexec/ld.elf_so" || bad "interpreter"
needed=$(sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' "$log/readelf.txt" | LC_ALL=C sort | tr '\n' ' ')
want="libICE.so.7 libSM.so.7 libX11.so.7 libXext.so.7 libc.so.12 libgcc_s.so.1 libm.so.0 libstdc++.so.9 "
[ "$needed" = "$want" ] && ok "NEEDED: $needed" || bad "NEEDED is '$needed', expected '$want'"
for lib in $needed; do
  found=$(ls "$sysroot/usr/lib/$lib" "$sysroot/lib/$lib" "$sysroot/usr/X11R7/lib/$lib" 2>/dev/null | head -n 1 || true)
  [ -n "$found" ] && ok "$lib resolves in the sysroot" || bad "$lib not in the sysroot"
done
[ -e "$sysroot/libexec/ld.elf_so" ] && ok "interpreter present in the sysroot (rooted)" || bad "interpreter missing in sysroot"
rpaths=$(sed -n 's/.*(\(RPATH\|RUNPATH\)).*\[\(.*\)\]/\2/p' "$log/readelf.txt")
[ "$rpaths" = "/usr/X11R7/lib" ] && ok "RPATH is exactly /usr/X11R7/lib" || bad "RPATH/RUNPATH is '$rpaths'"
if strings -a "$exe" | grep -E -- "$sysroot|$tools|$out|/home/|/runner|/tmp/|x86_64|lib64" > "$log/host-path-strings.txt"; then bad "runner or host paths in the binary (host-path-strings.txt)"; else ok "no runner or host paths in the binary"; fi
echo "relink-free ldd-style check: $(readelf -d "$exe" | grep -c NEEDED) NEEDED entries"

{
  echo "## NetBSD 11.1/macppc X11 cross build on Linux"
  echo
  echo "- result: $([ "$fails" -eq 0 ] && echo PASS || echo "FAIL ($fails checks)")"
  echo "- revision tested: $(git rev-parse HEAD) (GITHUB_SHA ${GITHUB_SHA:-unset})"
  echo "- pull request head: ${PR_HEAD_SHA:-n/a}"
  echo "- compiler: $("$tools/powerpc--netbsd-g++" --version | head -n 1); cmake $(cmake --version | head -n 1 | cut -d' ' -f3), ninja $(ninja --version), python $("$python" --version 2>&1)"
  echo "- flags: Release with CMAKE_CXX_FLAGS_RELEASE='$flags'"
  echo "- executable: $(stat -c %s "$exe") bytes, sha256 $(sha256sum "$exe" | cut -d' ' -f1)"
  echo "- cache: none"
  echo
  echo '```'
  grep -E '^(Type|Class|Data|Machine|  Machine| +\[Requesting)|NEEDED|RPATH|RUNPATH|IDENT' "$log/readelf.txt"
  echo '```'
} | tee "$out/summary.md"
echo
[ "$fails" -eq 0 ] || echo "$fails checks failed" >&2
exit $([ "$fails" -eq 0 ] && echo 0 || echo 1)
