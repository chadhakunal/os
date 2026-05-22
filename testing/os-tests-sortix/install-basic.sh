#!/bin/sh
# Install cross-compiled os-test ELFs (ok only) into build/rootfs/bin/os-tests-sortix/.
set -e

ROOT=${ROOT:?ROOT not set}
OSTEST_SRC=${OSTEST_SRC:?OSTEST_SRC not set}
OSTEST_BASE="${OSTEST_BASE:-$OSTEST_SRC/ostest}"
OUT="${OSTEST_OUT:-$OSTEST_BASE/basic}"
REPORT="${OSTEST_REPORT:-$OSTEST_BASE/report}"
INSTALL_DIR=${OSTEST_INSTALL_DIR:?OSTEST_INSTALL_DIR not set}
# Some tests (e.g. fstatat) open ".." and stat "basic/<suite>/<name>" — Sortix
# source layout under the parent of cwd. Mirror ELFs there when cwd is
# /bin/os-tests-sortix (.. == /bin).
BASIC_MIRROR_DIR="$(dirname "$INSTALL_DIR")/basic"
SOURCES="$OSTEST_BASE/sources.list"

if [ ! -f "$SOURCES" ]; then
  echo "install-basic: missing $SOURCES — run os-tests-build first" >&2
  exit 1
fi

rm -rf "$INSTALL_DIR" "$BASIC_MIRROR_DIR"
installed=0
skipped=0

while read -r rel; do
  [ -n "$rel" ] || continue
  if [ ! -f "$REPORT/$rel.out" ] || ! grep -qx ok "$REPORT/$rel.out"; then
    skipped=$((skipped + 1))
    continue
  fi
  if [ ! -f "$OUT/$rel" ]; then
    skipped=$((skipped + 1))
    continue
  fi
  mkdir -p "$(dirname "$INSTALL_DIR/$rel")" "$(dirname "$BASIC_MIRROR_DIR/$rel")"
  cp "$OUT/$rel" "$INSTALL_DIR/$rel"
  chmod +x "$INSTALL_DIR/$rel"
  cp "$OUT/$rel" "$BASIC_MIRROR_DIR/$rel"
  chmod +x "$BASIC_MIRROR_DIR/$rel"
  installed=$((installed + 1))
done <"$SOURCES"

echo "install-basic: $installed ELF(s) -> $INSTALL_DIR and $BASIC_MIRROR_DIR ($skipped not ok / missing)"
