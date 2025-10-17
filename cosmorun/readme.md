# Futu OpenD API Integration

## 概述

本目录包含与 Futu OpenD (富途牛牛) 通信的 Protobuf API 集成代码，用于下载市场数据、交易等功能。

**✅ 已完成**: 功能完整的CLI工具 (`futu_cli.c`)，可通过 `cosmorun.exe` 直接运行。

**初始化接口 (1000+)**:
- InitConnect (1001) - 初始化连接（所有命令自动执行）
- GetUserInfo (1005) - 获取用户信息
- GetGlobalState (1002) - 获取市场状态
- KeepAlive (1004) - 心跳保活

**行情接口 (3000+)**:
- Qot_GetBasicQot (3004) - 获取实时行情（自动订阅）
- Qot_RequestHistoryKL (3103) - 获取K线数据
- Qot_GetOrderBook (3012) - 获取盘口数据

**交易接口 (2000+)** ⚠️ 真实账户:
- Trd_GetAccList (2001) - 获取账户列表
- Trd_UnlockTrade (2005) - 解锁交易
- Trd_GetFunds (2101) - 获取资金
- Trd_GetPositionList (2201) - 获取持仓
- Trd_PlaceOrder (2202) - 下单（封装完成，慎用！）

**🔑 关键发现**: Futu协议实际使用**小端序 (little-endian)**，与官方文档描述的大端序不符！

---

## API 接口覆盖清单

### ✅ 已实现 (13个)

**初始化 (4/4)**
- 1001 InitConnect, 1002 GetGlobalState, 1004 KeepAlive, 1005 GetUserInfo

**行情 (4个核心)**
- 3001 Qot_Sub, 3004 Qot_GetBasicQot, 3012 Qot_GetOrderBook, 3103 Qot_RequestHistoryKL

**交易 (5个核心)**
- 2001 Trd_GetAccList, 2005 Trd_UnlockTrade, 2101 Trd_GetFunds, 2201 Trd_GetPositionList, 2202 Trd_PlaceOrder

### ❌ 常用未实现

**行情**: 3010 Qot_GetTicker (逐笔), 3006 Qot_GetKL (实时K线), 3005 Qot_GetRT (分时), 3201 Qot_GetMarketState (市场状态)

**交易**: 2205 Trd_ModifyOrder (改单/撤单 🔴), 2211 Trd_GetOrderList (查询订单 🔴), 2111 Trd_GetMaxTrdQtys (最大交易量 🔴)

---

## 快速开始

### 安装和启动 OpenD
1. 下载：https://www.moomoo.com/download/OpenAPI
2. 启动 OpenD 并登录
3. 启用 API 访问（设置 → API接口 → 开启）
4. 配置端口：11111（默认）

### 使用 CLI 工具（推荐）

```bash
cd /workspace/self-evolve-ai/cosmorun

# 初始化命令
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- init       # 初始化连接
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- userinfo   # 用户信息
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- state      # 市场状态
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- all        # 所有init命令

# 行情命令（自动 init + 订阅）
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- quote 1 00700           # 实时行情
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- kline 1 00700 2 1 10    # K线(日线,前复权,10条)
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- orderbook 1 00700 10    # 盘口(10档)

# 交易命令 ⚠️ 真实账户！
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- acclist                 # 账户列表
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- unlock <pwd_md5>        # 解锁交易
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- funds 123456 1          # 资金
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- position 123456 1       # 持仓
# 下单（慎用！）: order <acc_id> <market> <side> <type> <code> <price> <qty>
```

**参数说明**:
- **market**: 1=HK, 11=US, 21=SH, 22=SZ
- **kl_type**: 1=1分钟, 2=日线, 3=周线, 4=月线, 7=5分钟, 8=15分钟, 9=30分钟, 10=60分钟
- **rehab_type**: 0=不复权, 1=前复权, 2=后复权
- **trd_side**: 1=买入, 2=卖出
- **order_type**: 0=限价单, 1=市价单

**注意**：
- 使用 `--` 分隔源文件和程序参数
- 所有命令自动执行 InitConnect，无需手动 init
- 交易命令连接的是真实账户，下单前请三思！

---

## 环境搭建流程

