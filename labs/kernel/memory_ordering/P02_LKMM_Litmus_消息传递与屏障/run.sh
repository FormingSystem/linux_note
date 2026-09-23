#!/usr/bin/env bash
# Bash仅编排外部herd7；清单中的期望值不能冒充实际模型结果。
set -euo pipefail
lab_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
repo_root=$(cd -- "$lab_root/../../../.." && pwd -P)
model_root="$repo_root/research/source_reading/linux/tools/memory-model"
manifest="$lab_root/manifest.tsv"
generated="$lab_root/generated"
mode=run
filter=''
herd_name=herd7
fail() { printf '错误：%s\n' "$*" >&2; exit 1; }
usage() { printf '%s\n' '用法：bash run.sh [--check|--clean] [--filter 文本] [--herd 可执行文件]'; }
while (($#)); do
    case "$1" in
        --check|--clean)
            [[ $mode == run ]] || fail '检查和清理不能组合'
            mode=${1#--}; shift ;;
        --filter|--herd)
            [[ $# -ge 2 && -n $2 ]] || { usage >&2; exit 2; }
            if [[ $1 == --filter ]]; then filter=$2; else herd_name=$2; fi
            shift 2 ;;
        --help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done
[[ -f $manifest ]] || fail '缺少manifest.tsv'
names=(); expected=(); declare -A seen=()
model_version=''; schema=''
while IFS= read -r line || [[ -n $line ]]; do
    line=${line%$'\r'}
    case "$line" in
        '# schema_version: '*) [[ -z $schema ]] || fail '重复schema'; schema=${line#*: }; continue ;;
        '# linux_model: '*) [[ -z $model_version ]] || fail '重复模型版本'; model_version=${line#*: }; continue ;;
        '#'*|'') continue ;;
    esac
    [[ $line == *$'\t'* ]] || fail '清单必须使用制表符分隔'
    name=${line%%$'\t'*}; result=${line#*$'\t'}
    [[ $name =~ ^[A-Za-z0-9_+-]+\.litmus$ ]] || fail "非法文件名：$name"
    [[ $result == Sometimes || $result == Never || $result == Always ]] || fail "非法预期：$result"
    [[ -z ${seen[$name]+present} ]] || fail "重复测试：$name"
    seen[$name]=1; names+=("$name"); expected+=("$result")
done < "$manifest"
[[ $schema == 1 && $model_version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail '清单头部无效'
[[ ${#names[@]} -gt 0 ]] || fail '清单没有测试'

# 清理只覆盖已知输出，不递归删除目录，不跟随generated链接。
[[ ! -L $generated ]] || fail '拒绝链接generated目录'
if [[ -e $generated ]]; then
    [[ -d $generated && $(cd -- "$generated" && pwd -P) == "$generated" ]] || fail '输出目录边界不符'
fi
outputs=(run.txt inputs.sha256 herd_version.txt)
for name in "${names[@]}"; do
    outputs+=("${name%.litmus}.stdout.txt" "${name%.litmus}.stderr.txt")
done
if [[ $mode == clean ]]; then
    for name in "${outputs[@]}"; do rm -f -- "$generated/$name"; done
    rmdir -- "$generated" 2>/dev/null || true
    printf '%s\n' '已清理已知输出；其他手工文件保留'
    exit 0
fi

model_files=(linux-kernel.cfg linux-kernel.def linux-kernel.bell linux-kernel.cat lock.cat)
for name in "${model_files[@]}"; do [[ -f $model_root/$name ]] || fail "缺少模型：$name"; done
selected=()
for i in "${!names[@]}"; do
    name=${names[i]}
    [[ -f $lab_root/tests/$name ]] || fail "缺少测试：$name"
    IFS= read -r first_line < "$lab_root/tests/$name"
    [[ ${first_line%$'\r'} == "C ${name%.litmus}" ]] || fail "测试声明与文件名不符：$name"
    [[ ${name,,} != *"${filter,,}"* ]] || selected+=("$i")
done
[[ ${#selected[@]} -gt 0 ]] || fail '筛选没有匹配测试'
printf '模型清单版本：%s；静态检查测试=%d；本次选择=%d\n' "$model_version" "${#names[@]}" "${#selected[@]}"
if ! herd_path=$(command -v -- "$herd_name"); then
    if [[ $mode == check ]]; then
        printf '%s\n' '静态检查通过；未找到herd7，未执行模型'
        exit 0
    fi
    printf '%s\n' '未找到herd7，未执行模型' >&2
    exit 2
fi
herd_path=$(realpath -e -- "$herd_path")
[[ -x $herd_path ]] || fail 'herd程序不可执行'
if [[ $mode == check ]]; then
    "$herd_path" -version
    printf '%s\n' '静态检查通过；仅查询工具版本，未执行模型'
    exit 0
fi

mkdir -p -- "$generated"
for name in "${outputs[@]}"; do
    [[ ! -L $generated/$name && ( ! -e $generated/$name || -f $generated/$name ) ]] || fail "输出不是普通文件：$name"
done
# 重新运行先移除本脚本的旧结果，避免未执行项看起来沿用了成功输出。
for name in "${outputs[@]}"; do rm -f -- "$generated/$name"; done
report="$generated/run.txt"
printf 'status=running\nlinux_model=%s\nmodel_directory=%s\nherd=%s\n' "$model_version" "$model_root" "$herd_path" > "$report"
trap 'rc=$?; if ((rc)); then printf "status=failed\n" >> "$report"; fi; printf "exit_status=%d\n" "$rc" >> "$report"' EXIT
printf 'utc_start=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$report"
printf 'repository_head=%s\n' "$(git -C "$repo_root" rev-parse HEAD)" >> "$report"
(cd -- "$repo_root" && sha256sum -- "${manifest#"$repo_root/"}" \
    "${model_files[@]/#/research/source_reading/linux/tools/memory-model/}" \
    "${names[@]/#/labs/kernel/memory_ordering/P02_LKMM_Litmus_消息传递与屏障/tests/}") > "$generated/inputs.sha256"
"$herd_path" -version > "$generated/herd_version.txt" 2>&1 || fail '工具版本查询失败，见herd_version.txt'
for i in "${selected[@]}"; do
    name=${names[i]}; test_name=${name%.litmus}
    command=("$herd_path" -conf linux-kernel.cfg "$lab_root/tests/$name")
    printf 'command=' >> "$report"; printf '%q ' "${command[@]}" >> "$report"; printf '\n' >> "$report"
    stdout="$generated/$test_name.stdout.txt"; stderr="$generated/$test_name.stderr.txt"
    if (cd -- "$model_root" && "${command[@]}") > "$stdout" 2> "$stderr"; then
        command_status=0
    else
        command_status=$?
    fi
    printf 'test=%s process_exit=%d\n' "$test_name" "$command_status" >> "$report"
    [[ $command_status == 0 ]] || fail "herd执行失败：$test_name，标准输出和错误均已保存"
    observation_count=0; actual=''
    while read -r tag observed_name classification positive negative extra; do
        [[ $tag == Observation ]] || continue
        observation_count=$((observation_count + 1))
        [[ $observed_name == "$test_name" && -z $extra ]] || fail "Observation身份或字段异常：$test_name"
        [[ $classification == Sometimes || $classification == Never || $classification == Always ]] || fail '未知Observation分类'
        [[ $positive =~ ^[0-9]+$ && $negative =~ ^[0-9]+$ ]] || fail 'Observation计数无效'
        [[ ! $positive =~ ^0+$ || ! $negative =~ ^0+$ ]] || fail '模型没有允许执行，不以空集合计为通过'
        case "$classification" in
            Never) [[ $positive =~ ^0+$ && ! $negative =~ ^0+$ ]] || fail 'Never计数矛盾' ;;
            Always) [[ ! $positive =~ ^0+$ && $negative =~ ^0+$ ]] || fail 'Always计数矛盾' ;;
            Sometimes) [[ ! $positive =~ ^0+$ && ! $negative =~ ^0+$ ]] || fail 'Sometimes计数矛盾' ;;
        esac
        actual=$classification
    done < "$stdout"
    [[ $observation_count == 1 ]] || fail "Observation必须恰好一条：$test_name"
    printf 'observation=%s expected=%s\n' "$actual" "${expected[i]}" >> "$report"
    [[ $actual == "${expected[i]}" ]] || fail "结果与预期不符：$test_name"
    printf '%s: %s（预期一致）\n' "$test_name" "$actual"
done
printf 'status=passed\n' >> "$report"
printf '%s\n' '所选模型结果与清单一致；完整输出保存在generated/'
