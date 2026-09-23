#!/usr/bin/env bash
# 只调用C编译器生成汇编，不执行轮询程序。
set -euo pipefail
lab_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
lab_source="$lab_root/src/access_once.c"
lab_generated="$lab_root/generated"
lab_selected=all
lab_clean=false
while (($#)); do
    case "$1" in
        --compiler)
            [[ $# -ge 2 && ( $2 == gcc || $2 == clang ) ]] || {
                printf '%s\n' '用法：bash run.sh [--compiler gcc|clang] [--clean]' >&2
                exit 2
            }
            lab_selected=$2
            shift 2
            ;;
        --clean) lab_clean=true; shift ;;
        --help)
            printf '%s\n' '用法：bash run.sh [--compiler gcc|clang] [--clean]'
            exit 0
            ;;
        *) printf '未知参数：%s\n' "$1" >&2; exit 2 ;;
    esac
done
# 固定输出目录不得通过符号链接指向别处。
[[ ! -L "$lab_generated" ]] || { printf '%s\n' '拒绝使用链接generated目录' >&2; exit 1; }
if $lab_clean; then
    if [[ -d "$lab_generated" ]]; then
        [[ $(cd -- "$lab_generated" && pwd -P) == "$lab_generated" ]] || exit 1
        for lab_name in gcc clang; do
            rm -f -- "$lab_generated/${lab_name}_O0.s" \
                "$lab_generated/${lab_name}_O2.s" "$lab_generated/${lab_name}.txt"
        done
        rmdir -- "$lab_generated" 2>/dev/null || true # 其他手工材料保留。
    fi
    printf '%s\n' '已清理本实验已知生成文件；其他文件保留'
    exit 0
fi
mkdir -p -- "$lab_generated"
lab_count=0
for lab_name in gcc clang; do
    [[ $lab_selected == all || $lab_selected == "$lab_name" ]] || continue
    if ! lab_compiler=$(command -v "$lab_name"); then
        if [[ $lab_selected != all ]]; then
            printf '未找到指定编译器：%s\n' "$lab_name" >&2
            exit 1
        fi
        printf '跳过缺少的编译器：%s\n' "$lab_name"
        continue
    fi
    lab_report="$lab_generated/${lab_name}.txt"
    "$lab_compiler" --version > "$lab_report"
    "$lab_compiler" -dumpmachine >> "$lab_report"
    for lab_opt in O0 O2; do
        lab_command=("$lab_compiler" -std=gnu11 -Wall -Wextra -Werror "-$lab_opt"
            -S -fno-asynchronous-unwind-tables -fno-ident "$lab_source"
            -o "$lab_generated/${lab_name}_${lab_opt}.s")
        printf '%q ' "${lab_command[@]}" >> "$lab_report"
        printf '\n' >> "$lab_report"
        "${lab_command[@]}"
        printf '生成 %s_%s.s\n' "$lab_name" "$lab_opt"
    done
    lab_count=$((lab_count + 1))
done
[[ $lab_count -gt 0 ]] || { printf '%s\n' 'PATH中没有GCC或Clang' >&2; exit 1; }
printf '%s\n' '汇编生成完成；需人工检查内存操作和循环回边，不代表硬件或并发通过'
