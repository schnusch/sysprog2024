#!/bin/sh
set -e

echo .
echo ./a

# ./a/b is another tmpfs

touch a/b/c
