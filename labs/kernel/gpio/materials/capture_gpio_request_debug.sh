#!/bin/sh
# 目标Linux：暂时启用一个请求失败日志点，不改已有打印设置。
set -eu
control=/proc/dynamic_debug/control
if ! test -r "$control"; then
    control=/sys/kernel/debug/dynamic_debug/control
fi
test -r "$control"
test -w "$control"
capture_dir=$(mktemp -d ./gpio_request_debug.XXXXXX)
awk '$1 ~ /^drivers\/gpio\/gpiolib[.]c:/ && $2 ~ /]gpiod_request$/ {print}' \
    "$control" > "$capture_dir/site.before.txt"
test "$(wc -l < "$capture_dir/site.before.txt")" -eq 1
cat "$capture_dir/site.before.txt"
changed=0
cleanup() {
    if test "$changed" -eq 1; then
        printf '%s\n' 'file drivers/gpio/gpiolib.c func gpiod_request -p' \
            > "$control" || printf '%s\n' '恢复失败，请按保存的日志点状态恢复。' >&2
    fi
}
trap cleanup 0
trap 'exit 130' INT
trap 'exit 143' TERM
flags=$(awk '{print $3}' "$capture_dir/site.before.txt")
case "$flags" in
    *p*) : ;; # 原来已开，保持原样，不登记为本次修改。
    *)
        changed=1
        printf '%s\n' 'file drivers/gpio/gpiolib.c func gpiod_request +p' > "$control"
        ;;
esac
printf '%s\n' '执行一次已准备好的请求故障复现，保存对应内核日志后按回车。'
read -r reply
