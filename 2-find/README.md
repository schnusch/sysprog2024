# find

```
Usage: ./find [-name NAME] [-type <d|f>] [-follow|-L] [-xdev] [DIR...]
```

If `DIR` is omitted it uses the current directory by default.

`-follow` will follow symlinks when determining the file type (roughly `stat`
instead of `lstat`). Every symlink will return the info of its target and will
be followed.

`-xdev` retrieves the `st_dev` for `DIR` and stop recursing when another
`st_dev` is encountered, because it would mean we crossed into another
file system.

## Directory File Descriptors

When recursing, every file is accessed relative to a directory file descriptor
(see [O_DIRECTORY](https://man7.org/linux/man-pages/man2/open.2.html#DESCRIPTION)).
When recursing into a directory, a file descriptor to the directory is opened,
and then its children are accessed relative to that file descriptor.

## `fstatat(2)`

`stat` relative to a file descriptor is done with [`fstatat(2)`](https://man7.org/linux/man-pages/man2/fstatat.2.html). To not follow symlinks `AT_SYMLINK_NOFOLLOW` is passed
in `flags`. This has the added benefit of choosing between `lstat` and `stat`
by only using a bit flag without using function pointers or branches.

## `struct dir_chain`

Every call to [`find`](./main.c) receives a `struct dir_chain`.
`struct dir_chain` is a linked list where its `parent` points to the the callers
`struct dir_chain`. By traversing that linked list and accessing the members
`name` we can reconstruct the file path we are currently operating on, without
any manual string handling, see [`print_dir_chain`](./main.c).

To detect file system loops `find` stores the file's `st_dev` and `st_ino` in
`struct dir_chain *this`. If the current `st_dev` and `st_ino` is equal to any
previous entries in the linked list, the element was encountered previously and
we can detect a file system loop. Such loops might be caused by symlinks with
`-follow` or bind mounts.

## `struct find_args`

Arguments that stay the same (except the member `struct stat st;`) when
recursing with `find` are stored in a `struct find_args` and only a pointer to
those common arguments is passed to every call. This avoids copying all common
arguments for `find` when recursing.

## Testing

`make check` runs some tests in a Docker container.

For each test exist two scripts. [`test/test-*-prepare.sh`](./test) creates a
directory structure and write the expected output of `find` to `stdout`.
[`test/test-*-run.sh`](./test) starts `find`. The [`Makefile`](./test/Makefile)
will compare the outputs of the two scripts.
