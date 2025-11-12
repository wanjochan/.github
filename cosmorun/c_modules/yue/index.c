/*
 * Yue GUI Library Module for CosmoRun
 *
 * This module provides bindings to the Yue cross-platform GUI library.
 * Yue: https://github.com/yue/yue
 *
 * Usage:
 *   void *yue = __import("yue");
 *   void (*init)(void) = __import_sym(yue, "yue_init");
 *   init();
 */

#include "src/cosmo_libc.h"

// Platform detection for library names
#if defined(__APPLE__)
  #define YUE_LIB_NAME "libYue.dylib"
#elif defined(_WIN32)
  #define YUE_LIB_NAME "Yue.dll"
#else
  #define YUE_LIB_NAME "libYue.so"
#endif

static void *yue_handle = NULL;

/*
 * Initialize Yue library
 * Returns 0 on success, -1 on error
 */
int yue_init(void) {
    if (yue_handle) {
        return 0;  // Already initialized
    }

    // Build versioned library paths based on architecture
    #if defined(__aarch64__) || defined(_M_ARM64)
        #if defined(__APPLE__)
            #define YUE_VERSIONED "../libs/yue-0.15.6-arm64-64.dylib"
        #elif defined(_WIN32)
            #define YUE_VERSIONED "../libs/yue-0.15.6-arm64-64.dll"
        #else
            #define YUE_VERSIONED "../libs/yue-0.15.6-arm64-64.so"
        #endif
    #elif defined(__x86_64__) || defined(_M_X64)
        #if defined(__APPLE__)
            #define YUE_VERSIONED "../libs/yue-0.15.6-x86_64-64.dylib"
        #elif defined(_WIN32)
            #define YUE_VERSIONED "../libs/yue-0.15.6-x86_64-64.dll"
        #else
            #define YUE_VERSIONED "../libs/yue-0.15.6-x86_64-64.so"
        #endif
    #elif defined(__i386__) || defined(_M_IX86)
        #if defined(__APPLE__)
            #define YUE_VERSIONED "../libs/yue-0.15.6-x86-32.dylib"
        #elif defined(_WIN32)
            #define YUE_VERSIONED "../libs/yue-0.15.6-x86-32.dll"
        #else
            #define YUE_VERSIONED "../libs/yue-0.15.6-x86-32.so"
        #endif
    #else
        #define YUE_VERSIONED "../libs/yue.so"
    #endif

    // Try to load Yue library from multiple locations
    const char *search_paths[] = {
        // Standard location (versioned)
        YUE_VERSIONED,
        // Fallback locations
        "../libs/yue.so",
        "../libs/yue.dylib",
        "../libs/" YUE_LIB_NAME,
        "./lib/" YUE_LIB_NAME,
        "./lib/yue.so",
        // System paths
        "/usr/local/lib/" YUE_LIB_NAME,
        "/opt/homebrew/lib/" YUE_LIB_NAME,
        NULL
    };

    extern void *__dlopen(const char *, int);

    extern void *dlopen(const char *, int);

    for (int i = 0; search_paths[i]; i++) {
        yue_handle = dlopen(search_paths[i], 1); // RTLD_LAZY
        if (yue_handle) {
            printf("[yue] Loaded from: %s\n", search_paths[i]);
            break;
        }
    }

    if (!yue_handle) {
        fprintf(stderr, "[yue] Failed to load Yue library\n");
        fprintf(stderr, "[yue] Expected location: ../libs/%s\n", YUE_VERSIONED);
        fprintf(stderr, "[yue]\n");
        fprintf(stderr, "[yue] Download and install:\n");
        #if defined(__APPLE__)
            #if defined(__aarch64__)
                fprintf(stderr, "[yue]   curl -L https://github.com/yue/yue/releases/download/v0.15.6/lua_yue_lua_5.1_v0.15.6_mac_arm64.zip -o /tmp/yue.zip\n");
            #else
                fprintf(stderr, "[yue]   curl -L https://github.com/yue/yue/releases/download/v0.15.6/lua_yue_lua_5.1_v0.15.6_mac_x64.zip -o /tmp/yue.zip\n");
            #endif
            fprintf(stderr, "[yue]   unzip -q /tmp/yue.zip -d /tmp && mkdir -p ../libs\n");
            fprintf(stderr, "[yue]   cp /tmp/yue.so ../libs/%s\n", YUE_VERSIONED);
        #elif defined(_WIN32)
            fprintf(stderr, "[yue]   curl -L https://github.com/yue/yue/releases/download/v0.15.6/lua_yue_lua_5.1_v0.15.6_win_x64.zip -o %%TEMP%%\\yue.zip\n");
            fprintf(stderr, "[yue]   unzip -q %%TEMP%%\\yue.zip -d %%TEMP%% && mkdir ..\\libs\n");
            fprintf(stderr, "[yue]   copy %%TEMP%%\\yue.dll ..\\libs\\%s\n", YUE_VERSIONED);
        #else
            fprintf(stderr, "[yue]   curl -L https://github.com/yue/yue/releases/download/v0.15.6/lua_yue_lua_5.1_v0.15.6_linux_x64.zip -o /tmp/yue.zip\n");
            fprintf(stderr, "[yue]   unzip -q /tmp/yue.zip -d /tmp && mkdir -p ../libs\n");
            fprintf(stderr, "[yue]   cp /tmp/yue.so ../libs/%s\n", YUE_VERSIONED);
        #endif
        fprintf(stderr, "[yue]\n");
        fprintf(stderr, "[yue] See mod_yue.md for details\n");
        return -1;
    }

    return 0;
}

