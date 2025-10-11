// Minimal Lua runtime module for cosmorun (TCC-compatible version)
// Simplified to work with TCC by embedding essential Lua amalgamation.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __static_yoink
#define __static_yoink(sym)
#endif

// Lua amalgamation - include the single-file Lua distribution
// This embeds the entire Lua implementation in one compilation unit
#define LUA_IMPL
#define luaall_c

// We need to provide the Cosmopolitan-specific definitions that Lua expects
#define COSMOPOLITAN_C_START_
#define COSMOPOLITAN_C_END_

// Include Lua amalgamation components in correct order
#include "third_party/cosmopolitan/third_party/lua/onelua.c"

static lua_State *g_lua = NULL;
static char *g_last_error = NULL;

static void modlua_clear_error(void) {
  if (g_last_error) {
    free(g_last_error);
    g_last_error = NULL;
  }
}

static void modlua_capture_error(lua_State *L) {
  modlua_clear_error();
  const char *msg = lua_tostring(L, -1);
  if (msg) {
    g_last_error = strdup(msg);
  } else {
    g_last_error = strdup("unknown Lua error");
  }
  lua_pop(L, 1);
}

int mod_lua_init(void) {
  if (g_lua) {
    return 0;
  }
  g_lua = luaL_newstate();
  if (!g_lua) {
    modlua_clear_error();
    g_last_error = strdup("luaL_newstate failed");
    return -1;
  }
  luaL_openlibs(g_lua);
  return 0;
}

void mod_lua_close(void) {
  if (g_lua) {
    lua_close(g_lua);
    g_lua = NULL;
  }
  modlua_clear_error();
}

int mod_lua_eval(const char *chunk) {
  if (!chunk) {
    modlua_clear_error();
    g_last_error = strdup("chunk is NULL");
    return -1;
  }
  if (!g_lua && mod_lua_init() != 0) {
    return -1;
  }
  if (luaL_loadstring(g_lua, chunk) != LUA_OK) {
    modlua_capture_error(g_lua);
    return -1;
  }
  if (lua_pcall(g_lua, 0, LUA_MULTRET, 0) != LUA_OK) {
    modlua_capture_error(g_lua);
    return -1;
  }
  modlua_clear_error();
  return 0;
}

int mod_lua_eval_file(const char *path) {
  if (!path) {
    modlua_clear_error();
    g_last_error = strdup("path is NULL");
    return -1;
  }
  if (!g_lua && mod_lua_init() != 0) {
    return -1;
  }
  if (luaL_loadfile(g_lua, path) != LUA_OK) {
    modlua_capture_error(g_lua);
    return -1;
  }
  if (lua_pcall(g_lua, 0, LUA_MULTRET, 0) != LUA_OK) {
    modlua_capture_error(g_lua);
    return -1;
  }
  modlua_clear_error();
  return 0;
}

const char *mod_lua_last_error(void) {
  return g_last_error ? g_last_error : "";
}

double mod_lua_getglobal_number(const char *name, double fallback) {
  if (!g_lua || !name) {
    return fallback;
  }
  lua_getglobal(g_lua, name);
  double value = fallback;
  if (lua_isnumber(g_lua, -1)) {
    value = lua_tonumber(g_lua, -1);
  }
  lua_pop(g_lua, 1);
  return value;
}

const char *mod_lua_getglobal_string(const char *name) {
  if (!g_lua || !name) {
    return NULL;
  }
  lua_getglobal(g_lua, name);
  if (!lua_isstring(g_lua, -1)) {
    lua_pop(g_lua, 1);
    return NULL;
  }
  const char *value = lua_tostring(g_lua, -1);
  lua_pop(g_lua, 1);
  return value;
}

// Clean up automatically if the module is unloaded.
__attribute__((destructor))
static void mod_lua_shutdown(void) {
  mod_lua_close();
}
