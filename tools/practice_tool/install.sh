#!/usr/bin/env bash

set -euo pipefail

practice_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$practice_dir/scripts/lib/platform_environment.sh"
practice_environment_init "$practice_dir"

upgrade_requested=0
if_needed=0
runtime_only=0

show_help() {
    cat <<'EOF'
回路（Loop）安装模块

用法：
  ./install.sh [选项]

选项：
  --if-needed         仅在环境或项目依赖未就绪时安装
  --upgrade           重新选择兼容 Node.js 并刷新项目依赖
  --runtime-only      只准备 Node.js，不安装项目依赖
  -h, --help          显示本帮助

说明：
  本脚本只负责运行环境和项目依赖，不启动浏览器服务。
  start.sh 首次运行时会自动调用本模块；也可以独立执行。
EOF
}

for argument in "$@"; do
    case "$argument" in
        --if-needed) if_needed=1 ;;
        --upgrade) upgrade_requested=1 ;;
        --runtime-only) runtime_only=1 ;;
        -h|--help) show_help; exit 0 ;;
        *) printf '[practice] 未知安装参数：%s\n' "$argument" >&2; exit 2 ;;
    esac
done

practice_activate_local_node
if [[ "$if_needed" -eq 1 && "$upgrade_requested" -eq 0 ]] &&
    practice_node_is_supported && practice_dependencies_are_ready; then
    exit 0
fi

if [[ "$upgrade_requested" -eq 1 ]]; then
    printf '%s\n' '[practice] 安装模块进入升级模式：重新选择兼容 Node.js 并刷新项目依赖……'
    rm -f "$PRACTICE_READY_FILE"
    PRACTICE_FORCE_NODE_UPGRADE=1 bash "$practice_dir/scripts/install_environment.sh"
    practice_activate_local_node
elif ! practice_node_is_supported; then
    current_version="$(node --version 2>/dev/null || printf '未安装')"
    printf '[practice] 当前 Node.js 为 %s，工具最低要求为 v%d。\n' \
        "$current_version" "$PRACTICE_REQUIRED_NODE_MAJOR"
    printf '%s\n' '[practice] 正在调用运行时安装器，并按就近镜像、官方源顺序逐级回退……'
    bash "$practice_dir/scripts/install_environment.sh"
    practice_activate_local_node
fi

if ! practice_node_is_supported; then
    printf '[practice] Node.js 环境仍不兼容，当前版本：%s。\n' \
        "$(node --version 2>/dev/null || printf '未安装')" >&2
    exit 1
fi

if [[ "$runtime_only" -eq 1 ]]; then
    printf '[practice] Node.js 运行时已就绪：%s。\n' "$(node --version)"
    exit 0
fi

if ! practice_command_exists npm; then
    printf '%s\n' '[practice] Node.js 环境准备后仍未找到 npm。' >&2
    exit 1
fi

install_dependencies() {
    local registry
    for registry in $PRACTICE_NPM_REGISTRIES; do
        printf '[practice] 尝试 npm 下载源：%s\n' "$registry"
        if npm install --registry="$registry" --no-audit --no-fund \
            --fetch-retries=2 --fetch-timeout=120000; then
            return 0
        fi
        printf '[practice] 当前 npm 下载源不可用，切换下一源：%s\n' "$registry" >&2
    done
    return 1
}

cd "$practice_dir"
if [[ "$upgrade_requested" -eq 1 ]] || ! practice_dependencies_are_ready; then
    printf '%s\n' '[practice] 正在由安装模块检查并安装项目依赖……'
    printf '%s\n' '[practice] 首次下载可能受网络速度影响；安装模块不会打开浏览器。'
    install_dependencies
    mkdir -p .local
    printf '%s\n' 'environment-ready-v3-node-compatible' > "$PRACTICE_READY_FILE"
    practice_package_lock_digest > "$PRACTICE_DEPENDENCY_STAMP"
fi

printf '%s\n' '[practice] 安装完成；可以执行 ./run.sh 或 ./start.sh。'
