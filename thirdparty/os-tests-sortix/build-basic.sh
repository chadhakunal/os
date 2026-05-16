#!/bin/sh
# Cross-compile os-test basic/*.c for sbunix (freestanding + libc.a).
set -e

ROOT=${ROOT:?ROOT not set}
OSTEST_SRC=${OSTEST_SRC:?OSTEST_SRC not set}
OSTEST_BASE="${OSTEST_BASE:-$OSTEST_SRC/ostest}"
BASIC="$OSTEST_SRC/basic"
OUT="${OSTEST_OUT:-$OSTEST_BASE/basic}"
REPORT="${OSTEST_REPORT:-$OSTEST_BASE/report}"
SOURCES="$OSTEST_BASE/sources.list"
SUMMARY="$OSTEST_BASE/summary.txt"

CRT="$ROOT/build/libc/crt.S.o"
LIBC="$ROOT/build/libc.a"

if [ ! -f "$LIBC" ] || [ ! -f "$CRT" ]; then
  echo "build-basic: run 'make -C $ROOT build/libc.a build/libc/crt.S.o' first" >&2
  exit 1
fi
if [ ! -d "$BASIC" ]; then
  echo "build-basic: missing $BASIC — run 'make -C thirdparty os-tests-sortix' first" >&2
  exit 1
fi

CC=${CC:-riscv64-unknown-elf-gcc}
SYSINC=$($CC -print-file-name=include)

CFLAGS="-gdwarf-4 -Wall -Wextra -Os -ffreestanding -fno-builtin -nostdlib -nostdinc"
CFLAGS="$CFLAGS -isystem $SYSINC -mcmodel=medany"
CFLAGS="$CFLAGS -march=rv64imac_zicsr_zifencei -mabi=lp64"
CFLAGS="$CFLAGS -ffunction-sections -fdata-sections"
CFLAGS="$CFLAGS -Werror=implicit-function-declaration"
CPPFLAGS="-I$ROOT/libc/include -I$OSTEST_SRC -I$BASIC"
DEFS="-D_GNU_SOURCE -D_BSD_SOURCE -D_ALL_SOURCE -D_DEFAULT_SOURCE"
LDFLAGS="$CRT $LIBC -Wl,--gc-sections"

JOBS=${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}
mkdir -p "$OUT" "$REPORT"

# Keep in sync with OSTEST_PRUNE_DIRS in thirdparty/Makefile.
OSTEST_SKIP_DIRS="pthread threads spawn semaphore sys_sem arpa_inet netinet_in net_if netdb sys_socket"

