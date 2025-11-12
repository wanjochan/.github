// aarch64_call_on_alt_stack.S
// x0=fn, x1=arg, x2=stack_top_aligned
// 约定：进入 fn 时栈已 16B 对齐；返回正常 ret 落到 .Lafter。
.text
.global call_on_alt_stack
.type   call_on_alt_stack,%function
.p2align 2
call_on_alt_stack:
  // 建立调用者帧，保存 x29/x30（帧指针/返回地址）
  stp x29, x30, [sp, #-16]!
  mov x29, sp

  // 备份原 sp 到 x9
  mov x9, sp

  // 切到新栈
  mov sp, x2

  // 保存 fn 指针到 x11，准备参数到 x0
  mov x11, x0      // x11 = fn
  mov x0,  x1      // x0  = arg

  // 像正常调用一样进入，被调函数返回会落到下一条指令
  blr x11

.Lafter_fn:
  // 恢复原 sp
  mov sp, x9

  // 恢复调用者帧并返回
  ldp x29, x30, [sp], #16
  ret
.size call_on_alt_stack, .-call_on_alt_stack

