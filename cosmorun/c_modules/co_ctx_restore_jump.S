.text
.globl co_ctx_restore_jump

co_ctx_restore_jump:
    ldr x29, [x0, #0x70]
    ldp x27, x28, [x0, #0x60]
    ldp x25, x26, [x0, #0x50]
    ldp x23, x24, [x0, #0x40]
    ldp x21, x22, [x0, #0x30]
    ldp x19, x20, [x0, #0x20]
    ldp x30, x9, [x0]
    mov sp, x9
    ldp x0, x1, [x0, #0x10]
    br x30
