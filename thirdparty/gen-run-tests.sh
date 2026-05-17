#!/bin/sh
# Generate rootfs/etc/run_tests from ostest/sources.list (installed ELFs only).
# Must use only features supported by bin/sh (see rootfs/etc/rc): no VAR=value,
# no $(...), no subshells; if/then/else/fi and cd/echo/test builtins only.
set -e

ROOT=${ROOT:?ROOT not set}
OSTEST_BASE=${OSTEST_BASE:?OSTEST_BASE not set}
INSTALL_DIR=${OSTEST_INSTALL_DIR:?OSTEST_INSTALL_DIR not set}
OUT="$ROOT/rootfs/etc/run_tests"
SOURCES="$OSTEST_BASE/sources.list"

if [ ! -f "$SOURCES" ]; then
  echo "gen-run-tests: missing $SOURCES" >&2
  exit 1
fi

{
  cat <<'EOF'
#!/bin/sh
# Many os-tests opendir(".") and expect a subdirectory named after the suite
# (e.g. dirent/readdir looks for "./dirent"). Run from /bin/os-tests-sortix.

echo "========================================="
echo "  os-tests-sortix"
echo "========================================="

if [ ! -d /bin/os-tests-sortix ]; then
echo "missing /bin/os-tests-sortix (run: make -C thirdparty os-tests-install)"
exit 1
fi

cd /bin/os-tests-sortix
EOF

  while read -r rel; do
    [ -n "$rel" ] || continue
    [ -f "$INSTALL_DIR/$rel" ] || continue
    echo "echo $rel"
    echo "./$rel"
  done <"$SOURCES"

  cat <<'EOF'

echo "========================================="
echo "  Done"
EOF
} >"$OUT"

chmod +x "$OUT"
echo "gen-run-tests: wrote $OUT"
