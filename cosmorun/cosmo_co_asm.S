// cosmo_co_asm.S - Unified coroutine assembly wrapper
// Selects architecture-specific implementation

#if defined(__aarch64__) || defined(__arm64__)
    #include "__co_ctx_arm64.S"
#elif defined(__x86_64__) || defined(__amd64__)
    #include "__co_ctx_x64.S"
#else
    #error "Unsupported architecture for coroutines"
#endif