**注意**: 以下环境搭建仅用于生成proto参考代码或扩展新API，不是运行 `futu_cli.c` 的必要条件。

### 1. 下载 Protobuf 编译器 (protoc)

**来源**: https://github.com/protocolbuffers/protobuf/releases

**版本**: v28.3

**下载命令**:
```bash
cd /tmp
wget https://github.com/protocolbuffers/protobuf/releases/download/v28.3/protoc-28.3-linux-x86_64.zip
unzip protoc-28.3-linux-x86_64.zip
mkdir -p /workspace/self-evolve-ai/third_party/protoc
cp bin/protoc /workspace/self-evolve-ai/third_party/protoc/
chmod +x /workspace/self-evolve-ai/third_party/protoc/protoc
```

**验证**:
```bash
/workspace/self-evolve-ai/third_party/protoc/protoc --version
# 输出: libprotoc 28.3
```

### 2. 下载 nanopb 工具链

**来源**: https://github.com/nanopb/nanopb

**版本**: latest (commit from 2025-10-17)

**下载命令**:
```bash
cd /workspace/self-evolve-ai/third_party
git clone --depth=1 https://github.com/nanopb/nanopb.git
```

**依赖安装**:
```bash
pip install --user protobuf grpcio-tools
```

**验证**:
```bash
ls /workspace/self-evolve-ai/third_party/nanopb/generator/nanopb_generator.py
python3 /workspace/self-evolve-ai/third_party/nanopb/generator/nanopb_generator.py --version
```

### 3. 下载 Futu API Proto 文件

**来源**: https://github.com/futuopen/ftapi4go

**版本**: 9.0.5008 (2025-03-06)

**下载命令**:
```bash
cd /tmp
git clone --depth=1 https://github.com/futuopen/ftapi4go.git futu-api
mkdir -p /workspace/self-evolve-ai/third_party/futu-proto
cp -r futu-api/FTAPIProtoFiles_9.0.5008/* /workspace/self-evolve-ai/third_party/futu-proto/
```

**文件清单** (70个proto文件):
```
Common.proto                    # 公共类型定义
InitConnect.proto               # 连接初始化
KeepAlive.proto                 # 心跳保活
GetGlobalState.proto            # 获取全局状态
GetUserInfo.proto               # 获取用户信息

Qot_*.proto (46个文件)          # 行情相关API
├── Qot_GetBasicQot.proto       # 获取基础报价
├── Qot_GetKL.proto             # 获取K线数据
├── Qot_GetTicker.proto         # 获取逐笔成交
├── Qot_GetOrderBook.proto      # 获取买卖盘
└── ...

Trd_*.proto (17个文件)          # 交易相关API
├── Trd_PlaceOrder.proto        # 下单
├── Trd_GetOrderList.proto      # 获取订单列表
├── Trd_GetPositionList.proto   # 获取持仓
└── ...
```

### 4. 生成 C 代码

**生成基础 proto 文件的 C 代码**:
```bash
cd /workspace/self-evolve-ai/cosmorun/futulab

export PATH=/workspace/self-evolve-ai/third_party/protoc:$PATH

python3 /workspace/self-evolve-ai/third_party/nanopb/generator/nanopb_generator.py \
    /workspace/self-evolve-ai/third_party/futu-proto/Common.proto \
    /workspace/self-evolve-ai/third_party/futu-proto/KeepAlive.proto \
    /workspace/self-evolve-ai/third_party/futu-proto/InitConnect.proto \
    -I/workspace/self-evolve-ai/third_party/futu-proto
```

**输出文件**:
- `Common.pb.c` / `Common.pb.h`
- `KeepAlive.pb.c` / `KeepAlive.pb.h`
- `InitConnect.pb.c` / `InitConnect.pb.h`

**复制 nanopb 运行时库**:
```bash
cp /workspace/self-evolve-ai/third_party/nanopb/pb*.c .
cp /workspace/self-evolve-ai/third_party/nanopb/pb*.h .
```

**生成的文件**:
- `pb_common.c` / `pb_common.h`
- `pb_encode.c` / `pb_encode.h`
- `pb_decode.c` / `pb_decode.h`
- `pb.h`

