#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 2 ]]; then
    printf '用法：build_conf.sh Linux源码仓库绝对路径 新工具目录绝对路径\n' >&2
    exit 2
fi
linux_src=$1
tool_root=$2
revision=dfaf2136deb2af2e60b994421281ba42f1c087e0
case "$linux_src:$tool_root" in
    /*:/*) ;;
    *) printf '两个路径都必须是当前Bash环境中的绝对路径。\n' >&2; exit 2 ;;
esac
if [[ -e "$tool_root" ]]; then
    printf '工具目录已经存在，请选择新的空路径。\n' >&2
    exit 2
fi

# 从固定对象导出工具源文件；不切换或修改用户的Linux工作树。
git -C "$linux_src" cat-file -e "$revision^{commit}"
mkdir -p "$tool_root/source"
git -C "$linux_src" archive "$revision" scripts/kconfig scripts/include |
    tar -x -C "$tool_root/source"

# 构建的是宿主配置解析器，不是目标内核，不需要交叉编译器。
cd "$tool_root/source/scripts/kconfig"
bison -o parser.tab.c --defines=parser.tab.h -t -l parser.y
flex -o lexer.lex.c lexer.l
cc -D_GNU_SOURCE -O2 -I. -I../include \
    conf.c confdata.c expr.c lexer.lex.c menu.c parser.tab.c \
    preprocess.c symbol.c util.c -o "$tool_root/conf"
printf '配置工具已生成：%s/conf\n' "$tool_root"
