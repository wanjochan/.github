# x86_64_call_on_alt_stack.S
# rdi=fn, rsi=arg, rdx=stack_top_aligned
# 约定：在 call 前 RSP 必须 16B 对齐；我们把 RSP 设为对齐好的新栈，再 call *rax
# 注意：SysV 要求 “call 前 RSP 16B 对齐”，call 会压 8B 返回地址 -> 被调内部是 16N+8 状态
.text
.global call_on_alt_stack
.type   call_on_alt_stack,@function
.p2align 4
call_on_alt_stack:
  pushq %rbp
  mov   %rsp, %rbp

  mov   %rsp, %r8          # 备份原 rsp
  mov   %rdx, %rsp         # 切到新栈（要求已 16B 对齐）

  mov   %rdi, %rax         # rax = fn
  mov   %rsi, %rdi         # rdi = arg
  # 此时 %rsp 已 16B 对齐，满足 SysV “call 前对齐”要求
  call  *%rax

  mov   %r8,  %rsp         # 恢复原栈
  leave
  ret
.size call_on_alt_stack, .-call_on_alt_stack

