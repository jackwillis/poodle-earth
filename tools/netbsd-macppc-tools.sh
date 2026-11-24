#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# NetBSD 11.1/macppc cross tools, built by build.sh on a Linux host from pinned source sets.
# This is the first Linux-hosted CI slice only: no sysroot, no application build, no package
# assembly, no G4 access, no publication. Cold by construction: WORK must not exist or must be empty.
#   tools/netbsd-macppc-tools.sh WORK
# Writes WORK/logs/* and WORK/summary.md; the toolchain lands in WORK/tools.
#   NETBSD_MIRROR   source-set base URL (default: the NetBSD CDN, 11.1 release)
#   NETBSD_JOBS     build.sh -j value (default: nproc)
set -euo pipefail
cd "$(dirname "$0")/.."
repo=$PWD
work=${1:?usage: netbsd-macppc-tools.sh WORK}
mirror=${NETBSD_MIRROR:-https://cdn.netbsd.org/pub/NetBSD/NetBSD-11.1/source/sets}
jobs=${NETBSD_JOBS:-$(nproc)}
pins=$repo/packaging/netbsd-macppc/netbsd-11.1-source.sha512
sets="gnusrc sharesrc src syssrc"

if [ -e "$work" ] && [ -n "$(ls -A "$work")" ]; then
  echo "refusing: $work is not empty (this job must start cold)" >&2
  exit 2
fi
mkdir -p "$work"/{dist,logs,root}
work=$(cd "$work" && pwd)
log=$work/logs
now() { date +%s; }
t_start=$(now)

# Peak disk: sample the filesystem holding WORK every 5 s; report growth over the baseline.
used() { df --output=used -B1 "$work" | tail -n 1 | tr -d ' '; }
base_used=$(used)
echo "$base_used" > "$work/.peak"
( while sleep 5; do
    u=$(used); p=$(cat "$work/.peak")
    [ "$u" -gt "$p" ] && echo "$u" > "$work/.peak"
  done ) &
sampler=$!
trap 'kill $sampler 2>/dev/null || true' EXIT

# 1. Fetch and verify. A set that does not match its pin is a hard stop.
t0=$(now)
for s in $sets; do
  curl -fsSL --retry 4 --retry-delay 5 -o "$work/dist/$s.tgz" "$mirror/$s.tgz" 2>&1 | tee -a "$log/fetch.log"
done
( cd "$work/dist" && sha512sum -c "$pins" ) 2>&1 | tee "$log/verify.log"
# The pin file must name exactly the sets used.
[ "$(wc -l < "$pins")" -eq "$(echo $sets | wc -w)" ]
t_fetch=$(( $(now) - t0 ))

# 2. Extract (sets unpack to usr/src).
t0=$(now)
for s in $sets; do tar -xzf "$work/dist/$s.tgz" -C "$work/root"; done
[ -x "$work/root/usr/src/build.sh" ]
t_extract=$(( $(now) - t0 ))
dist_bytes=$(du -sb "$work/dist" | cut -f1)
src_bytes=$(du -sb "$work/root" | cut -f1)

# 3. build.sh tools, unprivileged, out-of-tree.
t0=$(now)
set +e
( cd "$work/root/usr/src" && \
  ./build.sh -U -m macppc -j "$jobs" \
    -O "$work/obj" -T "$work/tooldir" -D "$work/destdir" -R "$work/release" tools ) \
  > "$log/build-tools.log" 2>&1
status=$?
set -e
t_build=$(( $(now) - t0 ))
tail -n 40 "$log/build-tools.log"

# 4. Inspect what exists. This proves the tools were produced, not that they are reproducible.
gcc=$work/tooldir/bin/powerpc--netbsd-gcc
if [ "$status" -eq 0 ] && [ -x "$gcc" ]; then
  { "$gcc" --version | head -n 1
    "$work/tooldir/bin/powerpc--netbsd-ld" --version | head -n 1
    "$gcc" -dumpmachine
    ( cd "$work/tooldir/bin" && sha256sum powerpc--netbsd-gcc powerpc--netbsd-g++ powerpc--netbsd-ld powerpc--netbsd-as ) ; } \
    > "$log/tool-inspection.log" 2>&1 || status=1
fi
t_total=$(( $(now) - t_start ))
sleep 6; kill $sampler 2>/dev/null || true
peak_used=$(cat "$work/.peak")
tool_bytes=$(du -sb "$work/tooldir" 2>/dev/null | cut -f1 || echo 0)
obj_bytes=$(du -sb "$work/obj" 2>/dev/null | cut -f1 || echo 0)
gib() { awk -v b="$1" 'BEGIN{printf "%.2f GiB", b/1073741824}'; }

{
  echo "## NetBSD 11.1/macppc build.sh tools on Linux"
  echo
  echo "- result: $([ "$status" -eq 0 ] && echo PASS || echo "FAIL (build.sh exit $status)")"
  echo "- revision: ${GITHUB_SHA:-$(git rev-parse HEAD)}"
  echo "- cache: none (cold start; WORK was empty)"
  echo "- source sets (SHA-512 verified against packaging/netbsd-macppc/netbsd-11.1-source.sha512): $sets"
  echo "- mirror: $mirror"
  echo "- host: $(. /etc/os-release && echo "$PRETTY_NAME"), $(uname -m), $(nproc) CPUs, $(awk '/MemTotal/{printf "%.1f GiB", $2/1048576}' /proc/meminfo) RAM, runner image ${ImageVersion:-unknown}"
  echo "- build.sh: -U -m macppc -j $jobs tools"
  echo "- runtime: total ${t_total}s (fetch+verify ${t_fetch}s, extract ${t_extract}s, build.sh tools ${t_build}s)"
  echo "- disk: peak $(gib $((peak_used - base_used))) over baseline (sampled every 5 s); downloads $(gib "$dist_bytes"), extracted sources $(gib "$src_bytes"), obj $(gib "$obj_bytes"), tooldir $(gib "$tool_bytes")"
  echo
  [ -f "$log/tool-inspection.log" ] && { echo '```'; cat "$log/tool-inspection.log"; echo '```'; }
} | tee "$work/summary.md"
exit "$status"
