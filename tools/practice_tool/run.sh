#!/usr/bin/env bash

set -euo pipefail

practice_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$practice_dir/scripts/lib/platform_environment.sh"
practice_environment_init "$practice_dir"
practice_activate_local_node

show_help() {
    cat <<'EOF'
回路（Loop）运行模块

用法：
  ./run.sh [Vite 选项]

常用选项：
  --host <地址>    指定监听地址；安全默认值为 127.0.0.1
  --port <端口>    指定本地服务端口
  -h, --help       显示本帮助

说明：
  本脚本只运行已经安装好的工具，不安装或更新任何软件。
  环境未就绪时请先执行 ./install.sh。
EOF
}

for argument in "$@"; do
    case "$argument" in
        -h|--help) show_help; exit 0 ;;
    esac
done

if ! practice_node_is_supported || ! practice_command_exists npm; then
    printf '%s\n' '[practice] 运行环境未就绪；run.sh 不会自动安装。请先执行：' >&2
    printf '%s\n' '  ./install.sh' >&2
    exit 1
fi
if ! practice_dependencies_are_ready; then
    printf '%s\n' '[practice] 项目依赖未就绪；run.sh 不会自动安装。请先执行：' >&2
    printf '%s\n' '  ./install.sh' >&2
    exit 1
fi

cd "$practice_dir"
practice_url="${PRACTICE_URL:-http://127.0.0.1:5173/}"
printf '[practice] 正在启动：%s\n' "$practice_url"
vite_args=(--host 127.0.0.1)
if [[ "${PRACTICE_NO_OPEN:-0}" != "1" ]]; then
    vite_args+=(--open)
fi

exec npm run dev -- "${vite_args[@]}" "$@"
