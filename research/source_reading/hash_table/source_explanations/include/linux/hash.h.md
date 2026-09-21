---
id: research.source_reading.hash_table.hash_implementation
title: "hash.h 的位宽与取高位实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_hash.h的位宽与取高位实现

[模块导读](../../../navigation/P02_键位宽与落桶导读.md)已经建立完整键、表达式类型与桶索引的关系。下面语句取自固定 [include/linux/hash.h](../../../../linux/include/linux/hash.h)，身份见[总索引](../../../navigation/P01_Linux_6.12_哈希计算源码阅读索引.md#1.1_版本和任务边界)。中文 Doxygen 注释为仓库补充；片段依赖内核类型和配置，不是可以单独编译的头文件。

## 1.1\_32位乘法与取高位

u32 为无符号 32 位整数。GOLDEN_RATIO_32 是 0x61C88647；没有体系结构覆盖时，__hash_32 映射到 __hash_32_generic。前者生成固定宽度乘积，hash_32 再取最高的 bits 位；本课程使用 1～32 的合法范围，不把 bits=0 的移位当成一个零桶接口。

```c
/**
 * __hash_32_generic - 仓库阅读说明：混合一个 32 位值。
 * @val: 传入前已转换为 u32 的键。
 *
 * 返回固定宽度乘积，不访问桶或业务对象，也不提供随机性或同步。
 */
static inline u32 __hash_32_generic(u32 val)
{
	return val * GOLDEN_RATIO_32;
}

/**
 * hash_32 - 仓库阅读说明：由混合结果提取高 bits 位。
 * @val: 32 位输入。
 * @bits: 需要的桶索引宽度，本课程限制为 1 到 32。
 *
 * 内部 __hash_32 可被架构覆盖；不能从函数名推断生成指令。
 */
static inline u32 hash_32(u32 val, unsigned int bits)
{
	/* High bits are more random, so use them. */
	return __hash_32(val) >> (32 - bits);
}
```

赋值和返回转换保留无符号 32 位语义。乘法只把变化向更高位传播，因此取高位是算法的一部分，不可换成与桶掩码做 AND 后仍声称分布相同。hash_32 不读取完整对象、不比较重复键，也不会根据已存元素数改变 bits。

可修改性：调整常数、取位方向或 bits 会改变桶映射，插入与查找必须同步，已有成员需要重新登记。若提供架构覆盖，还要核对上游 lib/test_hash.c 的对照规则，不能只验证若干小整数就宣称完全等价。

## 1.2\_64位输入与指针入口

GOLDEN_RATIO_64 是 0x61C8864680B583EB。BITS_PER_LONG 为 64 时计算 64 位乘积，否则先把高半部混合后与低半部折叠。以下返回类型仍是 u32；本课程使用 1～32 位索引。

```c
/**
 * hash_64_generic - 仓库阅读说明：在不同机器字宽上混合完整 u64。
 * @val: 完整 64 位输入。
 * @bits: 输出桶索引宽度。
 *
 * 两个配置分支是不同映射，不承诺相同桶号。
 */
static __always_inline u32 hash_64_generic(u64 val, unsigned int bits)
{
#if BITS_PER_LONG == 64
	/* 64x64-bit multiply is efficient on all 64-bit processors */
	return val * GOLDEN_RATIO_64 >> (64 - bits);
#else
	/* Hash 64 bits using only 32x32-bit multiply. */
	return hash_32((u32)val ^ __hash_32(val >> 32), bits);
#endif
}

/**
 * hash_ptr - 仓库阅读说明：按指针地址身份计算索引。
 * @ptr: 待散列地址，不在这里解引用。
 * @bits: 桶索引宽度。
 *
 * hash_long 按目标内核 unsigned long 的宽度选择入口。
 */
static inline u32 hash_ptr(const void *ptr, unsigned int bits)
{
	return hash_long((unsigned long)ptr, bits);
}

/**
 * hash32_ptr - 仓库阅读说明：把地址折叠为 32 位值。
 * @ptr: 待折叠地址。
 *
 * 没有指定桶数，没有乘法混合，不应当作 hash_ptr 的同义接口。
 */
static inline u32 hash32_ptr(const void *ptr)
{
	unsigned long val = (unsigned long)ptr;

#if BITS_PER_LONG == 64
	val ^= (val >> 32);
#endif
	return (u32)val;
}
```

hash_long 在 32 位配置选择 hash_32，在 64 位配置选择 hash_64；hash_64 又可能由架构覆盖。不能把 Windows 宿主的 unsigned long 宽度直接当作 64 位 Linux 内核的该类型宽度，所以教材模型使用明确的 uint32_t/uint64_t 和选择参数。

hashtable.h 的 hash_min 在 sizeof(val) 不大于 4 时走 hash_32，否则走 hash_long。这里的类型选择意味着完整 u64 在 32 位配置下可能先截断；它与显式 hash_64_generic 的折叠路径不同。这个事实不自动导致错误命中，因为调用者仍应比较完整键，但可能损害分布。

可修改性：换输入类型要核对所有调用点的显式转换，换指针哈希要先说明业务比较的是地址还是内容；更换算法时要重新分桶，并单独验证冲突、缺失键和高半部不同的输入。以上函数不修改表、不获取锁、不延长对象寿命；这些副作用的缺席不能被误写成整个容器无锁安全。

返回[模块导读](../../../navigation/P02_键位宽与落桶导读.md#2.1_从调用表达式追到计算路径)或[总索引](../../../navigation/P01_Linux_6.12_哈希计算源码阅读索引.md#1.2_从问题选择入口)。
