#!/bin/sh
set -e

echo .
echo ./a

mkdir a/b
echo ./a/b

ln -s .. a/b/c
echo "/test/find: file system loop detected: ./a/b/c = ./a"

# ./a/d is another tmpfs
echo ./a/d

ln -s ../.. a/d/e
echo "/test/find: file system loop detected: ./a/d/e = ."

# /loop is a directory from the host
# /loop/loop is the same directory from the host
echo /loop
echo /loop/.gitkeep
echo "/test/find: file system loop detected: /loop/loop = /loop"
