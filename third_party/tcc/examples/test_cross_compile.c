#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    printf("Hello from TinyCC Cross-Compiler!\n");
    printf("Architecture: %s\n", 
#ifdef __x86_64__
        "x86_64"
#elif defined(__i386__)
        "i386"
#elif defined(__aarch64__)
        "aarch64"
#elif defined(__arm__)
        "arm"
#else
        "unknown"
#endif
    );
    
    printf("Platform: %s\n",
#ifdef _WIN32
        "Windows"
#elif defined(__linux__)
        "Linux"
#elif defined(__APPLE__)
        "macOS"
#else
        "Unknown"
#endif
    );
    
    printf("Compiler: TinyCC\n");
    printf("Arguments: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  [%d]: %s\n", i, argv[i]);
    }
    
    return 0;
} 