---
id: research.kref.impl.rculist_h
title: "rculist.h摘链与旧路径保留"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_rculist.h摘链与旧路径保留

上游相对路径 include/linux/rculist.h；NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20，blob 14dfa6008467e803d57f98cfa0275569f1c6a181。下列中文 Doxygen 和行内注释是仓库补充阅读说明，不属于上游原文。

## 1.1\_摘链后保留旧读者的前向路径

```c
/**
 * list_del_rcu - 仓库补充：绕过节点，但保留旧读者使用的next
 * @entry: 已登记且由调用者串行化修改的链表节点
 *
 * 不等待宽限期、不释放容器对象，也不修改对象的引用计数。
 */
static inline void list_del_rcu(struct list_head *entry)
{
    __list_del_entry(entry); /* 连接前后邻居；不把本节点next改为自链接。 */
    entry->prev = LIST_POISON2; /* prev不再供旧读者反向遍历使用。 */
}
```

`entry` 是对象内的链表节点地址。底层普通摘链辅助连接前后邻居；本包装再把 prev 写成诊断毒值，却保留 next 原值。假设链头为 B→A，读者已经保存 B 地址：删除后链头绕过 B 指向 A，读者仍能通过 B.next 前进到 A。该保证面向匹配的前向 RCU 遍历，不允许任意反向遍历、重复摘链或并发重用节点。

函数本身不取得更新锁，调用者须串行化写入者；仍要保留节点及其容器存储，直到相关旧读者不再访问。不能在随后立即 INIT_LIST_HEAD 改写 next，也不能用 list_empty 判定这个被摘节点的登记状态。应用可以在更新锁下维护独立 linked 字段，但该字段不是此接口提供的状态。

P10 的协议在 S3 调用本函数，锁外才归还表份额；S4 最后归还安排 RCU 回调，S5 才回收。函数不读取 ref，也不会自动完成这些阶段。若还有独立长期引用，是否进入 S4 由 kref 协议决定。

本批函数体去除仓库注释后与固定 Git 对象一致；宿主应用夹具使用该固定函数和既有普通链表辅助函数，验证前后连接、重复移除由应用门控、旧 next 保留。插入/遍历、锁和回收调度为顺序替身，未验证真实内存序或目标运行。

回到[条件取得模块的RCU应用入口](../../../navigation/P03_条件取得与查找窗口导读.md#3.7_从旧节点继续到最终回调)和[总阅读索引](../../../navigation/P01_Linux_6.12_kref源码阅读索引.md#1.2_按问题进入已落地证据)。
