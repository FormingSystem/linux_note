#!/usr/bin/env bash
set -eu

# 卸载失败时停止，保留仍在运行的旧实例。
if ! sudo rmmod hello_note; then
    sudo dmesg | tail -n 30
    exit 1
fi

# 加载失败时明确返回失败；日志不能改变结果。
if ! sudo insmod ./hello_note.ko; then
    sudo dmesg | tail -n 30
    exit 1
fi

sudo dmesg | tail -n 20