ostest_is_skipped() {
  rel=$1
  top=${rel%%/*}
  for skip in $OSTEST_SKIP_DIRS; do
    [ "$top" = "$skip" ] && return 0
  done
  return 1
}


export ROOT OSTEST_SRC OSTEST_BASE BASIC OUT REPORT SOURCES SUMMARY CC
export CFLAGS CPPFLAGS DEFS LDFLAGS

mkdir -p "$OSTEST_BASE"
: >"$SOURCES"
cd "$BASIC"

if [ -z "${OSTEST_TESTS:-}" ]; then
  find . -name '*.c' | sed 's|^\./||;s|\.c$||' | sort | while read -r rel; do
    if ostest_is_skipped "$rel"; then
      continue
    fi
    echo "$rel"
  done >>"$SOURCES"
else
  for spec in $OSTEST_TESTS; do
    if [ -f "$spec.c" ]; then
      if ostest_is_skipped "$spec"; then
        echo "build-basic: skipping pthread/network test: $spec" >&2
        continue
      fi
      echo "$spec" >>"$SOURCES"
    elif [ -d "$spec" ]; then
      if ostest_is_skipped "$spec/."; then
        echo "build-basic: skipping pthread/network directory: $spec" >&2
        continue
      fi
      find "$spec" -name '*.c' | sed 's|^\./||;s|\.c$||' | sort | while read -r rel; do
        ostest_is_skipped "$rel" && continue
        echo "$rel"
      done >>"$SOURCES"
    else
      echo "build-basic: unknown OSTEST_TESTS entry: $spec" >&2
      exit 1
    fi
  done
  sort -u -o "$SOURCES" "$SOURCES"
fi

total=$(wc -l <"$SOURCES" | tr -d ' ')
if [ "$total" -eq 0 ]; then
  echo "build-basic: no tests selected" >&2
  exit 1
fi

echo "build-basic: $total test(s), jobs=$JOBS"
[ -n "${OSTEST_TESTS:-}" ] && echo "  filter: $OSTEST_TESTS"

xargs -P "$JOBS" -I {} sh -c '
  rel=$1
  src="$BASIC/$rel.c"
  bin="$OUT/$rel"
  obj="$OUT/$rel.o"
  err="$REPORT/$rel.err"
  meta="$REPORT/$rel.out"

  mkdir -p "$(dirname "$bin")" "$(dirname "$err")"
  rm -f "$bin" "$obj" "$err" "$meta"

  if ! $CC $CFLAGS $CPPFLAGS $DEFS -c "$src" -o "$obj" >"$err" 2>&1; then
    rm -f "$obj"
    if grep -Eq '"'"'^/\*optional\*/$'"'"' "$src" 2>/dev/null; then
      outcome=missing_optional
    elif grep -E '"'"'error:'"'"' "$err" | grep -Ev '"'"'type specifier missing,'"'"' | head -1 \
      | grep -E '"'"'fatal error'"'"' >/dev/null 2>&1; then
      outcome=missing_header
    elif grep -E '"'"'error:'"'"' "$err" | grep -Ev '"'"'type specifier missing,'"'"' | head -1 \
      | grep -E '"'"'incompatible|pointer-sign'"'"' >/dev/null 2>&1; then
      outcome=incompatible
    elif grep -E '"'"'error:'"'"' "$err" | grep -Ev '"'"'type specifier missing,'"'"' | head -1 \
      | grep -E '"'"'undeclared|no member named|is not defined'"'"' >/dev/null 2>&1; then
      outcome=undeclared
    elif grep -E '"'"'error:'"'"' "$err" | grep -Ev '"'"'type specifier missing,'"'"' | head -1 \
      | grep -E '"'"'unknown type name|expected declaration specifiers|storage size of'"'"' \
      >/dev/null 2>&1; then
      outcome=unknown_type
    else
      outcome=compile_error
    fi
    printf '"'"'#!/bin/sh\necho %s\n'"'"' "$outcome" >"$bin"
    chmod +x "$bin"
    echo "$outcome" >"$meta"
    exit 0
  fi

  if ! $CC $CFLAGS -o "$bin" "$obj" $LDFLAGS >>"$err" 2>&1; then
    rm -f "$obj" "$bin"
    outcome=undefined
    printf '"'"'#!/bin/sh\necho %s\n'"'"' "$outcome" >"$bin"
    chmod +x "$bin"
    echo "$outcome" >"$meta"
    exit 0
  fi

  rm -f "$obj" "$err"
  echo ok >"$meta"
' _ {} <"$SOURCES"

{
  echo "os-test basic cross-build summary"
  echo "total: $total"
  [ -n "${OSTEST_TESTS:-}" ] && echo "filter: $OSTEST_TESTS"
  for tag in ok compile_error missing_header undeclared unknown_type incompatible undefined missing_optional; do
    c=0
    while read -r rel; do
      [ -f "$REPORT/$rel.out" ] || continue
      grep -qx "$tag" "$REPORT/$rel.out" && c=$((c + 1))
    done <"$SOURCES"
    echo "$tag: $c"
  done
} | tee "$SUMMARY"

failed=0
while read -r rel; do
  [ -f "$REPORT/$rel.out" ] || continue
  grep -qx ok "$REPORT/$rel.out" && continue
  outcome=$(cat "$REPORT/$rel.out")
  echo "  NOT LINKED: $rel ($outcome)"
  failed=$((failed + 1))
done <"$SOURCES"

if [ "$failed" -eq 0 ]; then
  echo "build-basic: all $total test(s) linked"
else
  echo "build-basic: $failed test(s) did not link (see above and $REPORT/*.err)"
fi
echo "build-basic: ok ELFs in $OUT"
echo "build-basic: summary: $SUMMARY"
