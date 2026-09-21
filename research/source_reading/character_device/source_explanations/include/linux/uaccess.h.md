---
id: research.character_device.source.uaccess
title: "Linux 6.12 普通用户复制的失败边界"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
---

# 第1章\_Linux6.12普通用户复制的失败边界

上游相对位置为 `include/linux/uaccess.h`。版本见[总阅读索引](../../../navigation/P01_Linux_6.12_字符设备源码阅读索引.md#1.1_版本和阅读边界)，原文见[源码副本](../../../../linux/include/linux/uaccess.h)。本页只讲普通用户复制的返回与目标副作用；具体架构如何处理异常和指令循环不在本页范围。

## 1.1\_普通复制的短复制处理

用户访问层约定返回未复制量。头文件的接口说明还明确区分普通 `copy_from_user` 与双下划线等变体的尾部处理，不能因为名字相近就交换使用。下面从 `_inline_copy_from_user` 裁剪 **完成架构复制之后** 的分支；此前的地址检查、故障注入、对象与检测器路径省略，不代表整个函数只有这些语句。

```c
/**
 * @brief 仓库阅读说明：S2 复制后的返回与失败尾部处理片段。
 * @param to 内核目标缓冲区。
 * @param from 用户源缓冲区。
 * @param n 请求长度；res 表示未完成量。
 * @note 以下为函数体裁剪，中文注释不是上游原注释。
 */
instrument_copy_from_user_before(to, from, n);
res = raw_copy_from_user(to, from, n);
instrument_copy_from_user_after(to, from, n, res);
if (likely(!res))
    return 0;
fail:
/* 剩余部分不是“保留原数据”：此分支会把目标尾部清零。 */
memset(to + (n - res), 0, res);
return res;
```

若 `n=4`、`res=2`，已经复制的前缀占两个字节，清零从 `to+2` 开始覆盖剩下两个字节。返回 2 表示尚有两个用户字节未取得，不表示内核目标只变化了两个字节。上层若把目标设为旧记录所在的共享缓存，再返回错误，不能据此宣称旧记录未变。

普通包装入口还会先检查复制大小，并根据 `INLINE_COPY_FROM_USER` 等选择内联或外部实现。前置检查可能在进入实际复制之前就拒绝请求，因此也不能把上面的裁剪反向概括成“任何失败必定清零全部目标”。对驱动足够稳妥的保证是：失败不提供内容回滚；需要事务替换就使用私有临时区，并检查完整复制是否成功。

用户访问帮助函数可能触发缺页和休眠。这个事实决定其调用上下文与锁的选择，但它不意味着“所有锁都不能覆盖用户复制”。具体锁范围和数据生命期由调用者安排，见[模块导读的交接关系](../../../navigation/P02_Linux_6.12_有限缓冲区IO模块导读.md#2.3_复制层与调用者的交接)与[知识正文](../../../../../../knowledge/driver_model/character_device/P05_文件操作契约与数据路径.md#5.5_一次加锁不等于整次读取都是快照)。
