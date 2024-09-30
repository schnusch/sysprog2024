#!/bin/sh
set -e

mkdir a
mkdir a/b

touch a/b/c
echo ./a/b/c
touch a/d
echo ./a/d
touch e
echo ./e

mkdir a/b/f

# symlink to directory
ln -s b a/g
echo ./a/g/c

# symlink to file
ln -s d a/h
echo ./a/h

# dangling symlink
ln -s i j
echo "/test/find: cannot stat ./j: No such file or directory"
