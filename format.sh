#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

usage() {
    cat <<'EOF'
用法：
  ./format.sh check [all|paths|headings|metadata|links|mermaid] [--summary] [路径...]
  ./format.sh fix   [all|paths|headings|metadata|links|mermaid] [--summary] [路径...]
  ./format.sh doctor
  ./format.sh install

动作：
  check       只检查并预览，不修改文件
  fix         写入所有可安全确定的修复

范围：
  all         路径、Markdown标题、元数据和文档链接（默认）
  paths       仅处理文件与目录名
  headings    仅处理 Markdown 标题及标题锚点
  metadata    仅处理文档ID、标题、类型、状态和领域
  links       仅处理 Markdown、Obsidian、Canvas 和 Base 链接
  mermaid     检查或修复跨渲染器不兼容的 Mermaid 伪层级子图

环境命令：
  doctor      只读扫描系统、版本、编码和仓库环境
  install     安装缺少的依赖，然后自动运行 doctor

选项：
  --summary   只显示统计摘要
  -h, --help  显示帮助

路径可以是单个文件、多个文件或目录；省略路径时处理全仓。
在 Bash 中推荐使用正斜杠路径；使用 Windows 反斜杠路径时必须给整个路径加引号。

示例：
  ./format.sh fix headings knowledge/foundations/example.md
  ./format.sh fix headings 'knowledge\foundations\example.md'

EOF
}

if (($# == 0)); then
    usage
    exit 0
fi

action=check
scope=all
arguments=()

if (($# > 0)); then
    case "$1" in
        check|fix)
            action=$1
            shift
            ;;
        doctor)
            shift
            if (($# > 0)); then
                printf 'doctor 不接受其他参数。\n' >&2
                exit 2
            fi
            exec "$repo_root/scripts/check_environment.sh"
            ;;
        install)
            shift
            if (($# > 0)); then
                printf 'install 不接受其他参数。\n' >&2
                exit 2
            fi
            case "$(uname -s)" in
                Linux*) exec "$repo_root/scripts/install_linux.sh" ;;
                MSYS*|MINGW*|CYGWIN*) exec "$repo_root/scripts/install_windows.sh" ;;
                *) printf '不支持当前系统：%s\n' "$(uname -s)" >&2; exit 2 ;;
            esac
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf '缺少动作 check 或 fix：%s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
fi

if (($# > 0)); then
    case "$1" in
        all|paths|headings|metadata|links|mermaid)
            scope=$1
            shift
            ;;
    esac
fi

for argument in "$@"; do
    case "$argument" in
        -h|--help)
            usage
            exit 0
            ;;
        --apply)
            printf '不支持 --apply；请使用 fix 动作。\n' >&2
            exit 2
            ;;
        *)
            arguments+=("$argument")
            ;;
    esac
done

if [[ $action == fix ]]; then
    arguments=(--apply "${arguments[@]}")
fi

case "$scope" in
    paths)
        exec "$repo_root/scripts/normalize_paths.sh" "${arguments[@]}"
        ;;
    headings)
        exec "$repo_root/scripts/format_markdown.sh" "${arguments[@]}"
        ;;
    metadata)
        exec "$repo_root/scripts/format_metadata.sh" "${arguments[@]}"
        ;;
    links)
        exec "$repo_root/scripts/update_links.sh" "${arguments[@]}"
        ;;
    mermaid)
        if [[ $action == fix ]]; then
            exec "$repo_root/scripts/fix_mermaid_level_subgraphs.sh" fix
        fi
        exec "$repo_root/scripts/fix_mermaid_level_subgraphs.sh" check
        ;;
    all)
        # 依次维护标题、元数据、路径和链接；元数据步骤不改正文标题。
        if [[ $action == check ]]; then
            status=0
            "$repo_root/scripts/format_markdown.sh" "${arguments[@]}" || status=1
            "$repo_root/scripts/format_metadata.sh" "${arguments[@]}" || status=1
            "$repo_root/scripts/normalize_paths.sh" "${arguments[@]}" || status=1
            "$repo_root/scripts/update_links.sh" "${arguments[@]}" || status=1
            "$repo_root/scripts/fix_mermaid_level_subgraphs.sh" check || status=1
            exit "$status"
        fi
        "$repo_root/scripts/format_markdown.sh" "${arguments[@]}"
        "$repo_root/scripts/format_metadata.sh" "${arguments[@]}"
        "$repo_root/scripts/normalize_paths.sh" "${arguments[@]}"
        "$repo_root/scripts/update_links.sh" "${arguments[@]}"
        exec "$repo_root/scripts/fix_mermaid_level_subgraphs.sh" fix
        ;;
esac