## 目录结构

```
cosmorun/futulab/
├── readme.md                   # 本文档
├── futu_utils.h/c              # ✅ 公共工具库（protobuf、SHA1、网络通信）
├── futu_cli.c                  # ✅ 完整CLI工具（支持基础命令+行情查询）
├── futu_simple.c               # 简单TCP连接测试
├── futu_main.c                 # protobuf客户端示例（早期版本，参考用）
├── Common.pb.c/h               # 生成的protobuf代码
├── InitConnect.pb.c/h
├── KeepAlive.pb.c/h
├── pb_common.c/h               # nanopb运行时
├── pb_encode.c/h
├── pb_decode.c/h
└── pb.h

third_party/
├── protoc/
│   └── protoc                  # protobuf编译器
├── nanopb/
│   ├── generator/
│   │   └── nanopb_generator.py # nanopb代码生成器
│   ├── pb_common.c/h
│   ├── pb_encode.c/h
│   ├── pb_decode.c/h
│   └── pb.h
└── futu-proto/
    ├── Common.proto
    ├── Qot_*.proto (46个)
    ├── Trd_*.proto (17个)
    └── ... (其他proto文件)
```

## 编译与测试

### ✅ 推荐方式: 使用 futu_cli.c (完整CLI工具)

```bash
cd /workspace/self-evolve-ai/cosmorun

# 初始化连接
./cosmorun.exe futulab/futu_cli.c init

# 获取用户信息
./cosmorun.exe futulab/futu_cli.c userinfo

# 获取市场状态
./cosmorun.exe futulab/futu_cli.c state

# 发送心跳
./cosmorun.exe futulab/futu_cli.c keepalive

# 执行所有命令
./cosmorun.exe futulab/futu_cli.c all
```

**测试输出示例**:
```
=== InitConnect ===
  retType: 0 (Success)
  serverVer: 904
  loginUserID: .....
  connID: 7384812954075817955
  keepAliveInterval: 10 seconds

=== GetUserInfo ===
  retType: 0 (Success)
  nickName: ....
  userID: .....
  hkQotRight: 3
  usQotRight: 5
  subQuota: 2000

=== GetGlobalState ===
  retType: 0 (Success)
  marketHK: 4
  marketUS: 11
  qotLogined: true
  trdLogined: true
  serverVer: 904
```

**特点**:
- ✅ 完整的protobuf协议支持（手动编码/解码）
- ✅ 内置SHA1哈希计算
- ✅ TCC兼容，无需GCC/Clang
- ✅ 单文件实现，只依赖 `cosmo_libc.h`
- ✅ 支持4个核心API + all命令

### 方式2: 使用 futu_simple.c (连接测试)

```bash
cd /workspace/self-evolve-ai/cosmorun
./cosmorun.exe futulab/futu_simple.c
```

**限制**:
- 只做TCP连接测试
- 不使用protobuf协议
- 适合验证网络连通性

### 方式3: 使用 GCC/Clang (nanopb版本)

```bash
cd /workspace/self-evolve-ai/cosmorun/futulab

gcc futu_main.c pb_common.c pb_encode.c pb_decode.c \
    Common.pb.c InitConnect.pb.c KeepAlive.pb.c \
    -I. -o futu_client

./futu_client
```

**注意**: futu_main.c 是早期示例，部分代码需要调整。推荐使用 `futu_cli.c`。

## 生成更多 Proto 文件

### 示例: 生成行情API

```bash
cd /workspace/self-evolve-ai/cosmorun/futulab

export PATH=/workspace/self-evolve-ai/third_party/protoc:$PATH

# 生成基础报价API
python3 /workspace/self-evolve-ai/third_party/nanopb/generator/nanopb_generator.py \
    /workspace/self-evolve-ai/third_party/futu-proto/Qot_Common.proto \
    /workspace/self-evolve-ai/third_party/futu-proto/Qot_GetBasicQot.proto \
    -I/workspace/self-evolve-ai/third_party/futu-proto

# 生成K线数据API
python3 /workspace/self-evolve-ai/third_party/nanopb/generator/nanopb_generator.py \
    /workspace/self-evolve-ai/third_party/futu-proto/Qot_GetKL.proto \
    -I/workspace/self-evolve-ai/third_party/futu-proto
```

