#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Jack Willis
# NetBSD 11.1/macppc base + X11 sysroot, assembled on a Linux host from pinned release sets.
# Sysroot slice only: no application build, no package, no G4 access, no publication. Cold by
# construction: WORK must not exist or must be empty. See packaging/netbsd-macppc/PROVENANCE.md.
#   tools/netbsd-macppc-sysroot.sh WORK
# Writes WORK/sysroot, WORK/logs/* and WORK/summary.md.
#   NETBSD_MIRROR            set base URL (default: the NetBSD CDN, 11.1/macppc)
#   NETBSD_CROSSCHECK_MIRROR second host whose published SHA512 must agree with the pins
set -euo pipefail
cd "$(dirname "$0")/.."
work=${1:?usage: netbsd-macppc-sysroot.sh WORK}
rel=NetBSD/NetBSD-11.1/macppc/binary/sets
mirror=${NETBSD_MIRROR:-https://cdn.netbsd.org/pub/$rel}
cross=${NETBSD_CROSSCHECK_MIRROR:-https://ftp.netbsd.org/pub/$rel}
pins=$PWD/packaging/netbsd-macppc/netbsd-11.1-macppc-sets.sha512
check=$PWD/tools/check-netbsd-macppc-sysroot.sh
sets=$(awk '{print $2}' "$pins" | sed 's/\.tgz$//')

if [ -e "$work" ] && [ -n "$(ls -A "$work")" ]; then
  echo "refusing: $work is not empty (this job must start cold)" >&2
  exit 2
fi
mkdir -p "$work"/{dist,logs,sysroot}
work=$(cd "$work" && pwd)
log=$work/logs
now() { date +%s; }
t_start=$(now)

used() { df --output=used -B1 "$work" | tail -n 1 | tr -d ' '; }
base_used=$(used)
echo "$base_used" > "$work/.peak"
( while sleep 2; do
    u=$(used); p=$(cat "$work/.peak")
    [ "$u" -gt "$p" ] && echo "$u" > "$work/.peak"
  done ) &
sampler=$!
trap 'kill $sampler 2>/dev/null || true' EXIT

# 1. Fetch the sets and both published digest files.
t0=$(now)
for s in $sets; do
  curl -fsSL --retry 4 --retry-delay 5 -o "$work/dist/$s.tgz" "$mirror/$s.tgz"
done
curl -fsSL --retry 4 -o "$work/dist/SHA512.primary" "$mirror/SHA512"
curl -fsSL --retry 4 -o "$work/dist/SHA512.cross" "$cross/SHA512"
t_fetch=$(( $(now) - t0 ))

# 2. Authenticate: the pins must equal both published files for these sets, and the downloads must
#    equal the pins. NetBSD publishes no signature for the sets (PROVENANCE.md).
t0=$(now)
{
  for f in primary cross; do
    for s in $sets; do
      sed -n "s/^SHA512 ($s\.tgz) = \(.*\)$/\1  $s.tgz/p" "$work/dist/SHA512.$f"
    done > "$work/dist/pins.$f"
    cmp "$work/dist/pins.$f" "$pins" && echo "pins match published SHA512 from $f host"
  done
  ( cd "$work/dist" && sha512sum -c "$pins" )
} 2>&1 | tee "$log/verify.log"
t_verify=$(( $(now) - t0 ))
set_sizes=$(cd "$work/dist" && for s in $sets; do printf '%s %s bytes, ' "$s.tgz" "$(stat -c %s "$s.tgz")"; done)

# 3. Assemble from an empty directory, unprivileged. A few directories in base are mode 000;
#    make directories traversable so the tree can be inspected and removed.
t0=$(now)
for s in $sets; do
  tar -xzf "$work/dist/$s.tgz" --no-same-owner -C "$work/sysroot"
  tar -tzf "$work/dist/$s.tgz" > "$log/files-$s.txt"
done
find "$work/sysroot" -type d -exec chmod u+rwx {} \;
t_extract=$(( $(now) - t0 ))

# 4. Verify the pieces the later dynamic-X11 build needs.
t0=$(now)
set +e
"$check" "$work/sysroot" > "$log/check.log" 2>&1
status=$?
set -e
t_check=$(( $(now) - t0 ))
tail -n 30 "$log/check.log"

t_total=$(( $(now) - t_start ))
sleep 3; kill $sampler 2>/dev/null || true
peak=$(cat "$work/.peak")
gib() { awk -v b="$1" 'BEGIN{printf "%.2f GiB", b/1073741824}'; }
{
  echo "## NetBSD 11.1/macppc base + X11 sysroot on Linux"
  echo
  echo "- result: $([ "$status" -eq 0 ] && echo PASS || echo "FAIL (check exit $status)")"
  echo "- revision tested: $(git rev-parse HEAD) (GITHUB_SHA ${GITHUB_SHA:-unset})"
  echo "- pull request head: ${PR_HEAD_SHA:-n/a}"
  echo "- cache: none (cold start; WORK was empty)"
  echo "- mirror: $mirror; cross-check host: $cross"
  echo "- authentication: SHA-512 pins in the repository == both hosts' published SHA512 == downloads; no vendor signature exists (packaging/netbsd-macppc/PROVENANCE.md)"
  echo "- host: $(. /etc/os-release && echo "$PRETTY_NAME"), $(uname -m), $(nproc) CPUs, runner image ${ImageVersion:-unknown}"
  echo "- runtime: total ${t_total}s (fetch ${t_fetch}s, verify ${t_verify}s, extract ${t_extract}s, check ${t_check}s)"
  echo "- disk: peak $(gib $((peak - base_used))) over baseline (sampled every 2 s); downloads $(gib "$(du -sb "$work/dist" | cut -f1)"), sysroot $(gib "$(du -sb "$work/sysroot" | cut -f1)")"
  echo "- sets: $set_sizes"
  echo
  echo '```'
  cat "$work/dist/../logs/verify.log" | grep -E ': OK|FAILED|match'
  echo '```'
  echo
  echo '```'
  grep -E '^(ok|FAIL|note)' "$log/check.log" | tail -n 60
  echo '```'
} | tee "$work/summary.md"
exit "$status"
