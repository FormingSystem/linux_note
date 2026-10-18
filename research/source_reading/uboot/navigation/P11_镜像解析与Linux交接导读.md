---
id: research.uboot.navigation.image_handoff
title: "镜像解析与Linux交接导读"
kind: source
status: evolving
domains:
  - boot
  - source_reading
---

# 第11章\_镜像解析与Linux交接导读

本模块承接 [正文的 M0～M5](../../../../knowledge/system_software/uboot/P11_内存镜像如何变成Linux入口现场.md)，版本固定在 [总索引](README.md) 的 U-Boot v2022.10、提交 `4debc57a3da6c3f4d3f89a637e99206f4cea0a96`。阅读任务是把输入内存、描述对象、目标范围和最终处理器入口连接起来；它不是某块板已启动成功的报告。

## 11.1\_顺着状态和字节分别走一遍

先从 `cmd/bootm.c` 看请求哪些阶段，再到 `boot/bootm.c` 看它们怎样按返回值推进。描述状态保存在全局 `images`，字节仍在源/目标内存，不能把描述字段更新当成实际复制。ARM64 裸 Image 的 `cmd/booti.c` 自行处理前段，再进入公共准备和交接路径。

| 阶段 | 协作对象与状态落点 | 唯一实现入口 |
| --- | --- | --- |
| M0～M1 | 参数/默认地址→内存容器→全局 `images` 的范围与格式 | [阶段位和镜像发现](../source_explanations/P11_镜像范围与Linux交接实现.md#11.1_M1与M2_阶段位怎样驱动实际操作) |
| M2 | `os.image_start/image_len/load`→目标字节；裸 Image 的头部决定搬移地址 | [Image 目标计算](../source_explanations/P11_镜像范围与Linux交接实现.md#11.2_booti怎样计算ARM64Image的目标地址) |
| M3 | `ft_addr` 指向的 DTB→范围登记、安置和属性修订 | [参数内存准备](../source_explanations/P11_镜像范围与Linux交接实现.md#11.3_M3_设备树位置和内容怎样一起改变) |
| M4～M5 | 清理钩子和架构路径→最终寄存器参数与 `ep`；返回时转入错误处理 | [Linux 回调与意外返回](../source_explanations/P11_镜像范围与Linux交接实现.md#11.4_M4与M5_准备跳转和返回不是同一个结果) |

`include/image.h` 的字段和阶段位表与 `include/bootm.h` 的返回码要随调用处阅读。不要先背全部宏再寻找含义，也不要把 `images.state` 当成成功完成位图。次级核共享释放单元已在 [ARM 入口实现](../source_explanations/P06_ARM入口与异常向量实现.md) 解释，本单元只补它如何进入最终 DTB。

## 11.2\_固定证据与实际验证范围

下表保存本模块核对原件的摘要；旧有 `start.S`、`transition.S` 和宏定义从前面 ARM 入口模块追溯。原件未经教学改写，实现页中的中文注释另行标明。

| 上游相对路径及本地原件 | SHA-256 |
| --- | --- |
| [boot/bootm.c](../upstream/v2022.10/boot/bootm.c) | `869c7e31fb1e63a7dbec3489bfbd1d3232cef0d3b9f85cf91d42777738670bf3` |
| [boot/bootm_os.c](../upstream/v2022.10/boot/bootm_os.c) | `9a6bd846b496da828e7048207783b1204a2a9a822fe76bb828a40c7e9f644370` |
| [cmd/bootm.c](../upstream/v2022.10/cmd/bootm.c) | `37660e09267a96de073debe0798636ddf0c9d6f3d72dc34819473c067e3b03ca` |
| [cmd/booti.c](../upstream/v2022.10/cmd/booti.c) | `dc430e68f3d362354ca5c1c0cf2e65dfba77c831fcccf68ab310f27a71fdcbfb` |
| [arch/arm/lib/bootm.c](../upstream/v2022.10/arch/arm/lib/bootm.c) | `e1ad9d328d530ad6204b5dae2b12e5fb33ed535aeb7a72b3177224e767fd8a6e` |
| [arch/arm/lib/bootm-fdt.c](../upstream/v2022.10/arch/arm/lib/bootm-fdt.c) | `a0b88860ba3048749ed394ba83b62a3b0ede0b6b44c3a4d2a288684f7c557ff2` |
| [include/bootm.h](../upstream/v2022.10/include/bootm.h) | `04a43cf32daa3831fb6e5305a2d775f80f5debf82b7e075138bf09814993d27e` |
| [doc/usage/cmd/booti.rst](../upstream/v2022.10/doc/usage/cmd/booti.rst) | `7c43631d04ef7ed5e15c1ceb723aa4c91c438fca71c12912d85fc1048bea5e27` |
| [boot/image-board.c](../upstream/v2022.10/boot/image-board.c) | `bb21dde5be803bb6b2796413322e60a1183057e3904b96d2ce427364dc382567` |
| [boot/image-fdt.c](../upstream/v2022.10/boot/image-fdt.c) | `37b950743c2ae7f855605ef1fbc738472f707c71ccc8d62f9709fdb66e52a025` |
| [arch/arm/lib/image.c](../upstream/v2022.10/arch/arm/lib/image.c) | `2b4e0c7efa9600ae462d55b315855c51d6341cbc518277a783cab0adac0aae20` |
| [arch/arm/cpu/armv8/spin_table.c](../upstream/v2022.10/arch/arm/cpu/armv8/spin_table.c) | `0b6bd38855ff8ebaa11e3668fa5ba601862641f9f0bb0cee67761c68b2e21503` |
| [include/image.h](../upstream/v2022.10/include/image.h) | `43ed60650ed4fcf89cd1c7106c20771b49904cc0c8c5e7e3d8fa0e201733189b` |
| [common/fdt_support.c](../upstream/v2022.10/common/fdt_support.c) | `dae359400210ccebed009d230077ba2852568714b61e46c57b5b1f24f07bf48a` |
| [arch/arm/cpu/armv8/cpu.c](../upstream/v2022.10/arch/arm/cpu/armv8/cpu.c) | `3a1e2fb48a7ef7e1db9725879c99b0e213823bb6ef56191b2783d3a949f5ca58` |

本次直接提取 `booti_setup`、`do_bootm_linux`、`boot_selected_os`，用 C11 宿主桩执行 16 个分支用例并核对头部字段偏移，全部通过。覆盖错误标识、普通/强制/保留位置及旧头部目标选择，PREP/GO/fake 分派，以及普通内核返回与允许返回的特例。编译开启 `-Wall -Wextra -Werror`，只对原件接口未使用的参数关闭告警。这些验证只针对合成 Image 头的地址选择、Linux 回调分派以及交接函数的返回处理；没有执行实际解压、设备树修改、缓存维护、特权级切换和目标板跳转。正文中的源/目标重叠例子用于推理地址范围，不表示上游已经为任意恶意输入提供完整内存安全检查。
