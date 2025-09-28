#!/bin/sh

export XDG_RUNTIME_DIR=$(mktemp -d)
mkdir -p "$XDG_RUNTIME_DIR"

weston --backend=headless-backend.so --socket=zathura-test-weston --idle-time=0 &
WESTON_PID=$!

# Wait for the socket to exist
for i in $(seq 10); do
  [ -e "$XDG_RUNTIME_DIR/zathura-test-weston" ] && break
  sleep 0.5
done

# run tests
$@
RET=$?

# Clean up Weston
kill $WESTON_PID
wait $WESTON_PID

rm -rf "$XDG_RUNTIME_DIR"
exit $RET
