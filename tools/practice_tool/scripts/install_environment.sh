#!/usr/bin/env bash

set -euo pipefail

practice_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$practice_dir/scripts/lib/platform_environment.sh"
practice_environment_init "$practice_dir"

if [[ "${PRACTICE_FORCE_NODE_UPGRADE:-0}" != "1" ]] && practice_node_is_supported; then
    exit 0
fi

if [[ "$PRACTICE_PLATFORM_FAMILY" = "msys2" ]]; then
    if practice_install_msys2_node; then
        exit 0
    else
        printf '[practice] MSYS2 安装后的 Node.js 仍低于 v%d。\n' "$PRACTICE_REQUIRED_NODE_MAJOR" >&2
        exit 1
    fi
fi

node_arch="$PRACTICE_NODE_ARCH"
required_node_major="$PRACTICE_REQUIRED_NODE_MAJOR"
preferred_node_majors="$PRACTICE_PREFERRED_NODE_MAJORS"
node_dist_sources="$PRACTICE_NODE_DIST_SOURCES"
practice_prepare_download_tools

runtime_root="$PRACTICE_RUNTIME_ROOT"
target_dir="$runtime_root/node"
cache_root="$PRACTICE_NODE_CACHE_ROOT"
offline_table="$PRACTICE_OFFLINE_NODE_TABLE"
mkdir -p "$runtime_root" "$cache_root"

resolve_practice_path() {
    practice_resolve_tool_path "$1"
}

extract_node_archive() {
    local archive_path="$1"
    local destination="$2"
    case "$archive_path" in
        *.tar.gz|*.tgz)
            practice_extract_tar gzip "$archive_path" "$destination"
            ;;
        *.tar.xz)
            practice_extract_tar xz "$archive_path" "$destination"
            ;;
        *)
            printf '[practice] Linux 不支持该离线包后缀：%s；请使用官方 .tar.gz、.tgz 或 .tar.xz。\n' "$archive_path" >&2
            return 1
            ;;
    esac
}