### 示例: 生成交易API

```bash
# 生成下单API
python3 /workspace/self-evolve-ai/third_party/nanopb/generator/nanopb_generator.py \
    /workspace/self-evolve-ai/third_party/futu-proto/Trd_Common.proto \
    /workspace/self-evolve-ai/third_party/futu-proto/Trd_PlaceOrder.proto \
    -I/workspace/self-evolve-ai/third_party/futu-proto
```

## Futu OpenD 安装与配置

### 下载

**官方地址**: https://www.moomoo.com/download/OpenAPI

**支持平台**:
- Windows
- macOS
- Linux (仅部分版本)

### 配置

1. **启动 OpenD**
2. **启用API访问**: 设置 → API接口 → 开启
3. **配置端口**: 默认 11111 (可修改)
4. **配置IP白名单**: 添加 127.0.0.1 或允许所有

### 验证连接

```bash
# 方式1: 使用测试程序
./cosmorun.exe futulab/futu_simple.c

# 方式2: 使用telnet
telnet 127.0.0.1 11111

# 方式3: 使用nc
nc -zv 127.0.0.1 11111
```

## 协议文档

### Futu OpenAPI 官方文档

- **主页**: https://openapi.futunn.com/futu-api-doc/
- **协议介绍**: https://openapi.futunn.com/futu-api-doc/en/ftapi/protocol.html
- **API列表**: https://openapi.futunn.com/futu-api-doc/en/api-intro.html

### 协议格式

**Header (44 bytes)**:
```
+----------+-------------+------------------+
| 字段     | 大小(字节)   | 说明              |
+----------+-------------+------------------+
| 标识     | 2           | "FT"             |
| 协议ID   | 4           | ⚠️ 小端序 (实测)  |
| 格式类型 | 1           | 0=protobuf       |
| 版本     | 1           | 0 (当前版本)      |
| 序列号   | 4           | ⚠️ 小端序 (实测)  |
| Body长度 | 4           | ⚠️ 小端序 (实测)  |
| SHA1     | 20          | Body的SHA1       |
| 保留字段 | 8           | 全0               |
+----------+-------------+------------------+
```

**⚠️ 重要**: 官方文档标注为大端序，但实际测试证实协议使用**小端序 (little-endian)**！
这一点可从C++参考实现 https://github.com/towerd/C-For-FutuOpenD 中确认。

**常用协议ID**:
- 1001: InitConnect (初始化连接)
- 1002: GetGlobalState (获取全局状态)
- 1004: KeepAlive (心跳)
- 1005: GetUserInfo (获取用户信息)
- 3001-3xxx: 行情相关 (Qot_*)
- 2001-2xxx: 交易相关 (Trd_*)

## 完整重现步骤

从零开始重现整个环境：

```bash
#!/bin/bash
set -euo pipefail

# 1. 下载 protoc
cd /tmp
wget https://github.com/protocolbuffers/protobuf/releases/download/v28.3/protoc-28.3-linux-x86_64.zip
unzip -q protoc-28.3-linux-x86_64.zip
mkdir -p /workspace/self-evolve-ai/third_party/protoc
cp bin/protoc /workspace/self-evolve-ai/third_party/protoc/
chmod +x /workspace/self-evolve-ai/third_party/protoc/protoc

# 2. 克隆 nanopb
cd /workspace/self-evolve-ai/third_party
git clone --depth=1 https://github.com/nanopb/nanopb.git

# 3. 安装 Python 依赖
pip install --user protobuf grpcio-tools

# 4. 克隆 Futu API
cd /tmp
git clone --depth=1 https://github.com/futuopen/ftapi4go.git futu-api
mkdir -p /workspace/self-evolve-ai/third_party/futu-proto
cp -r futu-api/FTAPIProtoFiles_9.0.5008/* /workspace/self-evolve-ai/third_party/futu-proto/

# 5. 生成 C 代码
cd /workspace/self-evolve-ai/cosmorun/futulab
export PATH=/workspace/self-evolve-ai/third_party/protoc:$PATH

python3 /workspace/self-evolve-ai/third_party/nanopb/generator/nanopb_generator.py \
    /workspace/self-evolve-ai/third_party/futu-proto/Common.proto \
    /workspace/self-evolve-ai/third_party/futu-proto/KeepAlive.proto \
    /workspace/self-evolve-ai/third_party/futu-proto/InitConnect.proto \
    -I/workspace/self-evolve-ai/third_party/futu-proto

# 6. 复制 nanopb 运行时
cp /workspace/self-evolve-ai/third_party/nanopb/pb*.c .
cp /workspace/self-evolve-ai/third_party/nanopb/pb*.h .

# 7. 测试编译
cd /workspace/self-evolve-ai/cosmorun
./cosmorun.exe futulab/futu_simple.c

echo "✓ Setup completed!"
```

