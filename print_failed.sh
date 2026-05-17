tests="psiginfo psignal pthread_kill pthread_sigmask raise sig2str sigaction sigaltstack sigpending sigprocmask sigqueue sigsuspend sigtimedwait sigwait sigwaitinfo str2sig"
for t in $tests; do
  echo "=== $t ==="
  cat thirdparty/os-tests-sortix/ostest/report/signal/$t.err 2>/dev/null || echo "(no file)"
done >/tmp/signal_errors.txt && echo "done"
