#!/bin/sh
set -e

echo .
echo ./a

# ./a/b is another tmpfs, but GNU find and busybox print the directory itself
echo ./a/b

touch a/b/c
