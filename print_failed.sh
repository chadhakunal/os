tests="fclose fdopen feof ferror fflush fgetc fgetpos fgets fileno flockfile fmemopen fopen fprintf"
for t in $tests; do
  echo "=== $t ==="
  cat thirdparty/os-tests-sortix/ostest/report/stdio/$t.err 2>/dev/null || echo "(no file)"
done >/tmp/stdio_errors.txt && echo "done"
