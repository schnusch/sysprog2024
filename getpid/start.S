.global _start
_start:
    call main
    push %eax
    call exit
