.global _start
_start:
	call main
	// return value is in eax, push it as exit's argument
	pushl %eax
	call exit
