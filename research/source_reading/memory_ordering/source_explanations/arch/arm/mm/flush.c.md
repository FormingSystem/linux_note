---
id: research.source_reading.memory_ordering.arm_heavy_mb_impl
title: "flush.c 平台屏障补充源码实现"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
---

# 第1章\_平台屏障补充源码实现

## 1.1\_DSB之后还有什么

本页只展开上游arch/arm/mm/flush.c中的arm_heavy_mb；版本为NXP官方linux-imx提交dfaf2136deb2af2e60b994421281ba42f1c087e0，Linux 6.12.20。原文件其他缓存维护函数不在本页覆盖范围。阅读位置见[模块A3](../../../../navigation/P07_ARM屏障与配置边界导读.md#7.3_沿一条调用走完配置与运行路径)及[总索引](../../../../navigation/P01_Linux_6.12_LKMM_源码与模型导读.md#1.3.3_ARMv7_映射)。调用前的DSB在[ARM头唯一实现](../include/asm/barrier.h.md#1.2_基础与DMA接口的配置选择)中，不在本函数重复执行。

源码可对照[官方固定提交原文件](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/arch/arm/mm/flush.c)，本次只读核对及文件身份记录见[基线](../../../../../linux/SOURCE_BASELINE.md#1.124_ARM屏障映射与平台补充路径)。CONFIG_ARM_HEAVY_MB是构建开关，决定是否编入本段；CONFIG_OUTER_CACHE_SYNC决定是否编入外部缓存同步分支。回调仍要在运行时通过非空检查才能被调用。末尾EXPORT_SYMBOL把函数导出供内核模块使用，它不是函数体中又执行了一次屏障。

```c
#ifdef CONFIG_ARM_HEAVY_MB
/* 平台可安装的全局函数指针；静态存储期缺省为空。 */
void (*soc_mb)(void);

/**
 * @brief 仓库补充阅读说明：按固定顺序执行已存在的平台补充钩子。
 * @details 不安装钩子、不获取业务对象引用、不产生设备完成通知。
 */
void arm_heavy_mb(void)
{
#ifdef CONFIG_OUTER_CACHE_SYNC
    if (outer_cache.sync)
        outer_cache.sync(); /* 先执行已安装的外部缓存同步回调。 */
#endif
    if (soc_mb)
        soc_mb(); /* 再执行已安装的SoC屏障回调。 */
}
EXPORT_SYMBOL(arm_heavy_mb);
#endif
```

实现原理是先检查再同步调用。软件状态落在两个函数指针地址：外部缓存操作表成员outer_cache.sync和全局soc_mb。CONFIG_OUTER_CACHE_SYNC控制前一种路径是否参与编译，非空检查再决定是否调用；后一种只有非空检查。两个条件并不互斥，都成立时按外部缓存、SoC的次序执行；都不成立时函数直接返回。空回调不表示宏前面的DSB也被取消。

函数没有锁、等待队列、IPI或回调完成标志。它同步调用已安装的回调，然后返回；这段代码不能单独证明回调指针允许运行时并发替换，也不能证明任意目标一定安装了它们。固定树的soc_mb赋值可在mach-mstar与mach-omap2的平台初始化中找到，这些是不同平台路径，不能挪来当i.MX目标的实际执行证据。

## 1.2\_检查调用次序能证明多少

用返回前记录一个字符的普通C函数分别替代两个钩子，组合外部缓存配置开启/关闭、两个指针为空/非空，便能检查无调用、只调用一个、先外部缓存再SoC的控制流。该检查验证的是本函数的条件和顺序，不执行真正缓存控制器或互连同步。

即使两个替身都成功返回，也没有读取设备的完成寄存器。把这一步解释成“设备工作已经完成”，会越过本函数没有提供的业务证据；把回调为空解释成“屏障无效”，又会漏掉调用者已经执行的架构指令。沿A2→A3分别核对，才能避免这两个相反的误判。
