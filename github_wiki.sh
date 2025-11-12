#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOU'
Usage: github_wiki.sh <repo> <command> [args]

Repo formats:
  • owner/repo (e.g., wanjochan/.github)
  • owner.repo (dot form)
  • path/to/local/repo (origin remote must be GitHub)

Commands:
  list-wiki [pattern]
      List wiki files (optionally filtered by pattern).
  read-wiki <page>
      Print a wiki page to stdout.
  upsert-wiki <page> <file> [message]
      Create/replace a wiki page from local file.
  delete-wiki <page> [message]
      Delete a page.

Authentication for write commands:
  • Export GITHUB_TOKEN / GH_TOKEN, or
  • Configure `git credential` / `~/.git-credentials`.
EOU
}

fail() { printf 'Error: %s\n' "$1" >&2; exit 1; }
require_cmd() { command -v "$1" >/dev/null 2>&1 || fail "Missing command: $1"; }

fetch_token() {
  if [[ -n "${GITHUB_TOKEN:-}" ]]; then printf '%s\n' "$GITHUB_TOKEN"; return; fi
  if [[ -n "${GH_TOKEN:-}" ]]; then printf '%s\n' "$GH_TOKEN"; return; fi
  local cred
  cred=$(printf "protocol=https\nhost=github.com\n\n" | git credential fill 2>/dev/null || true)
  local password
  password=$(printf '%s\n' "$cred" | awk -F= '/^password=/{print $2; exit}')
  if [[ -n "$password" ]]; then printf '%s\n' "$password"; return; fi
  local user
  user=$(printf '%s\n' "$cred" | awk -F= '/^username=/{print $2; exit}')
  if [[ -n "$user" ]]; then printf '%s\n' "$user"; return; fi
  local cred_file=$HOME/.git-credentials
  if [[ -f "$cred_file" ]]; then
    local line
    line=$(grep -m1 'github.com' "$cred_file" || true)
    if [[ -n "$line" ]]; then
      line=${line#*//}
      line=${line%%@*}
      printf '%s\n' "${line#*:}"
      return
    fi
  fi
  printf ''
}

slug_from_repo() {
  local arg=$1
  if [[ -d "$arg/.git" ]]; then
    local remote
    remote=$(git -C "$arg" remote get-url origin 2>/dev/null || true)
    [[ -n "$remote" ]] || fail "repo $arg 没有 origin remote"
    remote=${remote%.git}
    [[ $remote =~ github\.com[:/]+([^/]+/[^/]+)$ ]] || fail "origin $remote 不是 GitHub"
    printf '%s\n' "${BASH_REMATCH[1]}"
  elif [[ $arg == */* ]]; then
    printf '%s\n' "${arg%.git}"
  elif [[ $arg == *.* ]]; then
    printf '%s/%s\n' "${arg%%.*}" "${arg#*.}"
  else
    fail "无法识别 repo: $arg"
  fi
}

page_to_file() {
  local name=$1
  if [[ $name == *.md ]]; then
    printf '%s\n' "$name"
  else
    printf '%s.md\n' "${name// /-}"
  fi
}

clone_wiki() {
  local slug=$1 dest=$2 mode=$3 token=${4:-}
  local url="https://github.com/${slug}.wiki.git"
  if ! git clone --quiet "$url" "$dest" 2>/dev/null; then
    if [[ $mode == read ]]; then
      fail "无法 clone wiki: $slug"
    fi
    mkdir -p "$dest"
    git -C "$dest" init --quiet
  fi
  if [[ $mode == write ]]; then
    [[ -n "$token" ]] || fail "需要 GitHub token 才能写入"
    local auth="https://x-access-token:${token}@github.com/${slug}.wiki.git"
    if git -C "$dest" remote get-url origin >/dev/null 2>&1; then
      git -C "$dest" remote set-url origin "$auth"
    else
      git -C "$dest" remote add origin "$auth"
    fi
    git -C "$dest" fetch origin --quiet || true
  fi
}

list_wiki() {
  local slug=$1 filter=${2:-}
  local tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' RETURN
  clone_wiki "$slug" "$tmp/wiki" read
  if git -C "$tmp/wiki" rev-parse HEAD >/dev/null 2>&1; then
    local entries
    entries=$(git -C "$tmp/wiki" ls-tree --name-only -r HEAD)
    [[ -n "$entries" ]] || { echo "Wiki has no pages."; return; }
    if [[ -n "$filter" ]]; then
      printf '%s\n' "$entries" | grep -i -- "$filter" || true
    else
      printf '%s\n' "$entries"
    fi
  else
    echo "Wiki has no commits yet."
  fi
}

read_wiki() {
  [[ $# -ge 2 ]] || fail "read-wiki 需要 <page>"
  local slug=$1 page=$2
  local tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' RETURN
  clone_wiki "$slug" "$tmp/wiki" read
  git -C "$tmp/wiki" rev-parse HEAD >/dev/null 2>&1 || fail "Wiki has no commits yet."
  local file=$(page_to_file "$page")
  [[ -f "$tmp/wiki/$file" ]] || fail "Page $file 不存在"
  cat "$tmp/wiki/$file"
}

upsert_wiki() {
  [[ $# -ge 3 ]] || fail "upsert-wiki 需要 <page> <file>"
  local slug=$1 page=$2 src=$3 msg=${4:-}
  [[ -f "$src" ]] || fail "source $src 不存在"
  local token=$(fetch_token)
  local tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' RETURN
  clone_wiki "$slug" "$tmp/wiki" write "$token"
  local file=$(page_to_file "$page")
  mkdir -p "$(dirname "$tmp/wiki/$file")"
  cp "$src" "$tmp/wiki/$file"
  git -C "$tmp/wiki" add "$file"
  [[ -n "$msg" ]] || msg="Update wiki page: $page"
  git -C "$tmp/wiki" config user.name "${GIT_AUTHOR_NAME:-github_wiki.sh}"
  git -C "$tmp/wiki" config user.email "${GIT_AUTHOR_EMAIL:-github_wiki@example.com}"
  if git -C "$tmp/wiki" diff --cached --quiet; then
    echo "No changes detected for $file; skipping push."
    return
  fi
  git -C "$tmp/wiki" commit --quiet -m "$msg"
  git -C "$tmp/wiki" push origin HEAD:master --quiet
  printf 'Pushed %s to %s wiki.\n' "$file" "$slug"
}

delete_wiki() {
  [[ $# -ge 2 ]] || fail "delete-wiki 需要 <page>"
  local slug=$1 page=$2 msg=${3:-}
  local token=$(fetch_token)
  local tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' RETURN
  clone_wiki "$slug" "$tmp/wiki" write "$token"
  local file=$(page_to_file "$page")
  [[ -f "$tmp/wiki/$file" ]] || fail "Page $file 不存在"
  git -C "$tmp/wiki" rm -f "$file" >/dev/null
  [[ -n "$msg" ]] || msg="Delete wiki page: $page"
  git -C "$tmp/wiki" config user.name "${GIT_AUTHOR_NAME:-github_wiki.sh}"
  git -C "$tmp/wiki" config user.email "${GIT_AUTHOR_EMAIL:-github_wiki@example.com}"
  git -C "$tmp/wiki" commit --quiet -m "$msg"
  git -C "$tmp/wiki" push origin HEAD:master --quiet
  printf 'Deleted %s from %s wiki.\n' "$file" "$slug"
}

main() {
  require_cmd git
  require_cmd mktemp
  if [[ $# -lt 2 ]]; then usage; exit 1; fi
  local repo=$1; shift
  local cmd=$1; shift
  local slug=$(slug_from_repo "$repo"); slug=${slug%/}
  case "$cmd" in
    list-wiki) list_wiki "$slug" "${1:-}" ;;
    read-wiki) read_wiki "$slug" "$1" ;;
    upsert-wiki) upsert_wiki "$slug" "$1" "$2" "${3:-}" ;;
    delete-wiki) delete_wiki "$slug" "$1" "${2:-}" ;;
    -h|--help|help) usage ;;
    *) fail "未知命令: $cmd" ;;
  esac
}

main "$@"
