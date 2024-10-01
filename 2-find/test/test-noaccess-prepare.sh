#!/bin/sh
set -e

echo .

mkdir a
echo ./a

mkdir a/b
echo ./a/b

touch a/b/c

chmod a-rwx a/b
echo "/test/find: cannot open ./a/b: Permission denied"
