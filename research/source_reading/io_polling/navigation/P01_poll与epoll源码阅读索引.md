---
id: research.source_reading.io_polling.navigation.index
title: "Linux 6.12 poll与epoll源码阅读索引"
kind: source
status: evolving
domains:
  - linux
  - kernel
---

# 第1章\_poll与epoll源码阅读索引

本页组织版本化源码入口，不复制上游函数体。通用推理从[poll与epoll阅读路线](../../../../knowledge/linux/io_model/blocking_io/大纲.md)进入；本页沿正文的P0～P4和E0～E5阶段寻找状态所在地址与函数协作，不能把回调名称直接当成应用已经消费数据的证明。

## 1.1\_固定提交与文件位置

官方来源为NXP的linux-imx，来源分支lf-6.12.y，发布标签lf-6.12.20-2.0.0，固定提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20。身份与本轮边界见[源码基线](../../linux/SOURCE_BASELINE.md#1.132_poll登记与触发边界)。本地实验提交不作为证据。

| 上游相对文件 | Git blob | 阅读职责 |
| --- | --- | --- |
| include/linux/poll.h | fc641b50f1298eaf463b8c42251ee0df363a7eb3 | poll_table、登记回调、poll_wait与vfs_poll契约 |
| fs/select.c | 834f438296e2ba97c87636e3c5d12d65c7b94030 | 普通poll临时登记、触发标志、扫描与清理 |
| fs/eventpoll.c | 1a06e462b6efba8824456cffebad040720c4226a | 持久兴趣、回调、候选扫描、模式处理与撤销 |

## 1.2\_按阶段阅读

| 正文阶段 | 入口顺序 | 阅读时要回答的问题 |
| --- | --- | --- |
| P0～P1 建立本次等待 | poll_initwait → __pollwait；结合poll_wait | poll_table._qproc怎样决定登记方式？poll_table_entry怎样保存文件、队列与回调？ |
| P2 等待 | do_poll → poll_schedule_timeout | 如何先检查triggered再决定睡眠，并在再次扫描时处理触发状态？ |
| P3 通知与重查 | pollwake → __pollwake，再回到do_poll | 谁设置poll_wqueues.triggered，谁使等待任务可运行，谁重新查询文件？ |
| P4 清理 | poll_freewait → free_poll_entry | 本次登记怎样从目标队列移除，持有的文件引用何时放弃？ |
| E0 持久登记 | ep_insert → ep_item_poll → ep_ptable_queue_proc | epitem保存兴趣，eppoll_entry保存通知钩子；已有就绪怎样进入rdllist？ |
| E1 等待任务 | ep_poll | eventpoll.wq与目标设备队列为什么不同？最终检查与任务登记怎样受同一把锁约束？ |
| E2 形成候选 | ep_poll_callback | 兴趣过滤、已在链表的判断与ovflist分流分别避免什么问题？ |
| E3 复查与交付 | ep_start_scan → ep_send_events → ep_item_poll → ep_done_scan | txlist承接本轮扫描，期间回调进入哪里？过期候选如何被丢弃？ |
| E4 再次推进 | ep_send_events、ep_modify | LT重新排入、ET不自动重排、ONESHOT禁用与MOD重启各改什么状态？ |
| E5 撤销 | ep_remove相关路径、eventpoll_release_file | 目标文件最终释放与单个fd关闭为何不能画等号？ |

先读poll.h理解登记函数是可以替换的，再对照select.c和eventpoll.c，才容易看清“同一驱动.poll为不同等待方式服务”。不要先背红黑树与链表，再试图猜出为什么需要两套集合。

## 1.3\_两条容易读反的调用关系

目标状态变化以后，生产路径唤醒目标等待队列，队列中的ep_poll_callback把epitem纳入候选并唤醒eventpoll.wq中的等待者。随后交付路径通过ep_item_poll重新查询目标文件.poll。因此不能把路径画成“唤醒应用 → 应用调用驱动.poll → 驱动才触发ep_poll_callback”；那会倒置候选形成与交付复查的先后。

同样，poll_wait不是睡眠函数。它按poll_table的登记函数关联等待队列；_qproc为空时不增加等待项，调用者仍可以查询当前状态。普通poll唤醒后的再次扫描以及epoll的交付复查都需要区分“查状态”和“再次登记”。

## 1.4\_验证边界与继续阅读

上述证据是固定源码的只读核对，未修改或运行外部内核。宿主C模型只验证就绪观察不保留数据；[Linux管道实验](../../../../knowledge/linux/io_model/blocking_io/P02_epoll持久登记与交付.md#2.6_完整Linux实验_留下一个字节)当前没有Linux运行日志。两者均不证明所有驱动的.poll实现、并发关闭、信号或所有epoll标志都已验证。

本页只承担模块概念与阅读索引，未新建空的source_explanations目录；后续若增加特定宏体/函数体讲解，须按上游路径落地唯一实现标题并从对应阶段精确链接。回到[P03负载选择](../../../../knowledge/linux/io_model/blocking_io/P03_poll与epoll的选择.md)，可以把版本中的树、候选、回调与复制成本映射到实际应用，而不推导出恒定O(1)的口号。