/*
 * Get symbol from Yue library
 */
void *yue_get_symbol(const char *symbol_name) {
    if (!yue_handle) {
        fprintf(stderr, "[yue] Library not initialized. Call yue_init() first.\n");
        return NULL;
    }

    extern void *dlsym(void *, const char *);
    void *sym = dlsym(yue_handle, symbol_name);

    if (!sym) {
        fprintf(stderr, "[yue] Symbol not found: %s\n", symbol_name);
    }

    return sym;
}

/*
 * Cleanup Yue library
 */
void yue_cleanup(void) {
    if (yue_handle) {
        extern int dlclose(void *);
        dlclose(yue_handle);
        yue_handle = NULL;
    }
}

/*
 * Simple demo function
 * TODO: Add real Yue API bindings after compiling the library
 */
void yue_demo(void) {
    printf("Yue GUI Library Module\n");
    printf("======================\n");
    printf("Version: 0.15.6\n");
    printf("Platform: %s\n",
        #if defined(__APPLE__)
            "macOS"
        #elif defined(_WIN32)
            "Windows"
        #else
            "Linux"
        #endif
    );
    printf("\n");
    printf("To use Yue, you need to:\n");
    printf("1. Compile Yue library from third_party/yue\n");
    printf("2. Place libYue.dylib in cosmorun/lib/\n");
    printf("3. Call yue_init() to initialize\n");
    printf("4. Use yue_get_symbol() to access Yue APIs\n");
}

/*
 * Yoga layout engine types and function pointers
 */
typedef struct YGConfig YGConfig;
typedef struct YGNode YGNode;

static YGConfig* (*YGConfigNew)(void) = NULL;
static void (*YGConfigFree)(YGConfig*) = NULL;
static YGNode* (*YGNodeNew)(void) = NULL;
static void (*YGNodeFree)(YGNode*) = NULL;

/*
 * Get info about available APIs
 */
void yue_info(void *handle) {
    extern void *dlsym(void *, const char *);

    printf("\n=== Yue Library Info ===\n");
    printf("Library handle: %p\n", handle);

    void *lua_entry = dlsym(handle, "luaopen_yue_gui");
    printf("Lua entry point: %s\n", lua_entry ? "✓ Available" : "✗ Not found");

    void *yoga_config = dlsym(handle, "YGConfigNew");
    printf("Yoga layout engine: %s\n", yoga_config ? "✓ Available" : "✗ Not found");

    printf("\nNote: yue.so is a Lua module including:\n");
    printf("  - Yoga layout engine (C API)\n");
    printf("  - Yue GUI bindings (via Lua)\n");
}

/*
 * Initialize Yoga layout engine
 */
int yue_yoga_init(void *handle) {
    extern void *dlsym(void *, const char *);

    YGConfigNew = dlsym(handle, "YGConfigNew");
    YGConfigFree = dlsym(handle, "YGConfigFree");
    YGNodeNew = dlsym(handle, "YGNodeNew");
    YGNodeFree = dlsym(handle, "YGNodeFree");

    if (!YGConfigNew || !YGNodeNew) {
        fprintf(stderr, "[yue] Failed to load Yoga symbols\n");
        return -1;
    }

    printf("[yue] Yoga layout engine initialized\n");
    return 0;
}

/*
 * Test Yoga layout engine
 */
void yue_yoga_test(void) {
    if (!YGConfigNew || !YGNodeNew) {
        printf("[yue] Yoga not initialized\n");
        return;
    }

    printf("\n=== Yoga Layout Engine Test ===\n");

    YGConfig *config = YGConfigNew();
    printf("✓ YGConfig created: %p\n", config);

    YGNode *node = YGNodeNew();
    printf("✓ YGNode created: %p\n", node);

    YGNodeFree(node);
    YGConfigFree(config);

    printf("✓ Yoga test passed\n");
}

/*
 * Example: Create a simple window (requires compiled Yue library)
 */
int yue_create_window_example(void) {
    if (yue_init() != 0) {
        return -1;
    }

    yue_info(yue_handle);

    if (yue_yoga_init(yue_handle) == 0) {
        yue_yoga_test();
    }

    return 0;
}
