#!/bin/sh
PS4='$ '
set -ex
sudo tar -C /find_testbed/testbed/basic/MNT --zstd -xpf /find_testbed/xdev.tar.zstd
sudo chown -R user:users /test
cp -a /host/* /test
make -C /test clean all
exec "$@"
