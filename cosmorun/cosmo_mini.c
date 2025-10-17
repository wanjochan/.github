//../third_party/cosmocc/bin/cosmocc -D BUILD_COSMO_MINI cosmo_mini.c cosmo_run.c cosmo_tcc.c cosmo_utils.c xdl.c ../third_party/tinycc.hack/libtcc.c -I ../third_party/tinycc.hack/ -o cosmorun_mini.exe && lst -al cosmorun_mini.exe

//../third_party/cosmocc/bin/cosmocc -D BUILD_COSMO_MINI cosmo_mini.c xdl.c -I ../third_party/tinycc.hack/ -o cosmorun_mini.exe && ls -al cosmorun_mini.exe
//#include "cosmo_libc.h"
//#include "xdl.h"

//46XKB
//../third_party/cosmocc/bin/cosmocc -D BUILD_COSMO_MINI cosmo_mini.c -I ../third_party/tinycc.hack/ -o cosmorun_mini.exe && ls -al cosmorun_mini.exe

//23XKB
//../third_party/cosmocc/bin/cosmocc -mtiny -Os -D BUILD_COSMO_MINI cosmo_mini.c -I ../third_party/tinycc.hack/ -o cosmorun_mini.exe && ls -al cosmorun_mini.exe

#include "cosmo_libc.h"
//#include "cosmo_tcc.h"
//#include "cosmo_utils.h"

#ifdef BUILD_COSMO_MINI
extern char **environ;

// Detect current CPU architecture and return binary name
const char* detect_arch_binary() {
    struct utsname buf;
    if (uname(&buf) != 0) {
        return NULL;
    }

    // Map uname machine to our binary naming scheme
    if (strcmp(buf.machine, "x86_64") == 0 || strcmp(buf.machine, "amd64") == 0) {
        return "cosmorun-x86-64.exe";
    } else if (strcmp(buf.machine, "aarch64") == 0 || strcmp(buf.machine, "arm64") == 0) {
        return "cosmorun-arm-64.exe";
    } else if (strstr(buf.machine, "riscv") != NULL) {
        return "cosmorun-risc-64.exe";
    }

    return NULL;
}

// Find architecture-specific binary in same directory as this executable
const char* find_arch_binary(const char *argv0, const char *arch_binary) {
    static char path[PATH_MAX];

    // Get directory of current executable
    char *argv0_copy = strdup(argv0);
    char *dir = dirname(argv0_copy);

    // Build path: <dir>/<arch_binary>
    snprintf(path, sizeof(path), "%s/%s", dir, arch_binary);
    free(argv0_copy);

    // Check if file exists and is executable
    if (access(path, X_OK) == 0) {
        return path;
    }

    // Try current directory as fallback
    snprintf(path, sizeof(path), "./%s", arch_binary);
    if (access(path, X_OK) == 0) {
        return path;
    }

    return NULL;
}

// Stub: Download architecture-specific binary from remote
int download_arch_binary(const char *arch_binary) {
    // TODO: Implement remote download logic
    // For now, just show an error message
    fprintf(stderr, "\n");
    fprintf(stderr, "=================================================\n");
    fprintf(stderr, "Error: Architecture-specific binary not found\n");
    fprintf(stderr, "=================================================\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Missing file: %s\n", arch_binary);
    fprintf(stderr, "\n");
    fprintf(stderr, "Please download it from:\n");
    fprintf(stderr, "curl -L https://github.com/partnernetsoftware/.github/blob/main/cosmorun/cosmorun.exe\n");
    fprintf(stderr, "\n");

    return -1;
}

int main(int argc, char **argv) {
    // Detect architecture
    const char *arch_binary = detect_arch_binary();
    if (!arch_binary) {
        fprintf(stderr, "Error: Cannot detect CPU architecture\n");
        return 1;
    }

    // Find architecture-specific binary
    const char *binary_path = find_arch_binary(argv[0], arch_binary);

    // If not found, try to download (stub for now)
    if (!binary_path) {
        if (download_arch_binary(arch_binary) != 0) {
            return 1;
        }
        // After download, try to find again
        binary_path = find_arch_binary(argv[0], arch_binary);
        if (!binary_path) {
            fprintf(stderr, "Error: Failed to locate %s after download\n", arch_binary);
            return 1;
        }
    }

    // Execute architecture-specific binary
    // Pass environ instead of NULL to avoid EFAULT (Bad address)
    execve(binary_path, argv, environ);

    // execve only returns on error
    perror("execve failed");
    fprintf(stderr, "Failed to execute: %s\n", binary_path);
    return 1;
}
#endif
