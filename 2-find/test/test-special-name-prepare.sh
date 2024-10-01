#!/bin/sh
set -e
# /test/find . -name .
echo .
# /test/find ./ -name .
echo ./
# /test/find / -name /
echo /
# /test/find /. -name /
# /test/find /. -name .
echo /.
# /test/find /test -name test
echo /test
echo /test/test
echo "/test/find: cannot open /test/test/test-noaccess.d/a/b: Permission denied"

for root in / /./ /./; do
	echo "/test/find: cannot open ${root}proc/tty/driver: Permission denied"
	echo "/test/find: cannot open ${root}root: Permission denied"
	echo "/test/find: cannot open ${root}test/test/test-noaccess.d/a/b: Permission denied"
	echo "/test/find: cannot open ${root}var/cache/apt/archives/partial: Permission denied"
	echo "/test/find: cannot open ${root}var/cache/ldconfig: Permission denied"
	echo "/test/find: file system loop detected: ${root}loop/loop = ${root}loop"
done
