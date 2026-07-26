#!/usr/bin/env bash

set -euo pipefail

practice_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
required_node_major=18
local_node_bin="$practice_dir/.local/runtime/node/bin"
ready_file="$practice_dir/.local/environment-ready-v3-node-compatible"
practice_url="${PRACTICE_URL:-http://127.0.0.1:5173/}"
upgrade_requested=0
forwarded_args=()

show_help() {
    cat <<'EOF'
practice - 本地知识训练工具

用途：
  按模块、训练单元和阶段进行提示提问、脱稿输出与专业案例训练。
  训练工具独立保存题库和作答状态，通过显式知识源配置读取外部知识库。

用法：
  ./start.sh [选项]

选项：
  -h, --help                 显示本帮助，不安装环境或启动服务
  --upgrade                  更新兼容的 Node.js 运行时并刷新项目依赖
  --host <地址>              将参数继续传给 Vite
  --port <端口>              指定本地服务端口
  --completion bash          输出 Bash Tab 补全脚本
  --install-completion       安装当前用户的 Bash Tab 补全

环境变量：
  PRACTICE_SOURCE_CONFIG     指定知识源配置文件
  PRACTICE_NODE_DIST_SOURCES 按顺序指定 Node.js 下载源，使用空格分隔
  PRACTICE_NPM_REGISTRIES    按顺序指定 npm 下载源，使用空格分隔
  PRACTICE_NO_OPEN=1         启动后不自动打开浏览器
  PRACTICE_AUTO_COMPLETION=0 禁止首次正常启动自动安装 Bash 补全

示例：
  ./start.sh
  ./start.sh --upgrade
  ./start.sh --install-completion
  PRACTICE_NO_OPEN=1 ./start.sh --port 5174

更多说明：
  README.md
EOF
}

install_bash_completion() {
    local overwrite="${1:-0}"
    local user_data_home completion_dir completion_source completion_target

    if [[ -z "${HOME:-}" ]]; then
        printf '%s\n' '[practice] 未检测到 HOME，跳过 Bash 补全安装。' >&2
        return 1
    fi

    user_data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
    completion_dir="$user_data_home/bash-completion/completions"
    completion_source="$practice_dir/scripts/completions/start.bash"
    completion_target="$completion_dir/start.sh"
    mkdir -p "$completion_dir"

    if [[ -e "$completion_target" || -L "$completion_target" ]]; then
        if [[ "$overwrite" != "1" ]]; then
            return 0
        fi
        rm -f "$completion_target"
    fi

    if ln -s "$completion_source" "$completion_target" 2>/dev/null; then
        printf '[practice] Bash 补全已链接到当前工具目录：%s\n' "$completion_target"
    else
        printf 'source %q\n' "$completion_source" > "$completion_target"
        printf '[practice] 当前系统无法创建符号链接，已安装动态加载器：%s\n' "$completion_target"
    fi
    printf '[practice] 补全源：%s\n' "$completion_source"
    printf '%s\n' '[practice] 后续 Git 更新该源文件时，补全规则会同步更新。'
    printf '%s\n' '[practice] 当前 Bash 首次安装后请执行一次：'
    printf '  source <(%q --completion bash)\n' "$practice_dir/start.sh"
    if ! type _completion_loader >/dev/null 2>&1 &&
        [[ ! -r /usr/share/bash-completion/bash_completion ]]; then
        printf '%s\n' '[practice] 未检测到 bash-completion；请先通过系统包管理器安装，后续新终端才能自动加载补全。'
    else
        printf '%s\n' '[practice] 后续新开的 Bash 会自动发现补全，无需再次执行 source。'
    fi
}

if [[ "${1:-}" = "--completion" ]]; then
    if [[ "${2:-bash}" != "bash" ]]; then
        printf '[practice] 暂不支持该补全类型：%s\n' "${2:-}" >&2
        exit 2
    fi
    cat "$practice_dir/scripts/completions/start.bash"
    exit 0
fi

if [[ "${1:-}" = "--install-completion" ]]; then
    install_bash_completion 1
    exit 0
fi

for argument in "$@"; do
    case "$argument" in
        -h|--help)
            show_help
            exit 0
            ;;
        --upgrade)
            upgrade_requested=1
            ;;
        *)
            forwarded_args+=("$argument")
            ;;
    esac
done

if [[ "${PRACTICE_AUTO_COMPLETION:-1}" = "1" && -t 0 && -t 1 ]]; then
    install_bash_completion 0 || true
fi

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
    printf '%s\n' '[practice] 正在按就近镜像、官方源顺序准备最新兼容 Node.js；失败时会逐级回退……'
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

install_dependencies() {
    local registry
    local registries="${PRACTICE_NPM_REGISTRIES:-https://registry.npmmirror.com https://registry.npmjs.org}"
    for registry in $registries; do
        printf '[practice] 尝试 npm 下载源：%s\n' "$registry"
        if "${npm_command[@]}" install --registry="$registry" --no-audit --no-fund --fetch-retries=2 --fetch-timeout=120000; then
            return 0
        fi
        printf '[practice] 当前 npm 下载源不可用，切换下一源：%s\n' "$registry" >&2
    done
    return 1
}

if [[ ! -f "$ready_file" ]]; then
    printf '%s\n' '[practice] 首次运行：正在检查并安装项目依赖……'
    printf '%s\n' '[practice] 首次下载可能受网络速度影响；此阶段完成前不会打开浏览器。'
    install_dependencies
    mkdir -p .local
    printf '%s\n' 'environment-ready-v3-node-compatible' > "$ready_file"
    printf '%s\n' '[practice] 环境准备完成，以后启动将跳过此步骤。'
fi

if [[ ! -d node_modules ]]; then
    rm -f "$ready_file"
    printf '%s\n' '[practice] 依赖目录已被清理，正在重新准备……'
    install_dependencies
    mkdir -p .local
    printf '%s\n' 'environment-ready-v3-node-compatible' > "$ready_file"
fi

printf '[practice] 正在启动：%s\n' "$practice_url"
vite_args=(--host 127.0.0.1)
if [[ "${PRACTICE_NO_OPEN:-0}" != "1" ]]; then
    vite_args+=(--open)
fi

"${npm_command[@]}" run dev -- "${vite_args[@]}" "${forwarded_args[@]}"
