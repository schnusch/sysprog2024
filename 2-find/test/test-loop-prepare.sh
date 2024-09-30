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

# test-loop.d/loop and test-loop.d/loop/.gitkeep exist in the git repository
echo ./loop
echo ./loop/.gitkeep
# Because of the --volume shenanigans below, the directory test-loop.d/loop/loop
# is created by Docker.
echo ./loop/loop

# /loop is a directory from the host
# /loop/loop is the same directory from the host
echo /loop
echo /loop/.gitkeep
echo "/test/find: file system loop detected: /loop/loop = /loop"
