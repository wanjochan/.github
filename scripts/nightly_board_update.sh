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

# 读取 prompt 文件
PROMPT_FILE="$REPO_DIR/scripts/cointech_board_update.prompt"
if [[ ! -f "$PROMPT_FILE" ]]; then
  echo "错误: prompt 文件不存在: $PROMPT_FILE" >&2
  exit 1
fi

(
  cd "$REPO_DIR"
  "$CLAUDE_BIN" --print --model "$MODEL" --system-prompt "$SYSTEM_PROMPT" --add-dir "$REPO_DIR" "${EXTRA_ARGS[@]}" < "$PROMPT_FILE"
) >> "$LOG_FILE" 2>&1
