extern "C" void _start() {
	// This will still segfault when `_start` returns. `ret` will pop an address
	// from the stack and jump there, but since this function was not called
	// properly, but only jumped into, the stack was not set up properly and
	// execution after the `ret` will continue at some point, that will cause
	// a SIGSEGV.
}
