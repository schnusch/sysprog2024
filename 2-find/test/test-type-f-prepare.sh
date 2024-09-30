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
