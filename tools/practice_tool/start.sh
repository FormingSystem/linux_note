#!/usr/bin/env bash

set -euo pipefail

practice_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
required_node_major=18
local_node_bin="$practice_dir/.local/runtime/node/bin"
ready_file="$practice_dir/.local/environment-ready-v3-node-compatible"
practice_url="${PRACTICE_URL:-http://127.0.0.1:5173/}"
upgrade_requested=0
forwarded_args=()

for argument in "$@"; do
    case "$argument" in
        --upgrade)
            upgrade_requested=1
            ;;
        *)
            forwarded_args+=("$argument")
            ;;
    esac
done

node_is_supported() {
    command -v node >/dev/null 2>&1 &&
        [[ "$(node -p 'Number(process.versions.node.split(".")[0])' 2>/dev/null || printf '0')" -ge "$required_node_major" ]]
}

if [[ -x "$local_node_bin/node" && -x "$local_node_bin/npm" ]]; then
    export PATH="$local_node_bin:$PATH"
    hash -r
fi

if [[ "$upgrade_requested" -eq 1 ]]; then
    printf '%s\n' '[practice] 升级模式：重新选择官方最高可用兼容 Node.js，并刷新项目依赖……'
    rm -f "$ready_file"
    PRACTICE_FORCE_NODE_UPGRADE=1 bash "$practice_dir/scripts/install_environment.sh"
    if [[ -x "$local_node_bin/node" && -x "$local_node_bin/npm" ]]; then
        export PATH="$local_node_bin:$PATH"
        hash -r
    fi
fi

if ! node_is_supported || ! command -v npm >/dev/null 2>&1; then
    if [[ -f /e/node_js/node.exe && -f /e/node_js/npm ]]; then
        export PATH="/e/node_js:$PATH"
        hash -r
    elif [[ -f "/c/Program Files/nodejs/node.exe" && -f "/c/Program Files/nodejs/npm" ]]; then
        export PATH="/c/Program Files/nodejs:$PATH"
        hash -r
    fi
fi

if ! node_is_supported || ! command -v npm >/dev/null 2>&1; then
    current_version="$(node --version 2>/dev/null || printf '未安装')"
    printf '[practice] 当前 Node.js 为 %s，工具最低要求为 v%d。\n' "$current_version" "$required_node_major"
    printf '%s\n' '[practice] 正在从 Node.js 官方发行地址准备最新的兼容 LTS；失败时会逐级回退……'
    bash "$practice_dir/scripts/install_environment.sh"
    if [[ -x "$local_node_bin/node" && -x "$local_node_bin/npm" ]]; then
        export PATH="$local_node_bin:$PATH"
    elif [[ -f /e/node_js/node.exe && -f /e/node_js/npm ]]; then
        export PATH="/e/node_js:$PATH"
    elif [[ -f "/c/Program Files/nodejs/node.exe" && -f "/c/Program Files/nodejs/npm" ]]; then
        export PATH="/c/Program Files/nodejs:$PATH"
    fi
    hash -r
fi

if ! node_is_supported || ! command -v npm >/dev/null 2>&1; then
    printf '[practice] Node.js 环境仍不兼容，当前版本：%s。\n' "$(node --version 2>/dev/null || printf '未安装')" >&2
    exit 1
fi

if command -v npm >/dev/null 2>&1; then
    npm_command=(npm)
elif [[ -f /e/node_js/node.exe && -f /e/node_js/npm ]]; then
    export PATH="/e/node_js:$PATH"
    hash -r
    npm_command=(npm)
elif [[ -f "/c/Program Files/nodejs/node.exe" && -f "/c/Program Files/nodejs/npm" ]]; then
    export PATH="/c/Program Files/nodejs:$PATH"
    hash -r
    npm_command=(npm)
else
    printf '%s\n' '[practice] Node.js 环境准备后仍未找到 npm。' >&2
    exit 1
fi

cd "$practice_dir"

if [[ ! -f "$ready_file" ]]; then
    printf '%s\n' '[practice] 首次运行：正在检查并安装项目依赖……'
    printf '%s\n' '[practice] 首次下载可能受网络速度影响；此阶段完成前不会打开浏览器。'
    "${npm_command[@]}" install --no-audit --no-fund --fetch-retries=2 --fetch-timeout=120000
    mkdir -p .local
    printf '%s\n' 'environment-ready-v3-node-compatible' > "$ready_file"
    printf '%s\n' '[practice] 环境准备完成，以后启动将跳过此步骤。'
fi

if [[ ! -d node_modules ]]; then
    rm -f "$ready_file"
    printf '%s\n' '[practice] 依赖目录已被清理，正在重新准备……'
    "${npm_command[@]}" install --no-audit --no-fund --fetch-retries=2 --fetch-timeout=120000
    mkdir -p .local
    printf '%s\n' 'environment-ready-v3-node-compatible' > "$ready_file"
fi

printf '[practice] 正在启动：%s\n' "$practice_url"
vite_args=(--host 127.0.0.1)
if [[ "${PRACTICE_NO_OPEN:-0}" != "1" ]]; then
    vite_args+=(--open)
fi

"${npm_command[@]}" run dev -- "${vite_args[@]}" "${forwarded_args[@]}"
