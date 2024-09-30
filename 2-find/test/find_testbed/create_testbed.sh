#! /bin/bash

# Copyright (C) 2008 Carsten Weinhold <weinhold@os.inf.tu-dresden.de>
# Technische Universitaet Dresden, Operating Systems Research Group
#
# This file creates a testbed for testing programs similar to GNU find.
# It is licensed under the terms of the GNU General Public Licence 2.

set -e

# prepare ...
cd "${0%/*}"
rm -rf testbed
mkdir -p testbed/basic
mkdir -p testbed/basic/MNT
mkdir -p testbed/traps

BASIC_DIR="`cd testbed/basic && pwd`"
TRAPS_DIR="`cd testbed/traps && pwd`"


# ############################################################################

# create a few simple dirs
cd "$BASIC_DIR"
touch    a
mkdir -p A
mkdir    A/AA1
touch    A/aa1
mkdir -p B
touch    B/bb1
mkdir -p B/BB1/BBB1
touch    B/BB1/bbb1
touch    B/BB1/bbb2
touch    B/BB1/bbb3
mkdir -p B/BB2/BBB2
touch    B/BB2/BBB2/bbbb1
touch    B/BB2/bbb1
touch    B/BB2/bbb2
mkdir -p B/BB2/BBB3
touch    B/BB2/BBB3/bbbb1
mkdir -p B/BB2/.BBB4
touch    B/BB2/.BBB4/bbbb1
ln -s    B/BB1 LINK_TO_B_BB1
ln -s    A/aa1 LINK_TO_A_aa1
ln -s    ../BBB2 B/BB2/BBB3/LINK_TO_BBB2
ln -s    ../BB2 B/BB2/BBB3/BROKEN_LINK

# ############################################################################

# create deep hierarchy
cd "$TRAPS_DIR"
i=1
while [ $i -lt 280 ]
do
        mkdir sub_directory_${i}
        cd sub_directory_${i} 2>/dev/null
        touch depth_is_${i}_here
        i=$(($i + 1))
done
cd "$TRAPS_DIR"
mkdir -p sub_directory_1/ZZ
touch    sub_directory_1/ZZ/zzz

# create a loop
cd "$TRAPS_DIR"
mkdir loop_sub_directory
ln -s . loop_sub_directory/symlink_to_dot
ln -s ../loop_sub_directory loop_sub_directory/complicated_symlink_to_dot

exit 0
