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

## Malicious input

### Inspection

`verify_password` is called with *cdecl* calling conventions. Tbis means that
first the functions arguments are pushed, second its return address, and third
the function is called.

The called function receives a preamble as well, where it pushes the `ebp`
register and set's its own `ebp` to `esp` so it can access local variables in
its stack frame relative to `ebp`. (Saving and restoring some registers is
the responsibility of the callee as well.) A matching postamble is created,
that restores the caller's `ebp` before returning to the caller.

After executing the preamble the stack will look like this:

`*(void **)ebp`
: the saved value of `ebp` from the caller, that will be restored in the postamble

`*((void **)ebp + 1)`
: (The word after the address pinted to by `ebp`, (+4 bytes))
: the return address pushed by the call instruction

`(char *)ebp - x`
: addresses of the function's local variables

Knowing this we step through the program with gdb:

  * start gdb and run `simple_login` until it called `verify_password`

    ```
    $ gdb ./simple_login
    (gdb) break verify_password
    (gdb) run < /dev/null
    ```
  * gdb will now stop the execution at our breakpoint
  * ```
    (gdb) set disassembly-flavor intel`
    (gdb) disassemble
    ```

    ```asm
    Dump of assembler code for function verify_password:
       0x080483a4 <+0>:     push   ebp
       0x080483a5 <+1>:     mov    ebp,esp
       0x080483a7 <+3>:     push   edi
       0x080483a8 <+4>:     push   esi
       0x080483a9 <+5>:     sub    esp,0xf0
    => 0x080483af <+11>:    lea    eax,[ebp-0xd0]
       0x080483b5 <+17>:    mov    DWORD PTR [esp+0x4],eax
       0x080483b9 <+21>:    mov    DWORD PTR [esp],0x804855c
       0x080483c0 <+28>:    call   0x80482f8 <scanf@plt>
       0x080483c5 <+33>:    lea    eax,[ebp-0xd0]
       0x080483cb <+39>:    mov    DWORD PTR [ebp-0xe0],eax
       0x080483d1 <+45>:    mov    DWORD PTR [ebp-0xe4],0x804855f
       0x080483db <+55>:    mov    DWORD PTR [ebp-0xe8],0x10
       0x080483e5 <+65>:    cld
       0x080483e6 <+66>:    mov    esi,DWORD PTR [ebp-0xe0]
       0x080483ec <+72>:    mov    edi,DWORD PTR [ebp-0xe4]
       0x080483f2 <+78>:    mov    ecx,DWORD PTR [ebp-0xe8]
       0x080483f8 <+84>:    repz cmps BYTE PTR ds:[esi],BYTE PTR es:[edi]
       0x080483fa <+86>:    seta   dl
       0x080483fd <+89>:    setb   al
       0x08048400 <+92>:    mov    ecx,edx
       0x08048402 <+94>:    sub    cl,al
       0x08048404 <+96>:    mov    eax,ecx
       0x08048406 <+98>:    movsx  eax,al
       0x08048409 <+101>:   test   eax,eax
       0x0804840b <+103>:   jne    0x8048419 <verify_password+117>
       0x0804840d <+105>:   mov    DWORD PTR [ebp-0xdc],0x1
       0x08048417 <+115>:   jmp    0x8048423 <verify_password+127>
       0x08048419 <+117>:   mov    DWORD PTR [ebp-0xdc],0x0
       0x08048423 <+127>:   mov    eax,DWORD PTR [ebp-0xdc]
       0x08048429 <+133>:   add    esp,0xf0
       0x0804842f <+139>:   pop    esi
       0x08048430 <+140>:   pop    edi
       0x08048431 <+141>:   pop    ebp
       0x08048432 <+142>:   ret
    End of assembler dump.
    ```

    The `sub esp,0xf0` tells us 240 bytes were reserved on the stack for local
    variables. (This is a little misleading because the compiler already
    allocates space for the arguments of `scanf` as well.)
  * inspect local variables

    ```
    (gdb) info locals
    password = "\000..."
    ```
  * show the type of the variable *password*

    ```
    (gdb) ptype password
    type = char [200]
    ```

  * show the address of *password*

    ```
    (gdb) print/a &password
    $1 = 0xffffdb68
    ```
  * We now know `verify_password` has a local variable `char password[200]`
    at 0xffffdb68.
  * ```asm
    => 0x080483af <+11>:    lea    eax,[ebp-0xd0]
       0x080483b5 <+17>:    mov    DWORD PTR [esp+0x4],eax
       0x080483b9 <+21>:    mov    DWORD PTR [esp],0x804855c
       0x080483c0 <+28>:    call   0x80482f8 <scanf@plt>
    ```

    From this assembler code we can tell that `scanf("%s", password)` is called.
    (`print/s (char *)0x804855c` will tell us the format string `"%s"` and
    `print/a $ebp - 0xd0` that a pointer to *password* is passed.)
  * pointer to the return address

    ```
    (gdb) print/a (void **)$ebp + 1
    $1 = 0xffffdc38
    ```
  * show us the return address

    ```
    (gdb) print/a *((void **)$ebp + 1)
    $1 = 0x8048455 <main+34>
    ```
  * From this we can tell the return address is 0xffffdc38 - 0xffffdb68 = 212
    bytes after *password*.
  * Now we change return address through *password*, create a new breakpoint,
    and see what happens at `ret`.

    ```
    (gdb) set *(void **)(password + 212) = -1
    (gdb) break *(verify_password+142)
    (gdb) continue
    (gdb) stepi
    0xffffffff in ?? ()
    ```

We successfully manipulated the return address by writing to *password*.

Since `scanf` does not do any bounds checking, we can override the return
address if we make `scanf` read 216 bytes into *password*.

### Implementation

[overflow.c](./overflow.c) takes a hexadecimal address as an argument and
generates an input for `simple_login` that will exploit the buffer overflow
and makes `simple_login` execute code at that address.

### Test

`make test-overflow-sigsegv` runs `simple_login` with an input that makes it
jump to 0xFFFFFFFF. `make gdb-overflow` runs the same in gdb and prints
relevant information while running. You can see that `verify_password` returns
execution to 0xFFFFFF.
