#!/bin/sh
set -e

# test-loop.d/loop/.gitkeep exists in the git repository
echo /test/test/loop
echo /test/test/loop/.gitkeep

# Because of the --volume shenanigans below, the directory test-loop.d/loop/loop
# is created by Docker.
echo /test/test/loop/loop
