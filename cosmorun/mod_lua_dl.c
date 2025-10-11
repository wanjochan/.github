/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Lua 5.4 Dynamic Library Wrapper for cosmorun                               │
│ Loads Lua via dlopen() - architecture-specific .dylib/.so/.dll             │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "cosmo_libc.h"

// Lua API function pointers
static void *lua_lib_handle = NULL;
static char last_error_msg[1024] = {0};

// Lua state type (opaque)
typedef struct lua_State lua_State;

// Lua API function pointer types
typedef lua_State *(*luaL_newstate_t)(void);
typedef void (*lua_close_t)(lua_State *);
typedef int (*luaL_loadstring_t)(lua_State *, const char *);
typedef int (*luaL_loadfilex_t)(lua_State *, const char *, const char *);
typedef int (*lua_pcallk_t)(lua_State *, int, int, int, void *, void *);
typedef void (*luaL_openlibs_t)(lua_State *);
typedef int (*lua_getglobal_t)(lua_State *, const char *);
typedef int (*lua_isnumber_t)(lua_State *, int);
typedef double (*lua_tonumberx_t)(lua_State *, int, int *);
typedef const char *(*lua_tolstring_t)(lua_State *, int, size_t *);
typedef void (*lua_pop_t)(lua_State *, int);
typedef void (*lua_settop_t)(lua_State *, int);

// Function pointers
static luaL_newstate_t luaL_newstate_ptr = NULL;
static lua_close_t lua_close_ptr = NULL;
static luaL_loadstring_t luaL_loadstring_ptr = NULL;
static luaL_loadfilex_t luaL_loadfilex_ptr = NULL;
static lua_pcallk_t lua_pcallk_ptr = NULL;
static luaL_openlibs_t luaL_openlibs_ptr = NULL;
static lua_getglobal_t lua_getglobal_ptr = NULL;
static lua_isnumber_t lua_isnumber_ptr = NULL;
static lua_tonumberx_t lua_tonumberx_ptr = NULL;
static lua_tolstring_t lua_tolstring_ptr = NULL;
static lua_settop_t lua_settop_ptr = NULL;

// Lua state
static lua_State *L = NULL;

// Default allocator for lua_newstate
static void *lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

// Load Lua library
static int load_lua_library(void) {
  if (lua_lib_handle) {
    return 0;  // Already loaded
  }

  // Determine library name based on OS and architecture
  const char *lib_name = NULL;
#if defined(__APPLE__)
  #if defined(__aarch64__) || defined(__arm64__)
    lib_name = "lib/lua5.4-arm-64.dylib";
  #else
    lib_name = "lib/lua5.4-x86-64.dylib";
  #endif
#elif defined(__linux__)
  #if defined(__aarch64__) || defined(__arm64__)
    lib_name = "lib/lua5.4-arm-64.so";
  #else
    lib_name = "lib/lua5.4-x86-64.so";
  #endif
#elif defined(_WIN32)
  #if defined(__aarch64__) || defined(__arm64__)
    lib_name = "lib/lua5.4-arm-64.dll";
  #else
    lib_name = "lib/lua5.4-x86-64.dll";
  #endif
#else
  snprintf(last_error_msg, sizeof(last_error_msg),
           "Unsupported platform");
  return -1;
#endif

  // Load the library
  printf("[mod_lua_dl] Loading %s...\n", lib_name);
  lua_lib_handle = dlopen(lib_name, RTLD_LAZY | RTLD_LOCAL);
  if (!lua_lib_handle) {
    snprintf(last_error_msg, sizeof(last_error_msg),
             "Failed to load %s: %s", lib_name, dlerror());
    printf("[mod_lua_dl] Error: %s\n", last_error_msg);
    return -1;
  }
  printf("[mod_lua_dl] Library loaded successfully\n");

  // Resolve function pointers
  #define LOAD_SYM(name) do { \
    name##_ptr = dlsym(lua_lib_handle, #name); \
    if (!name##_ptr) { \
      snprintf(last_error_msg, sizeof(last_error_msg), \
               "Symbol not found: %s", #name); \
      dlclose(lua_lib_handle); \
      lua_lib_handle = NULL; \
      return -1; \
    } \
  } while(0)

  LOAD_SYM(luaL_newstate);
  LOAD_SYM(lua_close);
  LOAD_SYM(luaL_loadstring);
  LOAD_SYM(luaL_loadfilex);
  LOAD_SYM(lua_pcallk);
  LOAD_SYM(luaL_openlibs);
  LOAD_SYM(lua_getglobal);
  LOAD_SYM(lua_isnumber);
  LOAD_SYM(lua_tonumberx);
  LOAD_SYM(lua_tolstring);
  LOAD_SYM(lua_settop);

  #undef LOAD_SYM

  return 0;
}

