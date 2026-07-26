#!/usr/bin/env bash

set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
practice_dir="$repo_dir/tools/practice_tool"
ready_file="$practice_dir/.local/environment-ready-v1"
practice_url="http://127.0.0.1:5173/"

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
    printf '%s\n' '[practice] 首次运行：正在准备 Node.js 环境……'
    bash "$practice_dir/scripts/install_environment.sh"
    hash -r
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
        printf '%s\n' '[practice] Node.js 环境准备后仍未找到 npm，请关闭终端并重新打开 UCRT64。' >&2
        exit 1
    fi
fi

cd "$practice_dir"

if [[ ! -f "$ready_file" ]]; then
    printf '%s\n' '[practice] 首次运行：正在检查并安装项目依赖……'
    "${npm_command[@]}" install
    mkdir -p .local
    printf '%s\n' 'environment-ready-v1' > "$ready_file"
    printf '%s\n' '[practice] 环境准备完成，以后启动将跳过此步骤。'
fi

if [[ ! -d node_modules ]]; then
    rm -f "$ready_file"
    printf '%s\n' '[practice] 依赖目录已被清理，正在重新准备……'
    "${npm_command[@]}" install
    mkdir -p .local
    printf '%s\n' 'environment-ready-v1' > "$ready_file"
fi

printf '[practice] 正在启动：%s\n' "$practice_url"
if [[ "${PRACTICE_NO_OPEN:-0}" != "1" ]]; then
    if command -v cmd.exe >/dev/null 2>&1; then
        (sleep 2; cmd.exe //d //c start "" "$practice_url" >/dev/null 2>&1) &
    elif command -v xdg-open >/dev/null 2>&1; then
        (sleep 2; xdg-open "$practice_url" >/dev/null 2>&1) &
    fi
fi
"${npm_command[@]}" run dev -- --host 127.0.0.1
