---
id: research.kref.impl.timer_h
title: "timer.h状态观察与旧名包装"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_timer.h状态观察与旧名包装

固定 NXP 提交 dfaf2136deb2af2e60b994421281ba42f1c087e0 的 include/linux/timer.h，blob e67ecd1cbc97d6b92994c15b688cdde5ec3c998f。本页解释“未排队”与“已停止执行”的区别；先读[定时器退出模块](../../../navigation/P05_定时器重启与退出导读.md#5.2_从排队到最终关闭)。下面是上游函数体，中文 Doxygen 是仓库补充；不代表上游注释。

## 1.1\_pending只观察队列成员

```c
/** @brief 仓库补充阅读说明：观察嵌入 hlist 节点是否挂入队列，不检查正在执行的回调。 */
static inline int timer_pending(const struct timer_list * timer)
{
	return !hlist_unhashed_lockless(&timer->entry);
}
```
timer.entry 承载 pending 状态；hlist_unhashed_lockless 观察该节点的挂接标志，本函数反转结果。上游注释要求调用方对其他 timer 操作保持相应串行化；单次观察不承诺后续仍未排队。正在执行的 timer 由 base 的 running_timer 另行记录，因此返回 0 不能作为 free 或最后 put 的依据。

## 1.2\_旧名转到同步删除

```c
/** @brief 仓库补充阅读说明：旧名仅转发同步删除，没有增加禁止重启的保证。 */
static inline int del_timer_sync(struct timer_list *timer)
{
	return timer_delete_sync(timer);
}
```
固定版本的注释明确要求新代码使用 timer_delete_sync。这里解释原教材和已有调用点的旧名称，不为本仓库新增兼容接口。继续读[删除与关闭的差别](../../kernel/time/timer.c.md#1.2_等待执行与关闭重启)，两者共用同步主干，但 shutdown 参数不同。

返回[模块导读](../../../navigation/P05_定时器重启与退出导读.md#5.2_从排队到最终关闭)或[总索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
