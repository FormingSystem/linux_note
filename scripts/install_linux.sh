#!/usr/bin/env bash
set -euo pipefail

if [[ $(uname -s) != Linux* ]]; then
    printf '此脚本只支持 Linux；Windows 请运行 scripts/install_windows.ps1。\n' >&2
    exit 2
fi

if ((EUID == 0)); then
    elevate=()
elif command -v sudo >/dev/null 2>&1; then
    elevate=(sudo)
else
    printf '安装系统软件包需要 root 权限或 sudo。\n' >&2
    exit 1
fi

if command -v apt-get >/dev/null 2>&1; then
    "${elevate[@]}" apt-get update
    "${elevate[@]}" apt-get install -y bash git python3 findutils coreutils
elif command -v dnf >/dev/null 2>&1; then
    "${elevate[@]}" dnf install -y bash git python3 findutils coreutils
elif command -v yum >/dev/null 2>&1; then
    "${elevate[@]}" yum install -y bash git python3 findutils coreutils
elif command -v pacman >/dev/null 2>&1; then
    "${elevate[@]}" pacman -S --needed --noconfirm bash git python findutils coreutils
elif command -v zypper >/dev/null 2>&1; then
    "${elevate[@]}" zypper --non-interactive install bash git python3 findutils coreutils
elif command -v apk >/dev/null 2>&1; then
    "${elevate[@]}" apk add bash git python3 findutils coreutils
else
    printf '不支持当前包管理器，请手动安装 Bash 4+、Git、Python 3.9+、findutils 和 coreutils。\n' >&2
    exit 1
fi

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
exec "$repo_root/format.sh" doctor
