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

## 1.2\_一次内核工作交付

[note_kref_work.c](note_kref_work.c)与[Makefile](Makefile)对应[P01 完整模块](../../../../knowledge/linux/object_lifetime/kref/P01_kref_要解决什么问题.md#1.16.1_运行一次真实工作交付)。它只投递一个新工作项，成功后创建者和 worker 各归还一份；卸载等待私有队列销毁，回调不重排。目标构建和观察步骤在正文；本轮未实际装卸目标模块。

ARM Clang 前端检查通过，纳入的 354 份头文件中非生成源码与官方固定提交无差异，生成头仍属于当前配置环境。宿主 C 控制夹具采用固定 kref_init/get/put 函数体，把底层计数、分配和队列明确替换为顺序模型，覆盖队列分配失败、对象分配失败、投递拒绝、worker 先结束、创建者先结束五条路径。此检查不等于目标构建链接、原子并发或 workqueue 实际运行。
