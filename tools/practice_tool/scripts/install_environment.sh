#!/usr/bin/env bash

set -euo pipefail

if command -v npm >/dev/null 2>&1; then
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
        exit 0
        ;;
esac

if command -v winget.exe >/dev/null 2>&1; then
    printf '%s\n' '[practice] 使用 Windows winget 安装 Node.js LTS……'
    winget.exe install --id OpenJS.NodeJS.LTS --exact --silent \
        --accept-package-agreements --accept-source-agreements
    exit 0
fi

if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y nodejs npm
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y nodejs npm
elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -S --needed --noconfirm nodejs npm
else
    printf '%s\n' '[practice] 无法识别系统包管理器，请安装 Node.js LTS 后重试。' >&2
    exit 1
fi
