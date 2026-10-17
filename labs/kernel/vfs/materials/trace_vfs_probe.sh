#!/usr/bin/env bash
set -eu
trace_root=/sys/kernel/tracing
probe_program=/tmp/vfs_probe
if [ ! -x "$probe_program" ]; then
    printf '请先编译 %s\n' "$probe_program" >&2
    exit 1
fi
if [ ! -w "$trace_root/instances" ]; then
    printf '需要可用的tracefs及创建实例的权限\n' >&2
    exit 1
fi
for event_name in sys_enter_openat sys_exit_openat; do
    if [ ! -r "$trace_root/events/syscalls/$event_name/format" ]; then
        printf '当前内核没有可用事件：%s\n' "$event_name" >&2
        exit 1
    fi
done
trace_instance="$trace_root/instances/vfs_probe_$$"
# mkdir失败就停止，不接管同名既有实例。
mkdir -- "$trace_instance"
cleanup_trace() {
    printf '0\n' > "$trace_instance/tracing_on"
    rmdir -- "$trace_instance"
}
trap cleanup_trace EXIT
printf '0\n' > "$trace_instance/tracing_on"
trace_output=$(mktemp -d /tmp/vfs_trace.XXXXXX)
printf '本次记录目录：%s\n' "$trace_output"
for event_name in sys_enter_openat sys_exit_openat; do
    printf '1\n' > "$trace_instance/events/syscalls/$event_name/enable"
done
command_status=0
# 子shell先登记自己的PID，再开始记录；exec保留该PID执行被观察程序。
bash -c '
    set -eu
    printf "%s\n" "$$" > "$1/set_event_pid"
    printf "1\n" > "$1/tracing_on"
    exec "$2"
' _ "$trace_instance" "$probe_program" || command_status=$?
printf '0\n' > "$trace_instance/tracing_on"
cat "$trace_instance/trace" > "$trace_output/trace.txt"
for stats_file in "$trace_instance"/per_cpu/cpu*/stats; do
    [ -r "$stats_file" ] || continue
    printf '\n%s\n' "$stats_file"
    cat "$stats_file"
done > "$trace_output/stats.txt"
printf '记录：%s/trace.txt\n丢失统计：%s/stats.txt\n' "$trace_output" "$trace_output"
exit "$command_status"
