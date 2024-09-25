# traSH – a trashy shell implementation

A shell that only supports the command subset required by [the task](https://tu-dresden.de/ing/informatik/sya/professur-fuer-betriebssysteme/studium/praktika-seminare/komplexpraktikum-systemnahe-programmierung?set_language=en).

## Features

  * input history and tab completion through readline
  * run with `./trash -v` to receive debug output
  * run with `./trash -vv` to `ls` open file descriptors as well

## Known Issues

  * no concept of quoting
  * nothing like bash's [`-o pipefail`](https://www.gnu.org/software/bash/manual/html_node/The-Set-Builtin.html),
    only the exit status of the last pipeline command is available, see [POSIX](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_09_02)
