---
id: research.source_reading.linked_list.index
title: "Linux 6.12 链表源码阅读索引"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_Linux\_6.12\_链表源码阅读索引

读者已经从[教材](../../../../knowledge/linux/data_structures/单链表_linked_list/大纲.md)知道增删不等于分配回收。源码阅读要继续区分“具体哪条边被改”“检查器读哪些值”“谁提供同步”三个问题。

## 1.1\_版本和阅读任务

| 身份 | 本组使用值 |
| --- | --- |
| 官方来源 | https://github.com/nxp-imx/linux-imx.git |
| 分支与标签 | lf-6.12.y；lf-6.12.20-2.0.0 |
| 固定提交 | dfaf2136deb2af2e60b994421281ba42f1c087e0 |
| 版本 | Linux 6.12.20 |
| 架构边界 | 通用 list.h 为主；涉及内存访问时核对 ARM，不能按总线位宽猜原子性 |

统一身份见[Linux 源码基线](../../linux/SOURCE_BASELINE.md)。本地三个实验提交不参与分析。当前访问树是 ARM、PREEMPT_NONE、TINY_RCU、非 SMP 配置，启用 PROVE_LOCKING 不表示已启用 DEBUG_LIST；静态检查也不能模拟多 CPU 运行。

## 1.2\_由结论进入唯一实现

| 问题 | 导读与具体实现 | 上游路径 |
| --- | --- | --- |
| 头怎样初始化，四条边怎样接 | [拓扑导读](P02_拓扑修改与发布边界导读.md#2.1_先找到连接状态的地址) · [初始化与接链](../source_explanations/include/linux/list.h.md#1.1_初始化与四条接链赋值) | types.h、list.h |
| 摘除为什么不释放宿主 | [删除实现](../source_explanations/include/linux/list.h.md#1.2_摘链与删除后状态) | list.h、poison.h |
| safe 迭代与整批拼接做什么 | [遍历与拼接](../source_explanations/include/linux/list.h.md#1.3_游标和批次转移) | list.h |
| 检查失败是否完全不改状态 | [检查导读](P02_拓扑修改与发布边界导读.md#2.2_检查发生在修改之前) | list.h、lib/list_debug.c、lib/Kconfig.debug |
| 一次执行如何串行，失败是否重试 | [初始化导读](P02_拓扑修改与发布边界导读.md#2.3_一次执行不等于成功初始化) | once.h、once.c、once_lite.h |
| 真实子系统和访问边界 | [旁支位置](P02_拓扑修改与发布边界导读.md#2.4_读子系统时不要只找next) | wait.h、skbuff.h、mm/slab.h、base.h、klist.h、llist.h |

没有为教材例子复制整套 RCU、原子接口或分配器实现；进入那些任务时使用对应权威专题。返回[源码大纲](../大纲.md)。
