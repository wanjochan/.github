/* Sandboxing for CosmoRun - Restrict syscalls for compiled programs
 *
 * Provides OS-level sandboxing using seccomp-bpf (Linux) to restrict:
 * - File system access (read-only mode)
 * - Network access (block sockets)
 * - Process execution (block exec/fork)
 *
 * Usage:
 *   sandbox_config_t config = {
 *       .allow_write = 0,
 *       .allow_net = 0,
 *       .allow_exec = 0
 *   };
 *   cosmo_sandbox_enable(&config);
 */

#ifndef COSMO_SANDBOX_H
#define COSMO_SANDBOX_H

#ifdef __cplusplus
extern "C" {
#endif

/* Sandbox configuration */
typedef struct {
    int allow_write;    /* Allow write syscalls (open O_CREAT, write, unlink, etc) */
    int allow_net;      /* Allow network syscalls (socket, connect, bind, etc) */
    int allow_exec;     /* Allow exec syscalls (execve, fork, etc) */
} sandbox_config_t;

/* Enable sandboxing with given configuration
 * Returns: 0 on success, -1 on error
 *
 * Platform support:
 * - Linux: Uses seccomp-bpf for syscall filtering
 * - Others: Prints warning and returns success (no-op)
 */
int cosmo_sandbox_enable(sandbox_config_t* config);

/* Disable sandboxing (currently not supported with seccomp)
 * Note: seccomp filters cannot be removed once applied
 */
void cosmo_sandbox_disable(void);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_SANDBOX_H */
