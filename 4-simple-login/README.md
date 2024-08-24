# simple_login

You can prefix all commands with `./with-docker.sh` to run them in a i386
Debian container. This allows us to easily run it on NixOS and we do not have
manage a multilib environment.

## intialized GDB session

`make gdb-tui` runs `gdb -x gdb/tui.txt ./simple_login` in the container.
[gdb/tui.txt](./gdb/tui.txt) enables gdb's TUI with assembly and register view.
Breakpoints are set at the beginning of simple_login's `verify_password` and
right after its call to `scanf`. The second breakpoint allows us to `continue`
when stepping through with gdb lands us in libc's `scanf`.

## Finding out the password

The password is `AaMilmFS+z11/2J`.

### 1. `strings simple_login`

We can use binutils' `strings` to show all printable strings in simple_login.
Between the many symbols, near the top of the output are simple_login's output
phrases and the string `AaMilmFS+z11/2J`.

### 2. `objdump -s -j .rodata simple_login`

Binutils' `objdump` can also dump the `.rodata` section of simple_login, where
strings are stored. In the hexdump we can see `AaMilmFS+z11/2J` again.

### 3. `gdb -x gdb/password.txt ./simple_login`

[gdb/password.txt](./gdb/password.txt) sets the breakpoint `*(verify_password+84)`.
This is where the input string is compared to the password:

```asm
   0x080483e5 <+65>:    cld
   0x080483e6 <+66>:    mov    esi,DWORD PTR [ebp-0xe0]
   0x080483ec <+72>:    mov    edi,DWORD PTR [ebp-0xe4]
   0x080483f2 <+78>:    mov    ecx,DWORD PTR [ebp-0xe8]
=> 0x080483f8 <+84>:    repz cmps BYTE PTR ds:[esi],BYTE PTR es:[edi]
```

The code is explained in this [StackOverflow answer](https://stackoverflow.com/a/44630309)
and compares the data pointed to by registers `esi` (which holds the input)
and `edi` (which holds the password). (`cld` clears the direction flag, which
means `repz` will increment `esi` and `edi`.)

`print/s (char *)$edi` yields the hardcoded password.
