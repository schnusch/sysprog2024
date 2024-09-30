#!/bin/sh
set -e

# source gets mounted here
echo /host/test/test-absolute-name-run.sh

# tests are run here
echo /test/test/test-absolute-name-run.sh

echo "/test/find: cannot open /proc/tty/driver: Permission denied"
echo "/test/find: cannot open /root: Permission denied"
echo "/test/find: cannot open /var/cache/apt/archives/partial: Permission denied"
echo "/test/find: cannot open /var/cache/ldconfig: Permission denied"
echo "/test/find: file system loop detected: /loop/loop = /loop"
