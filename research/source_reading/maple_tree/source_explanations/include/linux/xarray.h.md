---
id: research.maple_tree.implementation.xarray_values
title: "include/linux/xarray.h 值标记辅助"
kind: source
status: evolving
domains:
  - linux
  - data_structures
---

# 第1章\_include/linux/xarray.h\_值标记辅助

## 1.1\_Maple使用的辅助边界

Maple 入口中的一串位可能表示对象、整数或内部控制标记。只有先明确当前字段允许的表示，才能解释低位测试；本页核对它借用的 XArray 辅助函数怎样构造和过滤这些值。

上游 include/linux/xarray.h，NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob 0b618ec04115fc3993bf33a7c358632bef170fc9。原始文件由固定 Git 对象核对；本页只展开 Maple 使用的值标记、internal 编码、特殊零 entry 与普通接口入参检查，不扩展为 XArray 完整教程。[模块导读](../../../navigation/P05_字段编码与状态分工.md#5.2_同一数值先按存储位置解读)和[总索引](../../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)组织调用背景。

## 1.2\_整数值使用低位标记并限制有效位宽

同一个 entry 槽有时放对象入口，有时放编码整数。要让使用者区分两者，编码会占用低位，因此可保存的整数位宽比 unsigned long 少一位。这些辅助函数操作的是已约定的表示，不负责证明任意位形对应活着的对象。

```c
/**
 * @brief 仓库补充阅读说明：调用者保证值落在 BITS_PER_LONG 减一位的非负范围。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static inline void *xa_mk_value(unsigned long v)
{
	WARN_ON((long)v < 0);
	return (void *)((v << 1) | 1);
}
```

```c
/**
 * @brief 仓库补充阅读说明：仅还原已按 value 契约编码的值。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static inline unsigned long xa_to_value(const void *entry)
{
	return (unsigned long)entry >> 1;
}
```

```c
/**
 * @brief 仓库补充阅读说明：低两位同时比较，不验证任何实际地址。
 * @note 下列固定语句不验证任意指针的有效性；应先满足所属字段的契约。
 */
static inline bool xa_is_internal(const void *entry)
{
	return ((unsigned long)entry & 3) == 2;
}
```

xa_mk_value 的 WARN_ON 不是拒绝分支，超过合法值域仍继续表达式，移位会丢掉最高位，不能据此存满 unsigned long 值域。xa_to_value 没有替调用者检测 entry 是否确为 value。xa_is_internal 只判断位形，Maple 的小值保留判断还叠加上界 4096；它们也都不是对象生命周期验证器。

## 1.3\_普通接口检查与零entry

树内部还需要表示“特殊零项”和“要求重试”等控制值。它们与普通业务整数不同；普通接口要过滤一部分内部标记，查询还要把特殊零项翻译成 NULL。下面两个常量从头文件不同位置并列，保留各自载荷。

```c
/**
 * @brief 仓库补充阅读说明：构造 internal 载荷；这不是任意业务指针构造器。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
static inline void *xa_mk_internal(unsigned long v)
{
	return (void *)((v << 2) | 2);
}
```

```c
/**
 * @brief 仓库补充阅读说明：零 entry 与重试 entry 是不同的 internal 编码。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
#define XA_ZERO_ENTRY		xa_mk_internal(257)
#define XA_RETRY_ENTRY		xa_mk_internal(256)
```

```c
/**
 * @brief 仓库补充阅读说明：普通查询用此判断把特殊零 entry 映射为 NULL。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
static inline bool xa_is_zero(const void *entry)
{
	return unlikely(entry == XA_ZERO_ENTRY);
}
```

```c
/**
 * @brief 仓库补充阅读说明：普通 store/insert 在 internal 编码且不大于 RETRY 时拒绝。
 * @note 以下保留官方固定版本语句，省略外围未展开的实现。
 */
static inline bool xa_is_advanced(const void *entry)
{
	return xa_is_internal(entry) && (entry <= XA_RETRY_ENTRY);
}
```

XA_RETRY_ENTRY 使用内部值 256，XA_ZERO_ENTRY 使用 257；xa_is_advanced 的比较上界与 Maple mt_is_reserved 的 4096 判断不同。检查通过不证明某个数字是有效对象地址。此前的 xa_is_internal 仅判断标记，本节不能被当作通用 C 指针的构造与验证方法。回到[接口导读](../../../navigation/P07_普通接口与范围契约.md#7.3_结果与证明边界)。

修改标记位或阈值必须同时核对构造、解码、输入拒绝和返回翻译四处：XA_RETRY_ENTRY 与 XA_ZERO_ENTRY 相邻但不同，Maple 的 4096 保留界也不是 xa_is_advanced 的比较界。对象位置见[普通接口关系图](../../../navigation/P07_普通接口与范围契约.md#7.1_从请求到共享索引)；这些辅助落在 [S0/S4 时序](../../../navigation/P07_普通接口与范围契约.md#7.2_沿一次调用划分责任)的入参检查和返回过滤箭头上。编码模型应覆盖阈值两侧、最高合法整数、特殊零项，不能只做单值往返。
