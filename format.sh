#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

usage() {
    cat <<'EOF'
用法：
  ./format.sh [all] [--apply] [--summary] [路径...]    文件名 + Markdown 标题
  ./format.sh names [--apply] [路径...]                 仅格式化文件和目录名
  ./format.sh md [--apply] [--summary] [路径...]        仅格式化 Markdown 标题

不给路径时处理全仓；指定文件或目录时只处理对应范围。默认仅预览。
EOF
}

command_name=${1:-all}
case "$command_name" in
    all|names|md) shift || true ;;
    -h|--help) usage; exit 0 ;;
    --apply|'') command_name=all ;;
    *) command_name=all ;;
esac

case "$command_name" in
    names)
        exec "$repo_root/scripts/normalize_paths.sh" "$@"
        ;;
    md)
        exec "$repo_root/scripts/format_markdown.sh" "$@"
        ;;
    all)
        # 先改 Markdown 标题，再重命名路径；后一步会继续维护路径引用。
        "$repo_root/scripts/format_markdown.sh" "$@"
        exec "$repo_root/scripts/normalize_paths.sh" "$@"
        ;;
esac
