// =============================================
// linux_crt1.c - Linux CRT1 adapted from TCC Windows implementation

#include <stdio.h>
#include <stdlib.h>

// Linux-specific definitions
extern int main(int argc, char **argv, char **envp);
extern char **environ;

// Linux CRT initialization - direct main call
extern int main(int argc, char **argv, char **envp);

// Simplified _start that directly calls main
// This avoids the complexity of _runmain and constructors/destructors
void _start(void)
{
    // Minimal arguments for TCC self-compilation
    char *argv[] = {"tcc", NULL};
    char *envp[] = {NULL};

    // Set global environ
    environ = envp;

    // Call main directly with minimal args
    int ret = main(1, argv, envp);

    // Exit using system call directly
    asm volatile("movl %0, %%edi\n\t"
                 "movl $60, %%eax\n\t"  // sys_exit
                 "syscall"
                 :
                 : "r" (ret)
                 : "rdi", "rax");
    __builtin_unreachable();
}

// =============================================
