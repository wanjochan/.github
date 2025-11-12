.text
.globl co_ctx_init

co_ctx_init:
    // x0 = ctx, x1 = entry_func, x2 = stack_top
    // Initialize ARM64 coroutine context (follows libcorpc convention)
    // DO NOT write to stack - only set register context

    // Clear all context registers
    stp xzr, xzr, [x0, #0x00]
    stp xzr, xzr, [x0, #0x10]
    stp xzr, xzr, [x0, #0x20]
    stp xzr, xzr, [x0, #0x30]
    stp xzr, xzr, [x0, #0x40]
    stp xzr, xzr, [x0, #0x50]
    stp xzr, xzr, [x0, #0x60]
    str xzr, [x0, #0x70]

    // Align stack_top to 16 bytes (ARM64 ABI requirement)
    and x3, x2, #-16

    // Set context registers (no stack initialization needed)
    // This follows libcorpc: "不需要设置栈中的LR和FP字段"
    stp x1, x3, [x0]       // x30 (LR/PC) = entry_func, sp = aligned stack_top
    str x3, [x0, #0x70]    // x29 (FP) = aligned stack_top

    ret