activate_offline_archive() {
    local archive_path="$1"
    local checksums_path="$2"
    local archive_name extracted_dir checksum_line official_name_pattern

    archive_name="$(basename "$archive_path")"
    if [[ ! -f "$checksums_path" || ! -f "$archive_path" ]]; then
        printf '[practice] 离线包或校验文件不存在：%s | %s\n' "$archive_path" "$checksums_path" >&2
        return 1
    fi
    official_name_pattern="^node-v([0-9]+)\.[0-9]+\.[0-9]+-linux-${node_arch}\.(tar\.gz|tgz|tar\.xz)$"
    if [[ ! "$archive_name" =~ $official_name_pattern ]]; then
        printf '[practice] 文件名与当前平台/架构不匹配：%s（期望 linux-%s 官方命名）。\n' "$archive_name" "$node_arch" >&2
        return 1
    fi
    if [[ "${BASH_REMATCH[1]}" -lt "$required_node_major" ]]; then
        printf '[practice] 离线包版本低于最低兼容线 v%d：%s\n' "$required_node_major" "$archive_name" >&2
        return 1
    fi
    checksum_line="$(grep "  $archive_name\$" "$checksums_path" || true)"
    if [[ -z "$checksum_line" ]]; then
        printf '[practice] SHASUMS256.txt 中没有 %s，忽略该缓存。\n' "$archive_name" >&2
        return 1
    fi
    if ! practice_verify_sha256_line "$checksum_line" "$(dirname "$archive_path")"; then
        printf '[practice] 离线缓存 %s 校验失败，忽略该文件。\n' "$archive_name" >&2
        return 1
    fi

    extracted_dir="$(mktemp -d "$runtime_root/node-extracted.XXXXXX")"
    if ! extract_node_archive "$archive_path" "$extracted_dir"; then
        rm -rf "$extracted_dir"
        return 1
    fi
    if [[ ! -x "$extracted_dir/bin/node" || ! -x "$extracted_dir/bin/npm" ]]; then
        rm -rf "$extracted_dir"
        return 1
    fi

    local version_before=""
    if [[ -x "$target_dir/bin/node" ]]; then
        version_before="$("$target_dir/bin/node" --version 2>/dev/null || true)"
    fi
    if [[ -e "$target_dir" ]]; then
        rm -rf "$target_dir"
    fi
    mv "$extracted_dir" "$target_dir"
    local version_after
    version_after="$("$target_dir/bin/node" --version)"
    practice_registry_record "nodejs-local-runtime" "$version_before" "$version_after" \
        "$target_dir" "tool-owned" "local-runtime" "$archive_path"
    case "$archive_path" in
        "$cache_root"/*)
            practice_registry_record "nodejs-download-cache" "" "$version_after" \
                "$(dirname "$archive_path")" "tool-owned" "download-cache" "$archive_path"
            ;;
        *)
            practice_registry_record "nodejs-offline-package" "" "$version_after" \
                "$archive_path" "external" "none" "user-specified"
            ;;
    esac
    printf '[practice] 已从缓存安装 %s。\n' "$version_after"
    return 0
}

install_from_table() {
    local table_path="$1"
    local enabled platform arch archive checksums archive_path checksums_path
    local installed=0

    if [[ ! -f "$table_path" ]]; then
        mkdir -p "$(dirname "$table_path")"
        cp "$practice_dir/config/offline_node_packages.example.tsv" "$table_path"
        printf '[practice] 已生成离线包表：%s\n' "$table_path"
        printf '%s\n' '[practice] 将目标行 enabled 改为 1，填写相对 practice_tool 根目录或绝对路径后重新运行。'
        return 1
    fi

    while IFS=$'\t' read -r enabled platform arch archive checksums; do
        checksums="${checksums%$'\r'}"
        [[ "$enabled" = "1" ]] || continue
        [[ "$platform" = "linux" && "$arch" = "$node_arch" ]] || continue
        archive_path="$(resolve_practice_path "$archive")"
        checksums_path="$(resolve_practice_path "$checksums")"
        if activate_offline_archive "$archive_path" "$checksums_path"; then
            installed=1
            break
        fi
    done < <(tail -n +2 "$table_path")

    [[ "$installed" -eq 1 ]]
}

offline_choice="${PRACTICE_INSTALL_MODE:-}"
if [[ -z "$offline_choice" && -t 0 && -t 1 ]]; then
    printf '%s\n' \
        '[practice] 请选择 Node.js 安装方式：' \
        '  [A] 自动安装（默认，5 秒后自动选择）' \
        '  [M] 手动指定一个离线安装包' \
        '  [T] 从离线安装表读取' \
        '----------------------------------------'
    printf '%s' '[practice] 请输入 A、M 或 T：'
    if read -r -n 1 -t 5 offline_choice; then
        printf '\n'
    else
        printf '\n[practice] 未选择，继续自动安装。\n'
    fi
fi

case "${offline_choice,,}" in
    m|manual)
        read -r -p '[practice] 归档路径（相对 practice_tool 根目录或绝对路径）: ' manual_archive
        read -r -p '[practice] SHASUMS256.txt 路径: ' manual_checksums
        if activate_offline_archive "$(resolve_practice_path "$manual_archive")" "$(resolve_practice_path "$manual_checksums")"; then
            exit 0
        fi
        printf '%s\n' '[practice] 手动离线包安装失败，继续自动安装。'
        ;;
    t|table)
        printf '[practice] 读取离线包表：%s\n' "$offline_table"
        if install_from_table "$offline_table"; then
            exit 0
        fi
        printf '%s\n' '[practice] 表格中没有可安装项，继续自动安装。'
        ;;
esac

install_official_node() {
    local major="$1"
    local source_root release_base attempt_dir archive_name archive_url version package_dir cached_dir cached_archive

    while IFS= read -r cached_dir; do
        cached_archive="$(find "$cached_dir" -maxdepth 1 -type f \( -name "node-v*-linux-$node_arch.tar.gz" -o -name "node-v*-linux-$node_arch.tgz" -o -name "node-v*-linux-$node_arch.tar.xz" \) -print -quit)"
        if [[ -n "$cached_archive" ]]; then
            cached_archive="$(basename "$cached_archive")"
            printf '[practice] 发现离线缓存：%s\n' "$cached_dir/$cached_archive"
            if activate_offline_archive "$cached_dir/$cached_archive" "$cached_dir/SHASUMS256.txt"; then
                return 0
            fi
        fi
    done < <(find "$cache_root" -mindepth 1 -maxdepth 1 -type d -name "v$major.*" -print | sort -Vr)

    for source_root in $node_dist_sources; do
        release_base="${source_root%/}/latest-v$major.x"
        attempt_dir="$(mktemp -d "$runtime_root/node-v$major.XXXXXX")"
        printf '[practice] 尝试 Node.js 下载源：%s（linux-%s）……\n' "$release_base" "$node_arch"
        if ! practice_download_file "$release_base/SHASUMS256.txt" "$attempt_dir/SHASUMS256.txt"; then
            rm -rf "$attempt_dir"
            continue
        fi

        archive_name="$(sed -n "s/^[0-9a-fA-F]\\{64\\}  \\(node-v[^ ]*-linux-$node_arch\\.tar\\.gz\\)$/\\1/p" "$attempt_dir/SHASUMS256.txt" | head -n 1)"
        if [[ -z "$archive_name" ]]; then
            printf '[practice] 当前源没有 Node.js v%s linux-%s 官方归档，切换下一下载源。\n' "$major" "$node_arch"
            rm -rf "$attempt_dir"
            continue
        fi

        archive_url="$release_base/$archive_name"
        version="${archive_name#node-v}"
        version="${version%%-linux-*}"
        package_dir="$cache_root/v$version"
        mkdir -p "$package_dir"
        cp "$attempt_dir/SHASUMS256.txt" "$package_dir/SHASUMS256.txt"
        printf '[practice] 下载 %s……\n' "$archive_name"
        if ! practice_download_file "$archive_url" "$package_dir/$archive_name"; then
            rm -f "$package_dir/$archive_name"
            rm -rf "$attempt_dir"
            continue
        fi

        rm -rf "$attempt_dir"
        if activate_offline_archive "$package_dir/$archive_name" "$package_dir/SHASUMS256.txt"; then
            return 0
        fi
    done
    return 1
}

for major in $preferred_node_majors; do
    if [[ "$major" -lt "$required_node_major" ]]; then
        continue
    fi
    if install_official_node "$major"; then
        exit 0
    fi
done

printf '[practice] 无法从已配置的 Node.js 下载源取得兼容版本（版本顺序：%s）。\n' "$preferred_node_majors" >&2
printf '[practice] 离线使用：把官方归档和同版本 SHASUMS256.txt 放到：\n' >&2
printf '[practice]   %s/v<版本>/\n' "$cache_root" >&2
printf '[practice] 示例：%s/v24.18.0/node-v24.18.0-linux-%s.tar.gz\n' "$cache_root" "$node_arch" >&2
exit 1
