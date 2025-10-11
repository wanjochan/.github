# CosmoRun - 跨平台C代码即时执行系统

基于 TinyCC 和 Cosmopolitan 的跨平台 C 代码即时编译执行系统，支持 Linux、Windows、macOS。

## 🚀 核心特性

- **即时编译执行** - 无需预编译，直接运行 C 代码
- **动态模块加载** - 按需加载功能模块
- **高性能缓存** - `.{arch}.o` 缓存系统，10-100x 加速重复执行
- **跨平台一致** - 同一套代码在所有平台表现一致
- **完整库生态** - SQLite3/Lua/NNG/DuckDB 全面集成

## 📊 已集成的动态库模块

| 模块 | 功能 | 平台支持 |
|------|------|----------|
| **SQLite3** | 嵌入式数据库 | Linux/macOS/Windows |
| **Lua 5.4** | 脚本语言运行时 | Linux/macOS/Windows |
| **NNG** | 网络消息传递 | Linux/macOS (4/6平台) |
| **DuckDB** | 分析型数据库 | Linux/macOS/Windows |

## 🔧 使用方法

### 基本用法
```bash
# 执行 C 源文件
./cosmorun.exe hello.c

# 执行内联 C 代码
./cosmorun.exe -e 'int main(){printf("Hello World\n"); return 0;}'

# 传递参数给 C 程序
./cosmorun.exe program.c arg1 arg2 arg3

# REPL 模式
./cosmorun.exe
>>> int x = 42;
>>> printf("%d\n", x);
42
```

### 模块导入 API
```c
void* cosmo_import(const char* path);          // 加载 C 源码或 .o 缓存
void* cosmo_import_sym(void* module, const char* symbol);  // 获取符号
void cosmo_import_free(void* module);          // 释放模块
```

**使用示例**：
```c
void* m = cosmo_import("math_lib.c");
int (*add)(int,int) = cosmo_import_sym(m, "add");
printf("Result: %d\n", add(2, 3));
cosmo_import_free(m);
```

## 💾 对象缓存系统 (`.{arch}.o`)

### 自动缓存流程

**首次加载** (编译 + 缓存):
- 编译 `math_lib.c` 到内存
- 保存 `math_lib.x86_64.o` 缓存
- 返回加载的模块

**后续加载** (缓存命中):
- 检测 `math_lib.x86_64.o` 存在且较新
- 直接从 .o 文件加载 (10-100x 加速)
- 返回加载的模块

### 缓存文件命名
- **x86-64**: `module.x86_64.o`
- **ARM64**: `module.aarch64.o`
- **ARM32**: `module.armv7l.o`

### 缓存失效
- 源文件修改 → 自动重新编译
- 架构变更 → 使用不同的 `.{arch}.o`
- 手动清理 → 删除 `.o` 文件

### 性能指标
- 小模块: 10-20x 加速
- 中型模块: 50x 加速
- 大型模块: 100x+ 加速

## 🛠️ 自定义 Builtin 函数

CosmoRun 扩展了标准 libc，提供跨平台的动态加载和平台检测能力。

### 动态库加载函数 (`__dlopen` 系列)

**`__dlopen(const char *name, int flags)`**
- **功能**: 智能跨平台动态库加载
- **特性**:
  - 自动平台检测（Windows/macOS/Linux）
  - 自动文件扩展名尝试（`.dll` / `.dylib` / `.so`）
  - 支持相对路径和绝对路径
  - 回退到系统库搜索路径
- **使用示例**:
  ```c
  // 跨平台加载 DuckDB
  void *handle = __dlopen("duckdb", RTLD_LAZY);
  // Windows → 尝试 duckdb.dll
  // macOS   → 尝试 duckdb.dylib, duckdb.so
  // Linux   → 尝试 duckdb.so, duckdb.dylib
  ```

**`__dlsym(void *handle, const char *symbol)`**
- **功能**: 从动态库获取符号
- **返回**: 符号地址，失败返回 NULL

**`__dlclose(void *handle)`**
- **功能**: 关闭动态库句柄
- **返回**: 成功返回 0

### 平台检测函数 (Cosmopolitan APE)

**`IsWindows()`** - 检测是否运行在 Windows
**`IsXnu()`** - 检测是否运行在 macOS
**`IsLinux()`** - 检测是否运行在 Linux

**使用示例**:
```c
if (IsWindows()) {
    printf("Running on Windows\n");
} else if (IsXnu()) {
    printf("Running on macOS\n");
} else if (IsLinux()) {
    printf("Running on Linux\n");
}
```

### 符号解析架构

#### Level 1: 内置符号表
- **内容**: `printf`, `malloc`, `strlen`, `memcpy` 等 ~30 个高频函数
- **性能**: 零延迟访问，微秒级响应

