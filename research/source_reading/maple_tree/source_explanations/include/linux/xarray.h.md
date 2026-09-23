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

上游 include/linux/xarray.h，NXP 官方固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，blob 0b618ec04115fc3993bf33a7c358632bef170fc9。原始文件由固定 Git 对象核对；本页只展开 Maple 说明中使用的三个 value/internal 辅助，不扩展为 XArray 完整教程。[模块导读](../../../navigation/P05_字段编码与状态分工.md#5.2_同一数值先按存储位置解读)和[总索引](../../../navigation/P01_Linux_6.12_Maple范围源码阅读索引.md#1.2_按读者问题进入证据)组织调用背景。

## 1.2\_整数值使用低位标记并限制有效位宽

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
