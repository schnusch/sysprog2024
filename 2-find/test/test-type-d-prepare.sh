#!/bin/sh
set -e

echo .

mkdir a
echo ./a

mkdir a/b
echo ./a/b

touch a/b/c
touch a/d
touch e

mkdir a/b/f
echo ./a/b/f

# symlink to directory
ln -s b a/g

# symlink to file
ln -s d a/h

# dangling symlink
ln -s i j
