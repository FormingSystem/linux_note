---
id: labs.kernel.object_lifetime.materials
title: "对象生命周期实验材料"
kind: reference
status: evolving
domains:
  - linux
  - kernel
  - object_lifetime
---

# 第1章\_对象生命周期实验材料

## 1.1\_引用责任与交付

[reference_ownership.c](reference_ownership.c)对应[kref 问题入口的完整程序](../../../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.6.1_运行完整的责任交接模型)。在宿主目录用 `cc -std=c11 -Wall -Wextra -Werror -O2 reference_ownership.c -o reference_ownership` 编译并运行，保留断言。输出为 `accept=0 released=1` 和 `accept=1 released=2`，表示拒绝与接收两条路径分别完成一次最终回收。

程序使用普通 C 分配对象和责任槽，按顺序安排动作，不是 Linux `kref` 的替代实现。字段借用、创建者与处理者两个结束顺序、直接转交和拒绝归还的推导见正文，不在此复制另一份教程。

本轮宿主检查包含六种所有权调度组合、两条受控分配失败路径和槽责任求和。它们未验证内核模块、真实 workqueue、并发访问、原子计数、内存序、饱和告警或 KASAN；没有主动执行 UAF。
