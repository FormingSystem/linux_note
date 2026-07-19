#!/usr/bin/env bash
set -euo pipefail

case "$(uname -s)" in
    MSYS*|MINGW*|CYGWIN*) ;;
    *)
        printf '此脚本只支持 Windows 上的 MSYS2 Bash。Linux 请运行 ./format.sh install。\n' >&2
        exit 2
        ;;
esac

if ! command -v pacman >/dev/null 2>&1; then
    printf '未找到 pacman；请使用 MSYS2 Bash，而不是 PowerShell、CMD 或 Git Bash。\n' >&2
    printf 'MSYS2 安装说明：https://www.msys2.org/\n' >&2
    exit 1
fi

python_package=python
case "${MSYSTEM:-}:$(command -v python3 2>/dev/null || true)" in
    UCRT64:*|*:/ucrt64/*) python_package=mingw-w64-ucrt-x86_64-python ;;
    MINGW64:*|*:/mingw64/*) python_package=mingw-w64-x86_64-python ;;
    CLANG64:*|*:/clang64/*) python_package=mingw-w64-clang-x86_64-python ;;
esac

printf '使用 MSYS2 软件包安装依赖（Python：%s）。\n' "$python_package"
pacman -S --needed --noconfirm bash git "$python_package" findutils coreutils

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
exec "$repo_root/format.sh" doctor
