#!/bin/sh
PS4='$ '
set -ex
sudo chown -R user:users /test
cp -a /host/* /test
make -C /test clean all
exec "$@"