#### Level 2: 系统库搜索
- **Windows**: `ucrtbase.dll`, `msvcrt.dll`, `kernel32.dll`
- **Linux**: `libc.so.6`, `libm.so.6`
- **macOS**: `libSystem.B.dylib`
- **缓存**: 动态库句柄缓存，避免重复加载

## 🏗️ 构建系统

```bash
# 构建 cosmorun.exe (需要 third_party/cosmocc 工具链)
cd cosmorun
./build_cosmorun.sh

# 自动执行测试验证
```

## 📖 技术栈示例

### SQLite3 数据库
```c
#include <sqlite3.h>
sqlite3 *db;
sqlite3_open(":memory:", &db);
sqlite3_exec(db, "CREATE TABLE users(id INTEGER, name TEXT)", NULL, NULL, NULL);
```

### Lua 脚本集成
```c
#include <lua.h>
#include <lauxlib.h>
lua_State *L = luaL_newstate();
luaL_dostring(L, "print('Hello from Lua!')");
```

### DuckDB 分析查询
```c
#include <duckdb.h>
duckdb_database db;
duckdb_open(NULL, &db);
duckdb_query(conn, "SELECT * FROM read_csv('data.csv')", &result);
```

### NNG 网络通信
```c
#include <nng/nng.h>
nng_socket sock;
nng_req0_open(&sock);
nng_dial(sock, "tcp://server:8080", NULL, 0);
```

## 🐛 故障排查

### 调试环境变量
- `COSMORUN_TRACE=1` - 启用详细日志

### 命令行选项
- `-I <path>` - 额外头文件路径（与 TCC/GCC 对齐）
- `-L <path>` - 额外库路径（与 TCC/GCC 对齐）
- `-l <library>` - 链接库（与 TCC/GCC 对齐）

### 日志解读
```bash
[cosmorun] Resolving symbol: printf
[cosmorun] Symbol 'printf' resolved from builtins: 0x456789

[cosmorun] Resolving symbol: atoi
[cosmorun] Symbol 'atoi' resolved from system: 0x7f8b2c4a1b20
```

## 🐳 容器环境使用

### 推荐方式 (需要 Shell 包装)
```bash
# 方式 1: 使用 sh
podman run --rm -v $(pwd):/workspace -w /workspace ubuntu:22.04 \
    sh -c './cosmorun.exe test.c'

# 方式 2: 使用 bash (更好的 APE 支持)
podman run --rm -v $(pwd):/workspace -w /workspace ubuntu:22.04 \
    bash -c './cosmorun.exe test.c'
```

### 原因说明
APE (Actually Portable Executable) 文件格式通过 Shell 的回退机制实现跨平台：
- **主机环境**: Shell 自动回退到脚本执行模式 ✅
- **容器环境**: 容器运行时直接 execve() 失败 ❌
- **解决方案**: 显式使用 Shell 包装

## 🔬 关键技术细节

### TinyCC 集成
- 使用上游 TinyCC (`third_party/tinycc/`) 或补丁版本 (`third_party/tinycc.hack/`)
- macOS 需要 Split Memory 补丁支持 MAP_JIT
- 自动注入常用符号到 TCC 符号表

### 隐式声明返回类型 Hack
**问题**: C 标准规定未声明函数默认返回 `int` (32位)，但指针是 64 位
**解决**: 修改 TinyCC 默认返回类型为 `char_pointer_type` (64位)
**效果**: `malloc`, `strdup` 等无需声明也能正常工作

### Windows 调用约定桥接
- TinyCC 生成 System V 调用约定代码
- Windows API 使用 Microsoft x64 约定
- 运行时自动生成跳板进行参数转换
- 使用 Cosmopolitan 的 `__sysv2nt14` 完成寄存器映射

## 📝 重要经验教训

### 跨平台安全性
**教训**: 永远不要假设"只在一个平台测试就够了"

**错误代码**:
```c
// 危险！缺少 entry 本身的 NULL 检查
for (const SymbolEntry *entry = builtin_symbol_table; entry->name; ++entry) {
    if (strcmp(entry->name, symbol_name) == 0) return entry->address;
}
```

**正确代码**:
```c
// 安全：双重检查保护
for (const SymbolEntry *entry = builtin_symbol_table; entry && entry->name; ++entry) {
    if (!entry->name) break;
    if (strcmp(entry->name, symbol_name) == 0) return entry->address;
}
```

## 🔒 安全考虑
- 动态编译执行 C 代码，需适当的安全策略
- 仅解析标准库函数，不执行任意代码
- 可配合容器或沙箱环境使用
- 智能崩溃处理，避免程序直接终止
