#!/bin/sh
set +e
/test/find / -name /
/test/find /. -name /
/test/find /. -name .
/test/find /test -name test
/test/find . -name .
/test/find ./ -name .
