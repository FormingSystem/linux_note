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
路径解析兼容正斜杠、Windows反斜杠、盘符绝对路径，以及UCRT中被反斜杠或空格拆散的复制路径；
只有在仓库内唯一匹配时才会自动还原，存在歧义时不会猜测。

示例：
  ./format.sh fix headings knowledge/foundations/example.md
  ./format.sh fix headings 'knowledge\foundations\example.md'
  ./format.sh fix headings F:\repo\knowledge\foundations\example.md

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

# Bash 会在脚本接收参数前处理未加引号的反斜杠和空格。对于从 Windows
# 终端或编辑器复制的路径，只能依据仓库中的真实路径做唯一反向匹配。
path_key() {
    local value=${1,,}
    value=${value//\\/}
    value=${value//\//}
    value=${value//:/}
    value=${value//\*/_}
    value=${value//$' '/}
    value=${value//$'\t'/}
    PATH_KEY=$value
}

resolve_path_arguments() {
    local -a inventory=() inventory_relative=() inventory_keys=() resolved=()
    local candidate relative normalized key joined joined_key relative_key absolute_key key_index
    local index next match_count matched consumed needs_inventory=false

    # 常规相对路径和加引号的Windows路径可以直接完成规范化，无需扫描仓库。
    for index in "${!arguments[@]}"; do
        candidate=${arguments[index]}
        [[ $candidate == --* ]] && continue
        normalized=${candidate//\\//}
        if [[ -e $repo_root/$normalized || -e $normalized ]]; then
            arguments[index]=$normalized
        else
            needs_inventory=true
        fi
    done
    [[ $needs_inventory == false ]] && return 0

    while IFS= read -r -d '' candidate; do
        relative=${candidate#./}
        inventory+=("$candidate")
        inventory_relative+=("$relative")
        path_key "$relative"
        relative_key=$PATH_KEY
        path_key "$repo_root/$relative"
        absolute_key=$PATH_KEY
        inventory_keys+=("$relative_key|$absolute_key")
    done < <(cd "$repo_root" && find . -path './.git' -prune -o -print0)

    index=0
    while ((index < ${#arguments[@]})); do
        candidate=${arguments[index]}
        if [[ $candidate == --* ]]; then
            resolved+=("$candidate")
            ((index += 1))
            continue
        fi

        normalized=${candidate//\\//}
        if [[ -e $repo_root/$normalized || -e $normalized ]]; then
            resolved+=("$normalized")
            ((index += 1))
            continue
        fi

        joined=$candidate
        matched=
        consumed=1
        for ((next = index; next < ${#arguments[@]}; next++)); do
            if ((next > index)); then
                [[ ${arguments[next]} == --* ]] && break
                joined+="${arguments[next]}"
                consumed=$((next - index + 1))
            fi
            path_key "$joined"
            joined_key=$PATH_KEY
            match_count=0
            matched=
            for key_index in "${!inventory[@]}"; do
                IFS='|' read -r relative_key absolute_key <<< "${inventory_keys[key_index]}"
                if [[ $joined_key == "$relative_key" || $joined_key == "$absolute_key" ]]; then
                    matched=${inventory_relative[key_index]}
                    ((match_count += 1))
                fi
            done
            if ((match_count == 1)); then
                break
            fi
            matched=
        done

        if [[ -n $matched ]]; then
            printf '已识别Windows路径：%s\n' "$matched" >&2
            resolved+=("$matched")
            ((index += consumed))
        else
            resolved+=("$candidate")
            ((index += 1))
        fi
    done
    arguments=("${resolved[@]}")
}

if ((${#arguments[@]} > 0)); then
    resolve_path_arguments
fi

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
