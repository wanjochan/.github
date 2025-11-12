/* Sandboxing implementation using seccomp-bpf (Linux)
 *
 * Note: This is compiled for Cosmopolitan (APE) which runs on multiple OSes.
 * We include Linux headers unconditionally and check OS at runtime.
 */

#include "cosmo_sandbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

/* Declare prctl system call (not in Cosmopolitan headers) */
extern int prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);

/* Define seccomp constants if not available */
#ifndef SECCOMP_MODE_FILTER
#define SECCOMP_MODE_FILTER 2
#endif

#ifndef SECCOMP_RET_KILL
#define SECCOMP_RET_KILL        0x00000000U
#define SECCOMP_RET_ALLOW       0x7fff0000U
#define SECCOMP_RET_ERRNO       0x00050000U
#define SECCOMP_RET_DATA        0x0000ffffU
#endif

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#endif

#ifndef PR_SET_SECCOMP
#define PR_SET_SECCOMP 22
#endif

/* BPF program structure (from linux/filter.h) - declare early */
struct sock_filter {
    unsigned short code;
    unsigned char jt;
    unsigned char jf;
    unsigned int k;
};

struct sock_fprog {
    unsigned short len;
    struct sock_filter *filter;
};

/* BPF helper macros */
#ifndef BPF_STMT
#define BPF_STMT(code, k) ((struct sock_filter){ (unsigned short)(code), 0, 0, k })
#define BPF_JUMP(code, k, jt, jf) ((struct sock_filter){ (unsigned short)(code), jt, jf, k })
#endif

/* BPF instruction classes */
#ifndef BPF_LD
#define BPF_LD   0x00
#define BPF_W    0x00
#define BPF_ABS  0x20
#define BPF_JMP  0x05
#define BPF_JEQ  0x10
#define BPF_K    0x00
#define BPF_RET  0x06
#endif

/* Audit architectures */
#ifndef AUDIT_ARCH_X86_64
#define AUDIT_ARCH_X86_64 0xc000003e
#endif
#ifndef AUDIT_ARCH_AARCH64
#define AUDIT_ARCH_AARCH64 0xc00000b7
#endif

/* Detect architecture at runtime */
#if defined(__x86_64__)
#define AUDIT_ARCH_CURRENT AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
#define AUDIT_ARCH_CURRENT AUDIT_ARCH_AARCH64
#else
#define AUDIT_ARCH_CURRENT 0
#endif

/* Syscall numbers - we'll use hardcoded values for common syscalls */
/* x86_64 syscall numbers */
#define SYS_WRITE_X64 1
#define SYS_OPEN_X64 2
#define SYS_CLOSE_X64 3
#define SYS_CREAT_X64 85
#define SYS_UNLINK_X64 87
#define SYS_MKDIR_X64 83
#define SYS_RMDIR_X64 84
#define SYS_SOCKET_X64 41
#define SYS_CONNECT_X64 42
#define SYS_BIND_X64 49
#define SYS_LISTEN_X64 50
#define SYS_ACCEPT_X64 43
#define SYS_EXECVE_X64 59
#define SYS_FORK_X64 57
#define SYS_VFORK_X64 58
#define SYS_CLONE_X64 56

/* Seccomp data structure */
struct seccomp_data {
    int nr;
    unsigned int arch;
    unsigned long long instruction_pointer;
    unsigned long long args[6];
};

/* Check if we're running on Linux at runtime */
static int is_linux(void) {
    /* Try to call a Linux-specific syscall that's safe */
    /* prctl with invalid args will return EINVAL on Linux, ENOSYS on others */
    int ret = prctl(PR_SET_NO_NEW_PRIVS, 0, 0, 0, 0);
    return (ret == 0 || errno == EINVAL || errno == EPERM);
}

/* Drop privileges to nobody user if running as root */
static void drop_privileges(void) {
    uid_t uid = getuid();
    if (uid == 0) {
        /* Try to setuid to nobody (usually uid 65534) */
        uid_t nobody_uid = 65534;
        gid_t nobody_gid = 65534;

        if (setgid(nobody_gid) != 0) {
            fprintf(stderr, "Warning: Failed to drop group privileges\n");
        }
        if (setuid(nobody_uid) != 0) {
            fprintf(stderr, "Warning: Failed to drop user privileges\n");
        }
    }
}

/* Install seccomp filter */
static int install_seccomp_filter(struct sock_filter *filter, size_t filter_len) {
    struct sock_fprog prog = {
        .len = (unsigned short)filter_len,
        .filter = filter
    };

    /* Set no_new_privs to enable seccomp without CAP_SYS_ADMIN */
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        perror("prctl(PR_SET_NO_NEW_PRIVS)");
        return -1;
    }

    /* Install seccomp filter */
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, (unsigned long)&prog, 0, 0) != 0) {
        perror("prctl(PR_SET_SECCOMP)");
        return -1;
    }

    return 0;
}

/* Build BPF filter dynamically based on config */
static int build_and_install_filter(sandbox_config_t* config) {
    /* Maximum filter size (allow some headroom) */
    struct sock_filter filter[256];
    int idx = 0;

    /* Load syscall architecture */
    filter[idx++] = BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                             (offsetof(struct seccomp_data, arch)));

    /* Verify architecture matches (kill process if not) */
    filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_CURRENT, 1, 0);
    filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL);

    /* Load syscall number */
    filter[idx++] = BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                             (offsetof(struct seccomp_data, nr)));

    /* Block write syscalls if not allowed */
    if (!config->allow_write) {
        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_WRITE_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_OPEN_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_CREAT_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_UNLINK_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_MKDIR_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_RMDIR_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));
    }

    /* Block network syscalls if not allowed */
    if (!config->allow_net) {
        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_SOCKET_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_CONNECT_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_BIND_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_LISTEN_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_ACCEPT_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));
    }

    /* Block exec syscalls if not allowed */
    if (!config->allow_exec) {
        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_EXECVE_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_FORK_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_VFORK_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));

        filter[idx++] = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_CLONE_X64, 0, 1);
        filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (EACCES & SECCOMP_RET_DATA));
    }

    /* Allow all other syscalls */
    filter[idx++] = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);

    return install_seccomp_filter(filter, idx);
}

/* Public API implementation */

int cosmo_sandbox_enable(sandbox_config_t* config) {
    if (!config) {
        fprintf(stderr, "Error: NULL sandbox configuration\n");
        return -1;
    }

    /* Check if we're running on Linux at runtime */
    if (!is_linux()) {
        fprintf(stderr, "Warning: Sandboxing not supported on this platform (Linux only)\n");
        fprintf(stderr, "Warning: Program will run without syscall restrictions\n");
        return 0;  /* Return success on non-Linux platforms (graceful degradation) */
    }

    fprintf(stderr, "[Sandbox] Enabling with: write=%s, net=%s, exec=%s\n",
            config->allow_write ? "allowed" : "blocked",
            config->allow_net ? "allowed" : "blocked",
            config->allow_exec ? "allowed" : "blocked");

    /* Drop privileges if running as root */
    drop_privileges();

    /* Build and install seccomp filter */
    if (build_and_install_filter(config) != 0) {
        fprintf(stderr, "Error: Failed to install seccomp filter\n");
        return -1;
    }

    fprintf(stderr, "[Sandbox] Seccomp filter installed successfully\n");
    return 0;
}

void cosmo_sandbox_disable(void) {
    if (is_linux()) {
        fprintf(stderr, "Warning: Seccomp filters cannot be removed once applied\n");
    }
}