// Initialize Lua
int mod_lua_init(void) {
  if (L) {
    snprintf(last_error_msg, sizeof(last_error_msg),
             "Lua already initialized");
    return -1;
  }

  if (load_lua_library() != 0) {
    return -1;
  }

  L = luaL_newstate_ptr();
  if (!L) {
    snprintf(last_error_msg, sizeof(last_error_msg),
             "Failed to create Lua state");
    return -1;
  }

  luaL_openlibs_ptr(L);
  last_error_msg[0] = '\0';
  return 0;
}

// Evaluate Lua code string
int mod_lua_eval(const char *code) {
  if (!L) {
    snprintf(last_error_msg, sizeof(last_error_msg),
             "Lua not initialized");
    return -1;
  }

  int status = luaL_loadstring_ptr(L, code);
  if (status != 0) {  // LUA_OK = 0
    const char *err = lua_tolstring_ptr(L, -1, NULL);
    snprintf(last_error_msg, sizeof(last_error_msg),
             "Load error: %s", err ? err : "unknown");
    lua_settop_ptr(L, -2);  // lua_pop(L, 1)
    return -1;
  }

  status = lua_pcallk_ptr(L, 0, 0, 0, NULL, NULL);
  if (status != 0) {
    const char *err = lua_tolstring_ptr(L, -1, NULL);
    snprintf(last_error_msg, sizeof(last_error_msg),
             "Runtime error: %s", err ? err : "unknown");
    lua_settop_ptr(L, -2);  // lua_pop(L, 1)
    return -1;
  }

  return 0;
}

// Evaluate Lua file
int mod_lua_eval_file(const char *filename) {
  if (!L) {
    snprintf(last_error_msg, sizeof(last_error_msg),
             "Lua not initialized");
    return -1;
  }

  int status = luaL_loadfilex_ptr(L, filename, NULL);
  if (status != 0) {
    const char *err = lua_tolstring_ptr(L, -1, NULL);
    snprintf(last_error_msg, sizeof(last_error_msg),
             "Load file error: %s", err ? err : "unknown");
    lua_settop_ptr(L, -2);  // lua_pop(L, 1)
    return -1;
  }

  status = lua_pcallk_ptr(L, 0, 0, 0, NULL, NULL);
  if (status != 0) {
    const char *err = lua_tolstring_ptr(L, -1, NULL);
    snprintf(last_error_msg, sizeof(last_error_msg),
             "Runtime error: %s", err ? err : "unknown");
    lua_settop_ptr(L, -2);  // lua_pop(L, 1)
    return -1;
  }

  return 0;
}

// Get global number variable
double mod_lua_getglobal_number(const char *name, double default_val) {
  if (!L || !name) {
    return default_val;
  }

  lua_getglobal_ptr(L, name);
  if (!lua_isnumber_ptr(L, -1)) {
    lua_settop_ptr(L, -2);  // lua_pop(L, 1)
    return default_val;
  }

  double value = lua_tonumberx_ptr(L, -1, NULL);
  lua_settop_ptr(L, -2);  // lua_pop(L, 1)
  return value;
}

// Get global string variable
const char *mod_lua_getglobal_string(const char *name) {
  static char str_buffer[4096];

  if (!L || !name) {
    return NULL;
  }

  lua_getglobal_ptr(L, name);
  const char *value = lua_tolstring_ptr(L, -1, NULL);
  if (value) {
    strncpy(str_buffer, value, sizeof(str_buffer) - 1);
    str_buffer[sizeof(str_buffer) - 1] = '\0';
  }
  lua_settop_ptr(L, -2);  // lua_pop(L, 1)

  return value ? str_buffer : NULL;
}

// Get last error message
const char *mod_lua_last_error(void) {
  return last_error_msg[0] ? last_error_msg : NULL;
}

// Close Lua state and unload library
void mod_lua_close(void) {
  if (L && lua_close_ptr) {
    lua_close_ptr(L);
    L = NULL;
  }

  if (lua_lib_handle) {
    dlclose(lua_lib_handle);
    lua_lib_handle = NULL;
  }

  last_error_msg[0] = '\0';
}