## 已知问题与解决方案

### ⚠️ 问题1: 字节序错误 (已解决)

**现象**:
```
Connection closed by server
或者
Response too large: 1107296256 bytes
```

**原因**: 协议实际使用**小端序**，但官方文档标注为大端序

**解决方案** ✅:
```c
/* 正确: 小端序 (little-endian) */
header->proto_id[0] = proto_id & 0xFF;
header->proto_id[1] = (proto_id >> 8) & 0xFF;
header->proto_id[2] = (proto_id >> 16) & 0xFF;
header->proto_id[3] = (proto_id >> 24) & 0xFF;

/* ❌ 错误: 大端序 (官方文档描述但不正确) */
header->proto_id[0] = (proto_id >> 24) & 0xFF;
header->proto_id[1] = (proto_id >> 16) & 0xFF;
header->proto_id[2] = (proto_id >> 8) & 0xFF;
header->proto_id[3] = proto_id & 0xFF;
```

**参考**: C++实现 https://github.com/towerd/C-For-FutuOpenD/blob/master/FutuOpenDClient/NetCenter.cpp

### 问题2: TCC 不支持 nanopb 标准头文件

**现象**:
```
TCC Error: In file included from futulab/pb.h:87:
/usr/lib/gcc/.../stdint-gcc.h:60: error: ';' expected
```

**原因**: TCC 对某些 GCC 特定的 stdint.h 实现支持不完整

**解决方案** ✅:
- 使用 `futu_cli.c` - 手动实现protobuf编码/解码，完全兼容TCC
- 或使用 GCC/Clang 编译 nanopb 版本

### 问题3: OpenD 不响应或连接超时

**可能原因**:
1. OpenD 未启动
2. 端口配置错误
3. 未启用API访问
4. SHA1哈希计算错误

**解决方案**:
1. 确认 OpenD 已启动并登录：
   ```
   API监听地址: 127.0.0.1:11111
   API启用RSA: 否
   登录成功
   ```
2. 使用 `futu_simple.c` 验证TCP连接
3. 确认SHA1哈希正确计算 (body数据的SHA1)
4. 确认使用小端序编码header

### 问题4: InitConnect 字段要求

**经验**:
- **最少字段**: clientVer, clientID, recvNotify (3个字段即可)
- **可选字段**: packetEncAlgo, pushProtoFmt, programmingLanguage
- 发送过多或格式错误的字段会导致OpenD关闭连接

**成功示例** (C++参考实现):
```c
c2s.clientVer = 100;
c2s.clientID = "demo";
c2s.recvNotify = true;
```

## 参考资源

- **Futu OpenAPI GitHub**: https://github.com/futuopen/ftapi4go
- **C++ 参考实现**: https://github.com/towerd/C-For-FutuOpenD
- **Protocol Buffers**: https://protobuf.dev/
- **nanopb**: https://jpa.kapsi.fi/nanopb/
- **Futu 官方文档**: https://openapi.futunn.com/futu-api-doc/
- **OpenD 下载**: https://www.moomoo.com/download/OpenAPI

## 版本信息与兼容性

### 已测试版本 (2025-10-17)

| 组件 | 版本 | 说明 |
|------|------|------|
| **OpenD** | 9.04.5408 | serverVer: 904 (通过InitConnect响应确认) |
| **Proto文件** | 9.0.5008 | 2025-03-06发布 |
| **protoc** | 28.3 | protobuf编译器 |
| **nanopb** | latest | 2025-10-17 commit |
| **futu-api (Python)** | 9.4.5408 | 用于对比测试 |

