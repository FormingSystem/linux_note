#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

usage() {
    cat <<'EOF'
用法：
  ./format.sh [all] [--apply] [--summary] [路径...]    文件名 + Markdown 标题 + 链接
  ./format.sh names [--apply] [路径...]                 仅格式化文件和目录名
  ./format.sh md [--apply] [--summary] [路径...]        仅格式化 Markdown 标题
  ./format.sh links [--apply] [--summary] [路径...]     仅扫描并更新文档链接

不给路径时处理全仓；指定文件或目录时只处理对应范围。默认仅预览。
EOF
}

command_name=${1:-all}
case "$command_name" in
    all|names|md|links) shift || true ;;
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
    links)
        exec "$repo_root/scripts/update_links.sh" "$@"
        ;;
    all)
        # 先改标题，再重命名路径，最后统一检查所有链接。
        "$repo_root/scripts/format_markdown.sh" "$@"
        "$repo_root/scripts/normalize_paths.sh" "$@"
        exec "$repo_root/scripts/update_links.sh" "$@"
        ;;
esac
