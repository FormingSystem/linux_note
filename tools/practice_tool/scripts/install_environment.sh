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

if command -v curl >/dev/null 2>&1; then
    download_file() {
        curl --fail --location --retry 2 --connect-timeout 15 --progress-bar "$1" -o "$2"
    }
elif command -v wget >/dev/null 2>&1; then
    download_file() {
        wget --tries=2 --timeout=15 --show-progress "$1" -O "$2"
    }
else
    printf '%s\n' '[practice] 下载 Node.js 需要 curl 或 wget，请先安装其中一个。' >&2
    exit 1
fi

if ! command -v sha256sum >/dev/null 2>&1; then
    printf '%s\n' '[practice] 校验 Node.js 下载需要 sha256sum。' >&2
    exit 1
fi

runtime_root="$practice_dir/.local/runtime"
target_dir="$runtime_root/node"
mkdir -p "$runtime_root"

install_official_node() {
    local major="$1"
    local release_base="https://nodejs.org/dist/latest-v$major.x"
    local attempt_dir archive_name archive_url
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
    printf '[practice] 下载 %s……\n' "$archive_name"
    if ! download_file "$archive_url" "$attempt_dir/$archive_name"; then
        rm -rf "$attempt_dir"
        return 1
    fi

    if ! (
        cd "$attempt_dir"
        grep "  $archive_name\$" SHASUMS256.txt | sha256sum --check -
    ); then
        printf '[practice] %s 校验失败，尝试较低版本。\n' "$archive_name" >&2
        rm -rf "$attempt_dir"
        return 1
    fi

    mkdir -p "$attempt_dir/extracted"
    if ! tar -xzf "$attempt_dir/$archive_name" --strip-components=1 -C "$attempt_dir/extracted"; then
        rm -rf "$attempt_dir"
        return 1
    fi
    if [[ ! -x "$attempt_dir/extracted/bin/node" || ! -x "$attempt_dir/extracted/bin/npm" ]]; then
        rm -rf "$attempt_dir"
        return 1
    fi

    if [[ -e "$target_dir" ]]; then
        rm -rf "$target_dir"
    fi
    mv "$attempt_dir/extracted" "$target_dir"
    rm -rf "$attempt_dir"
    printf '[practice] 已安装 %s 到工具本地目录。\n' "$("$target_dir/bin/node" --version)"
    return 0
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
printf '[practice] 请检查网络后重试，或手动安装 Node.js v%d 以上版本。\n' "$required_node_major" >&2
exit 1
