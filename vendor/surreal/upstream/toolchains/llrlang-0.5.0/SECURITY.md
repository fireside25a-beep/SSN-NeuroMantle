# Security policy

LLRLANG 0.5.0 is a research-grade native compiler release. Please report reproducible compiler crashes, silent miscompilations, incorrect runtime trap behavior, malformed ELF output, bootstrap divergence, or managed-region bounds failures privately to the project maintainer before public disclosure when practical.

Raw memory forms and direct syscall forms are privileged low-level operations and are outside the managed-memory safety guarantee. See `docs/MEMORY_AND_SAFETY.md` for the precise boundary.
