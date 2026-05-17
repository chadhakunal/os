tests="wait waitid waitpid"
for t in $tests; do
  echo "=== $t ==="
  cat thirdparty/os-tests-sortix/ostest/report/sys_wait/$t.err 2>/dev/null || echo "(no file)"
done >/tmp/syswait_errors.txt && echo "done"
