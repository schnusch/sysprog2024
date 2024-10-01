#!/bin/sh
set -e
echo "/test/find: file system loop detected: ./testbed/traps/loop_sub_directory/complicated_symlink_to_dot = ./testbed/traps/loop_sub_directory"
echo "/test/find: file system loop detected: ./testbed/traps/loop_sub_directory/symlink_to_dot = ./testbed/traps/loop_sub_directory"
exec ../test-testbed-follow-find.sh
