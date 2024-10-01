.global _start
_start:
	call main
	// return value is in eax, but eax is syscall number and ebx is the
	// argument to exit
	movl %eax, %ebx
	movl $1, %eax
	int $0x80