### 协议兼容性

**当前实现基于**:
- Proto格式: protobuf (format_type=0)
- 协议版本: 0 (proto_ver=0)
- 字节序: 小端序 (little-endian)
- InitConnect: clientVer=100, 3字段最小实现

**协议变更检测**:
1. OpenD版本号通过 `InitConnect.S2C.serverVer` 字段获取
2. Proto文件版本在目录名中标注 (如 `FTAPIProtoFiles_9.0.5008`)
3. 如遇协议不兼容，OpenD会返回 `retType=-1` 并在 `retMsg` 中说明

**升级建议**:
1. **OpenD更新**时，先用 `futu_cli.c init` 测试兼容性
2. **Proto文件更新**时，从 https://github.com/futuopen/ftapi4go 下载最新版本
3. **重新生成**受影响的 `.pb.c/.pb.h` 文件
4. 如协议格式变更（小概率），参考官方Python/C++实现调整

### 版本对应关系

OpenD、Proto文件、官方SDK通常保持同步更新：
```
OpenD 9.04.5408 ← → Proto 9.0.5008 ← → futu-api 9.4.5408
        ↓                  ↓                     ↓
   (服务端)           (协议定义)            (客户端SDK)
```

更新时优先保持三者版本一致性。

## 维护日志

### 2025-10-17 (下午): 🚀 行情和交易接口全面封装完成

**新增功能**:
- ✅ 行情接口封装（3个）：K线数据、盘口数据、实时行情
- ✅ 交易接口封装（5个）：账户列表、解锁交易、资金、持仓、下单
- ✅ 所有行情接口测试通过
- ✅ 交易接口（只读）测试通过

**测试结果**:
```bash
# 行情接口 - 全部通过
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- quote 1 00700      ✅
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- kline 1 00700 2 1 10  ✅
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- orderbook 1 00700 10  ✅

# 交易接口（只读）- 测试通过
./cosmorun.exe futulab/futu_cli.c futulab/futu_utils.c -- acclist  ✅

# 交易接口（写操作）- 已封装但未测试（真实账户）
# - unlock, funds, position, order
```

**关键修复**:
1. K线接口字段顺序：rehabType → klType → security（之前顺序错误）
2. 时间格式：必须提供 "yyyy-MM-dd" 格式的起止时间
3. 盘口订阅：需订阅 SubType_OrderBook (2)，而非 SubType_Basic (1)
4. 函数前向声明：解决 TCC 编译器的函数引用问题

**文件更新**:
- `futu_utils.h`: 新增协议常量和8个API构建函数
- `futu_utils.c`: 实现所有行情和交易接口的请求构建
- `futu_cli.c`: 新增8个命令（kline, orderbook, acclist, unlock, funds, position, order）
- `readme.md`: 更新使用说明和参数文档

### 2025-10-17 (上午): 🎉 完整CLI工具实现成功

**重大突破**:
- ✅ 发现协议实际使用**小端序**（与官方文档不符）
- ✅ 实现 `futu_cli.c` - 完整的TCC兼容CLI工具
- ✅ 手动实现protobuf编码/解码（无需nanopb）
- ✅ 内置SHA1哈希计算
- ✅ 测试通过4个核心API：InitConnect, GetUserInfo, GetGlobalState, KeepAlive

**实现细节**:
- 单文件实现（~660行C代码）
- 只依赖 `cosmo_libc.h`
- 支持通过 `cosmorun.exe` 直接运行
- 参考C++实现确认协议细节

**调试过程**:
1. 初始使用大端序失败（OpenD关闭连接）
2. 对比Python SDK和C++实现
3. 发现字节序差异并修正
4. 简化InitConnect字段到最小集合
5. 所有命令测试通过

**环境搭建**:
- 下载并配置 protoc v28.3
- 克隆 nanopb 工具链
- 下载 Futu API proto 文件 v9.0.5008
- 生成基础 proto 的 C 代码（用于参考）
- 创建测试程序 (futu_simple.c, futu_cli.c)
- 编写详细使用文档
