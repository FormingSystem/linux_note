#!/usr/bin/env bash

set -euo pipefail

practice_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
required_node_major=18
preferred_node_majors="${PRACTICE_NODE_MAJORS:-24 22 20 18}"

node_is_supported() {
    command -v node >/dev/null 2>&1 &&
        [[ "$(node -p 'Number(process.versions.node.split(".")[0])' 2>/dev/null || printf '0')" -ge "$required_node_major" ]] &&
        command -v npm >/dev/null 2>&1
}

if [[ "${PRACTICE_FORCE_NODE_UPGRADE:-0}" != "1" ]] && node_is_supported; then
    exit 0
fi

case "${MSYSTEM:-}" in
    UCRT64|MINGW64|MINGW32|CLANG64|CLANG32|CLANGARM64)
        if ! command -v pacman >/dev/null 2>&1; then
            printf '%s\n' '[practice] 当前是 MSYS2，但未找到 pacman。请修复 MSYS2 安装后重试。' >&2
            exit 1
        fi
        if [[ -z "${MINGW_PACKAGE_PREFIX:-}" ]]; then
            printf '%s\n' '[practice] 无法确定当前 MSYS2 软件包前缀，请从 UCRT64 终端重新运行。' >&2
            exit 1
        fi
        printf '[practice] 使用 MSYS2 pacman 安装 %s-nodejs（无需 sudo）……\n' "$MINGW_PACKAGE_PREFIX"
        pacman -S --needed --noconfirm "${MINGW_PACKAGE_PREFIX}-nodejs"
        hash -r
        if node_is_supported; then
            exit 0
        fi
        printf '[practice] MSYS2 安装后的 Node.js 仍低于 v%d。\n' "$required_node_major" >&2
        exit 1
        ;;
esac

if command -v winget.exe >/dev/null 2>&1; then
    printf '%s\n' '[practice] 使用 Windows winget 安装最新 Node.js LTS……'
    winget.exe install --id OpenJS.NodeJS.LTS --exact --silent \
        --accept-package-agreements --accept-source-agreements
    exit 0
fi

case "$(uname -m)" in
    x86_64|amd64) node_arch="x64" ;;
    aarch64|arm64) node_arch="arm64" ;;
    armv7l) node_arch="armv7l" ;;
    *)
        printf '[practice] Node.js 官方 Linux 二进制暂不支持当前架构：%s。\n' "$(uname -m)" >&2
        exit 1
        ;;
esac

download_available=1
if command -v curl >/dev/null 2>&1; then
    download_file() {
        curl --fail --location --retry 2 --connect-timeout 15 --progress-bar "$1" -o "$2"
    }
elif command -v wget >/dev/null 2>&1; then
    download_file() {
        wget --tries=2 --timeout=15 --show-progress "$1" -O "$2"
    }
else
    download_available=0
fi

if ! command -v sha256sum >/dev/null 2>&1; then
    printf '%s\n' '[practice] 校验 Node.js 下载需要 sha256sum。' >&2
    exit 1
fi

runtime_root="$practice_dir/.local/runtime"
target_dir="$runtime_root/node"
cache_root="$practice_dir/.local/downloads/node"
offline_table="${PRACTICE_OFFLINE_NODE_TABLE:-$practice_dir/config/offline_node_packages.local.tsv}"
mkdir -p "$runtime_root" "$cache_root"

resolve_practice_path() {
    local value="$1"
    if [[ "$value" = /* ]]; then
        printf '%s\n' "$value"
    else
        printf '%s\n' "$practice_dir/$value"
    fi
}

extract_node_archive() {
    local archive_path="$1"
    local destination="$2"
    case "$archive_path" in
        *.tar.gz|*.tgz)
            tar -xzf "$archive_path" --strip-components=1 -C "$destination"
            ;;
        *.tar.xz)
            tar -xJf "$archive_path" --strip-components=1 -C "$destination"
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
    local archive_name extracted_dir checksum_line

    archive_name="$(basename "$archive_path")"
    if [[ ! -f "$checksums_path" || ! -f "$archive_path" ]]; then
        printf '[practice] 离线包或校验文件不存在：%s | %s\n' "$archive_path" "$checksums_path" >&2
        return 1
    fi
    if [[ ! "$archive_name" =~ ^node-v([0-9]+)\.[0-9]+\.[0-9]+-linux-$node_arch\.(tar\.gz|tgz|tar\.xz)$ ]]; then
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
    if ! (
        cd "$(dirname "$archive_path")"
        printf '%s\n' "$checksum_line" | sha256sum --check -
    ); then
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

    if [[ -e "$target_dir" ]]; then
        rm -rf "$target_dir"
    fi
    mv "$extracted_dir" "$target_dir"
    printf '[practice] 已从缓存安装 %s。\n' "$("$target_dir/bin/node" --version)"
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
    printf '[practice] Node.js 安装方式（5 秒后自动联网）：[A]自动 [M]手动指定一个离线包 [T]读取离线包表：'
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
    local release_base="https://nodejs.org/dist/latest-v$major.x"
    local attempt_dir archive_name archive_url version package_dir cached_dir cached_archive

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

    if [[ "$download_available" -ne 1 ]]; then
        return 1
    fi

    attempt_dir="$(mktemp -d "$runtime_root/node-v$major.XXXXXX")"

    printf '[practice] 查询 Node.js 官方 latest-v%s.x（linux-%s）……\n' "$major" "$node_arch"
    if ! download_file "$release_base/SHASUMS256.txt" "$attempt_dir/SHASUMS256.txt"; then
        rm -rf "$attempt_dir"
        return 1
    fi

    archive_name="$(sed -n "s/^[0-9a-fA-F]\\{64\\}  \\(node-v[^ ]*-linux-$node_arch\\.tar\\.gz\\)$/\\1/p" "$attempt_dir/SHASUMS256.txt" | head -n 1)"
    if [[ -z "$archive_name" ]]; then
        printf '[practice] Node.js v%s 没有 linux-%s 官方归档，尝试较低版本。\n' "$major" "$node_arch"
        rm -rf "$attempt_dir"
        return 1
    fi

    archive_url="$release_base/$archive_name"
    version="${archive_name#node-v}"
    version="${version%%-linux-*}"
    package_dir="$cache_root/v$version"
    mkdir -p "$package_dir"
    cp "$attempt_dir/SHASUMS256.txt" "$package_dir/SHASUMS256.txt"
    printf '[practice] 下载 %s……\n' "$archive_name"
    if ! download_file "$archive_url" "$package_dir/$archive_name"; then
        rm -f "$package_dir/$archive_name"
        rm -rf "$attempt_dir"
        return 1
    fi

    rm -rf "$attempt_dir"
    activate_offline_archive "$package_dir/$archive_name" "$package_dir/SHASUMS256.txt"
}

for major in $preferred_node_majors; do
    if [[ "$major" -lt "$required_node_major" ]]; then
        continue
    fi
    if install_official_node "$major"; then
        exit 0
    fi
done

printf '[practice] 无法从 Node.js 官方地址取得任何兼容版本（尝试顺序：%s）。\n' "$preferred_node_majors" >&2
printf '[practice] 离线使用：把官方归档和同版本 SHASUMS256.txt 放到：\n' >&2
printf '[practice]   %s/v<版本>/\n' "$cache_root" >&2
printf '[practice] 示例：%s/v24.18.0/node-v24.18.0-linux-%s.tar.gz\n' "$cache_root" "$node_arch" >&2
exit 1
