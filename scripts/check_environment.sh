#!/usr/bin/env bash
set -uo pipefail

errors=0
warnings=0

pass() { printf '[OK]   %s\n' "$1"; }
fail() { printf '[FAIL] %s\n' "$1"; errors=$((errors + 1)); }
warn() { printf '[WARN] %s\n' "$1"; warnings=$((warnings + 1)); }

case "$(uname -s 2>/dev/null || true)" in
    Linux*) platform=Linux ;;
    MSYS*|MINGW*|CYGWIN*) platform='Windows/MSYS2' ;;
    *) platform='未知平台' ;;
esac
printf 'format.sh 环境诊断（%s）\n' "$platform"

if ((BASH_VERSINFO[0] >= 4)); then
    pass "Bash ${BASH_VERSION}（要求 4.0+）"
else
    fail "Bash ${BASH_VERSION} 过旧（要求 4.0+）"
fi

if command -v git >/dev/null 2>&1; then
    pass "$(git --version)"
else
    fail '未找到 Git'
fi

if command -v python3 >/dev/null 2>&1; then
    if python3 -c 'import sys; raise SystemExit(sys.version_info < (3, 9))'; then
        pass "$(python3 --version 2>&1)（要求 3.9+）"
    else
        fail "$(python3 --version 2>&1) 过旧（要求 3.9+）"
    fi
else
    fail '未找到 python3'
fi

for tool in find rmdir; do
    if command -v "$tool" >/dev/null 2>&1; then
        pass "基础工具 $tool"
    else
        fail "未找到基础工具 $tool"
    fi
done

encoding=$(python3 -c 'import locale; print(locale.getpreferredencoding(False))' 2>/dev/null || true)
if [[ ${encoding^^} == UTF-8 || ${encoding^^} == UTF8 ]]; then
    pass "默认字符编码 $encoding"
else
    warn "默认字符编码为 ${encoding:-未知}；脚本会强制 Python 使用 UTF-8"
fi

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    pass '当前目录位于 Git 工作树内'
else
    warn '当前目录不在 Git 工作树内；format.sh 必须从仓库中运行'
fi

printf '诊断完成：错误=%d，警告=%d\n' "$errors" "$warnings"
((errors == 0))
