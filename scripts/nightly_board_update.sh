#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="/workspace/.github"
CODEX_BIN=$(command -v codex || true)
LOG_FILE=${COINTECH_LOG:-/workspace/.github/log/cointech_board.log}
mkdir -p "$(dirname "$LOG_FILE")"

if [[ -z "$CODEX_BIN" ]]; then
  echo "codex CLI 未安装或不在 PATH" >&2
  exit 1
fi

PROMPT=$(cat <<'PROMPT'
你是一名自动化助手,在 /workspace/.github 仓库中运行。请严格按顺序完成:
1. 确认 cointech_board.md 顶部包含形如 `[最新更新时间] 2025-02-15 02:00 UTC` 的行; 如无则以当前 UTC 时间补上并保存。
2. 使用 `./github_wiki.sh wanjochan/.github upsert-wiki cointech/board cointech_board.md "Nightly cointech board sync"` 把更新后的 Markdown 推送到 wiki 子目录。
3. 使用 `./github_wiki.sh wanjochan/.github upsert-wiki board cointech_board.md "Nightly board sync"` 把同一份内容同步到根路径 /board。
任何一步失败都要立即输出错误并停止,不做其它改动。
PROMPT
)

MODEL=${CODEX_MODEL:-}
SANDBOX=${CODEX_SANDBOX:-workspace-write}
MODEL_ARG=()
SANDBOX_ARG=( --sandbox "$SANDBOX" )
if [[ -n "$MODEL" ]]; then
  MODEL_ARG=( -m "$MODEL" )
fi

{
  "$CODEX_BIN" exec -C "$REPO_DIR" "${MODEL_ARG[@]}" "${SANDBOX_ARG[@]}" - <<PROMPT
$PROMPT
PROMPT
} >> "$LOG_FILE" 2>&1
