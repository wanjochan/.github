#!/usr/bin/env bash
set -euo pipefail

# 确保 PATH 包含常用路径（cron 环境可能缺失）
export PATH="/usr/local/bin:/usr/bin:/bin:$PATH"

# 网络检查 - 尝试多个 DNS
if ! ping -c 1 -W 3 github.com >/dev/null 2>&1; then
  # 尝试使用公共 DNS
  if ! ping -c 1 -W 3 8.8.8.8 >/dev/null 2>&1; then
    echo "[$(date -u +"%Y-%m-%d %H:%M UTC")] 网络完全不通，跳过 wiki 同步" >&2
    exit 0
  fi
  echo "[$(date -u +"%Y-%m-%d %H:%M UTC")] DNS 解析问题，但网络可达，继续尝试" >&2
fi

REPO_DIR="/workspace/.github"
CLAUDE_BIN="${CLAUDE_BIN:-$(command -v claude || true)}"
LOG_FILE="${COINTECH_LOG:-$REPO_DIR/log/cointech_board.log}"
MODEL="${CLAUDE_MODEL:-sonnet}"
SYSTEM_PROMPT="${CLAUDE_SYSTEM_PROMPT:-You are an automation agent running scheduled wiki sync tasks.}"
EXTRA_ARGS=()
if [[ -n "${CLAUDE_PERMISSION_MODE:-}" ]]; then
  EXTRA_ARGS+=(--permission-mode "${CLAUDE_PERMISSION_MODE}")
fi
if [[ -n "${CLAUDE_EXTRA_ARGS:-}" ]]; then
  # shellcheck disable=SC2206
  EXTRA_ARGS+=(${CLAUDE_EXTRA_ARGS})
fi

mkdir -p "$(dirname "$LOG_FILE")"

if [[ -z "$CLAUDE_BIN" ]]; then
  echo "claude CLI 未安装或不在 PATH" >&2
  exit 1
fi

read -r -d '' PROMPT <<'PROMPT'
你是 CoinTech 技术指数榜单的自动化更新助手，在 /workspace/.github 仓库中运行。

## 核心任务流程

### 步骤 0: 数据收集（必须执行，禁止臆造）
使用 web_search 工具搜索以下**真实数据源**，获取最新链上数据：
- DefiLlama API: TVL、费用、活跃地址数据
- Token Terminal: 协议收入、开发者活跃度
- L2Beat: Layer2 TVL、Sequencer 状态
- CoinGecko/CoinMarketCap: 市值、交易量
- GitHub API: 核心仓库提交频次、贡献者数量
- 各公链浏览器: 日活地址、合约调用量（Etherscan、Solscan、BscScan 等）

**搜索示例查询**：
- "Ethereum daily fees DefiLlama 2025"
- "Solana active addresses Solscan latest"
- "Arbitrum TVL L2Beat current"
- "Base transaction count growth 2025"

**数据验证要求**：
- 每个项目至少从 2 个独立数据源交叉验证
- 标注数据来源和时间戳
- 发现数据缺失时标记为 "数据待更新"，不要编造

### 步骤 1: 更新榜单内容
基于步骤 0 收集的真实数据：
1. 读取 cointech_board.md
2. 更新排名前 20 项目的关键指标（TVL、日活、费用等）
3. 调整技术指数评分（基于 12 维度权重表）
4. **获取当前真实 UTC 时间**（使用 shell 命令 `date -u +"%Y-%m-%d %H:%M UTC"`），更新顶部时间戳为：`[最新更新时间] YYYY-MM-DD HH:MM UTC`
5. 保存文件

**禁止行为**：
- ❌ 不搜索直接编造数据
- ❌ 使用过时缓存数据（超过 24 小时）
- ❌ 臆想项目排名变化

### 步骤 2: 推送到 Wiki 子目录
```bash
./github_wiki.sh wanjochan/.github upsert-wiki cointech/board cointech_board.md "Nightly cointech board sync"
```

### 步骤 3: 同步到 Wiki 根路径
```bash
./github_wiki.sh wanjochan/.github upsert-wiki board cointech_board.md "Nightly board sync"
```

## 错误处理
- 任何步骤失败立即输出错误并停止
- 网络搜索失败时，仅更新时间戳，不修改数据内容
- 记录所有数据源访问情况到日志

## 输出格式要求
在日志中记录：
1. 搜索的数据源列表及响应状态
2. 更新的项目数量和主要变化
3. Wiki 推送结果
PROMPT

(
  cd "$REPO_DIR"
  "$CLAUDE_BIN" --print --model "$MODEL" --system-prompt "$SYSTEM_PROMPT" --add-dir "$REPO_DIR" "${EXTRA_ARGS[@]}" <<PROMPT
$PROMPT
PROMPT
) >> "$LOG_FILE" 2>&1
