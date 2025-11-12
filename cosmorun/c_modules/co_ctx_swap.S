.text
.globl co_ctx_swap

co_ctx_swap:
    mov x9, sp
    stp x30, x9, [x0]
    stp x0, x1, [x0, #0x10]
    stp x19, x20, [x0, #0x20]
    stp x21, x22, [x0, #0x30]
    stp x23, x24, [x0, #0x40]
    stp x25, x26, [x0, #0x50]
    stp x27, x28, [x0, #0x60]
    str x29, [x0, #0x70]

    ldr x29, [x1, #0x70]
    ldp x27, x28, [x1, #0x60]
    ldp x25, x26, [x1, #0x50]
    ldp x23, x24, [x1, #0x40]
    ldp x21, x22, [x1, #0x30]
    ldp x19, x20, [x1, #0x20]
    ldp x30, x9, [x1]
    mov sp, x9
    ldp x0, x1, [x1, #0x10]

    br x30
