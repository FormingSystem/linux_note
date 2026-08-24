---
id: research.source_reading.sequence_counters.linux_6_12_seqlock_header_implementation
title: "Linux 6.12 seqlock.h 读写与 latch 源码实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
topics: [synchronization, seqcount, seqlock, implementation]
source_project: linux
source_version: "6.12.20"
---

# 第1章\_Linux\_6.12\_seqlock\_h读写与latch源码实现

## 1.1\_实现讲解边界

本章是 `include/linux/seqlock.h` 的唯一实现讲解，裁剪普通读写、关联锁属性与 latch 三组符号。中文 Doxygen 由仓库补充；省略的 KCSAN、Lockdep 和 raw 变体应回到上游完整文件核对。

## 1.2\_源码符号覆盖账本

| 标题 | 上游行段 | 原理 |
| --- | --- | --- |
| [普通读侧](#1.3_普通读侧begin与retry) | 266～388 | 稳定 start、读后屏障与比较 |
| [普通写侧](#1.4_普通写侧begin与end) | 449～503 | 奇偶开关窗与顺序 |
| [关联锁](#1.5_关联锁属性与RT补偿) | 89～230 | writer 断言、preemptible 与 RT 推进 |
| [latch](#1.6_latch重定向与双副本更新) | 576～779 | 最低位选副本与两次写入 |

## 1.3\_普通读侧begin与retry

```c
/**
 * @brief 读取一个稳定的偶数代际作为读区起点。
 * @param s seqcount 或关联锁变体。
 * @return 必须交给 read_seqcount_retry() 的 start。
 */
#define read_seqcount_begin(s) ({         \
    seqcount_lockdep_reader_access(s);    \
    raw_read_seqcount_begin(s);           \
})

/**
 * @brief 在数据复制后验证 sequence 是否仍等于 start。
 * @return 非零表示必须丢弃快照并重试。
 */
static inline int do_read_seqcount_retry(const seqcount_t *s,
                                         unsigned start)
{
    smp_rmb(); /* 先完成受保护数据读取，再进行末尾 sequence 比较。 */
    return READ_ONCE(s->sequence) != start;
}
```

完整 begin 会在奇数 sequence 上等待/重读，并使用属性访问适配关联锁类型。retry 的比较只有与 begin 和数据访问配对才有意义。

## 1.4\_普通写侧begin与end

```c
/** @brief 在 writer 已串行且满足抢占约束时打开奇数写窗口。 */
static inline void do_write_seqcount_begin(seqcount_t *s)
{
    /* 省略：Lockdep/KCSAN 嵌套原子区。 */
    s->sequence++;
    smp_wmb(); /* 奇数状态先于后续字段写被观察。 */
}

/** @brief 在全部字段写完成后发布新的偶数稳定代际。 */
static inline void do_write_seqcount_end(seqcount_t *s)
{
    smp_wmb(); /* 字段写先于偶数关闭窗口。 */
    s->sequence++;
    /* 省略：KCSAN 区域结束。 */
}
```

代码展示该提交的实现，不应被概括成所有版本固定“两个 wmb”。调用者应依赖配对 API 契约。

## 1.5\_关联锁属性与RT补偿

```c
/**
 * @brief 关联锁变体读取 sequence；RT 下帮助可抢占 writer 推进。
 * @note 展开自 SEQCOUNT_LOCKNAME() 的概念裁剪。
 */
static unsigned seqprop_lock_sequence(const seqcount_lock_t *s)
{
    unsigned seq = smp_load_acquire(&s->seqcount.sequence);

    if (IS_ENABLED(CONFIG_PREEMPT_RT) &&
        lock_type_is_preemptible && unlikely(seq & 1)) {
        associated_lock(s->lock);
        associated_unlock(s->lock);
        seq = smp_load_acquire(&s->seqcount.sequence);
    }
    return seq;
}
```

真实宏还生成 `lockdep_assert_held(s->lock)` 属性。关联指针使契约可验证/可推进，不自动包围 writer 业务代码取得锁。

## 1.6\_latch重定向与双副本更新

```c
/** @brief 翻转最低位，把 reader 重定向到另一份稳定副本。 */
static __always_inline void raw_write_seqcount_latch(seqcount_latch_t *s)
{
    smp_wmb();
    s->seqcount.sequence++;
    smp_wmb();
}

/** @brief reader 取得完整 sequence，最低位用于选择 data[2]。 */
static __always_inline unsigned
raw_read_seqcount_latch(const seqcount_latch_t *s)
{
    return READ_ONCE(s->seqcount.sequence);
}

static __always_inline int
raw_read_seqcount_latch_retry(const seqcount_latch_t *s, unsigned start)
{
    smp_rmb();
    return READ_ONCE(s->seqcount.sequence) != start;
}
```

writer 完整协议必须执行：翻转到副本 1 → 更新 data[0] → 再翻转到副本 0 → 更新 data[1]。只调用一次翻转会让两副本长期不一致。

## 1.7\_复核问题

- read begin 的 Lockdep 访问与真实 sequence 读取分别有什么作用？
- writer begin/end 的奇偶变化为什么必须夹住全部字段写？
- RT 补偿为什么只发生在奇数且关联锁可抢占时？
- latch 若包含动态指针，为什么仍需 RCU 管生命期？

模块导读：[Linux 6.12 seqcount 与 seqlock 模块源码概念导读](../navigation/P02_Linux_6.12_seqcount与seqlock模块源码概念导读.md#2.1_模块问题与职责分支)。

总索引：[Linux 6.12 序列计数器源码总阅读索引](../navigation/P01_Linux_6.12_序列计数器源码总阅读索引.md#1.5_建议阅读顺序)。
