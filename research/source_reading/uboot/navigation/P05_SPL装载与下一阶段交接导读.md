---
id: research.uboot.loader_handoff_navigation
title: "SPL装载与下一阶段交接导读"
kind: source
status: evolving
domains:
  - boot
  - embedded
  - source_reading
---

# 第5章\_SPL装载与下一阶段交接导读

[加载交接正文](../../../../knowledge/system_software/uboot/P05_为什么需要SPL以及它怎样交出下一阶段.md) 已经从早期可用内存解释 SPL/TPL 的设计理由。本模块要帮助读者找到当前镜像的加载责任链：谁提供候选设备，谁写目标内存，失败以后谁决定继续尝试，最后谁消费入口地址。版本沿 [源码总索引](README.md)，固定上游 U-Boot v2022.10、提交 `4debc57a3da6c3f4d3f89a637e99206f4cea0a96`。

## 5.1\_沿同一镜像描述追踪L0到L5

SPL 公共框架没有把所有存储实现混成一个大函数。它用板级候选列表表达“先试谁”，用链接记录表达“当前镜像有什么方法”，用 `spl_image_info` 表达“这个方法产生了什么结果”。控制流的真正连接点是函数指针调用、描述字段和返回值；名称相同不代表所选板启用了同一路径。

下表复用正文 [L0～L5 阶段表](../../../../knowledge/system_software/uboot/P05_为什么需要SPL以及它怎样交出下一阶段.md#5.4_SPL的完整周期是准备环境_装载_检查与交接)。这是 SPL 自身的周期，交接后 proper 才开始第一章的 S0～S5，不能把两份镜像的阶段编号硬拼成同一个初始化函数。

| 阶段与问题 | 状态落点与源码方向 | 唯一实现入口 |
| --- | --- | --- |
| L0～L2：当前环境由谁准备 | 所选 crt0、板级 `board_init_f` 与公共 SPL 初始化标志 | [公共初始化与板级 DDR 的边界](../source_explanations/P05_SPL装载与下一阶段交接实现.md#5.3_L0到L2_公共初始化成功不等于板级DDR已就绪) |
| L2～L3：候选怎么找到动作 | 栈上的 `spl_boot_list` → 链接记录 → 回调 | [加载器登记与首次匹配](../source_explanations/P05_SPL装载与下一阶段交接实现.md#5.1_L2到L3_候选设备与加载器记录怎样相遇) |
| L3～L4：返回值证明什么 | 目标 RAM 和 `spl_image_info` 由加载器写，包装层按配置检查 | [检查、失败与描述生命期](../source_explanations/P05_SPL装载与下一阶段交接实现.md#5.2_L3到L5_成功结果怎样成为交接入口) |
| L5：谁消费入口 | `os` 选择目标协议，`entry_point` 由对应跳转路径使用 | [目标分派与入口调用](../source_explanations/P05_SPL装载与下一阶段交接实现.md#5.2_L3到L5_成功结果怎样成为交接入口) |

先看 `board_init_r` 如何创建并初始化对象，再顺着候选循环读查询、包装和回调，最后回来检查成功后的分派。不要从 `jump_to_image_no_args` 倒推“上游已经替我验证所有地址”，也不要从 `hang` 倒推“此处必然有多核竞争”。记录数组的产生方式沿 [链接列表实现](../source_explanations/P01_ARM32链接布局与启动实现.md#1.5_链接列表怎样从声明变成有边界的数组) 阅读，早期 malloc 和 GD 沿 [运行环境模块](P04_栈与GD及分配器交接导读.md) 阅读。

## 5.2\_平台材料只证明它自己的交接路径

通用 SPL 框架文档描述各阶段的工程职责，Rockchip 文档和板级代码说明部分平台怎样实现这些职责，i.MX6ULL DCD 手册则给出由 ROM 解释配置数据的另一种路径。比较时必须先固定“谁执行、从哪里取指、用哪块 RAM、下一步交给谁”，不能把它们概括成品牌或处理器位宽的差异。

i.MX6ULL 手册身份为 `IMX6ULLRM` Rev.1、11/2017，原件 SHA-256 为 `4dd7f4db09f52a9d5ecf7d807ef6c16c6a4287cb9477d1e770d1a5cb7ff515b5`。§8.3.3 p.259、§8.7.1.1～8.7.2 pp.310～312 的当前必要字段和解释已落在正文，不需要先打开手册才能理解 DCD 反例。原件取得方式由 [ARM 资料索引](../../../../reference/external_resources/arm/README.md) 管理，本目录不另复制整份手册。

以下原件来自同一固定 U-Boot 提交。`include/spl.h` 提供类型/注册宏，`spl_mmc.c` 展示一个实际加载方法，`common/spl/Kconfig` 提供功能选择说明；板配置、镜像配置与平台文档用于限定示例，不能代替完整板级运行验证。

| 上游相对路径及本地原件 | SHA-256 |
| --- | --- |
| [common/spl/spl_mmc.c](../upstream/v2022.10/common/spl/spl_mmc.c) | `dbfe28fbb18d9bee279b154dd2297d404c773d37d1eb7bda0e95b8d7fd743748` |
| [doc/board/rockchip/rockchip.rst](../upstream/v2022.10/doc/board/rockchip/rockchip.rst) | `9434a179c7a3f399a704ae8bcf06f5b2f67281ce23944be3080dddc562e4a1f5` |
| [configs/mx6ull_14x14_evk_defconfig](../upstream/v2022.10/configs/mx6ull_14x14_evk_defconfig) | `24625e9bdd7e03c597f39230f809adf4832bb2af236b6167d064fd59f7560b8f` |
| [board/freescale/mx6ullevk/imximage.cfg](../upstream/v2022.10/board/freescale/mx6ullevk/imximage.cfg) | `eefc0053a603af9a0459193a72c4a3b980df1c87fab209438355f4e0e4e10b85` |
| [common/spl/Kconfig](../upstream/v2022.10/common/spl/Kconfig) | `f86ff9643c13d3c6f97f8d21cb2dea2a7242a79c414053cebecd380512157ddc` |
| [doc/imx/mkimage/imximage.txt](../upstream/v2022.10/doc/imx/mkimage/imximage.txt) | `1760bc986b5e357d1d53ec1b3aa029214c3e7d012c43b98cab3642f523cdee92` |
| [doc/board/nxp/mx6ullevk.rst](../upstream/v2022.10/doc/board/nxp/mx6ullevk.rst) | `747faa2c12c593624f1e483d5cbe9bad9138e29f679e261567ad1e7b5df0ad1d` |
| [configs/seeed_npi_imx6ull_defconfig](../upstream/v2022.10/configs/seeed_npi_imx6ull_defconfig) | `da2aaeea1c249855d000d4bbf21ae66f33ea90de946126a2063c90405c7c06d0` |
| [include/spl.h](../upstream/v2022.10/include/spl.h) | `956b293a5557fc7ca64014a44a7a0cb6b286aafaa71a7804f75be6069143ce2a` |
| [arch/arm/mach-rockchip/spl.c](../upstream/v2022.10/arch/arm/mach-rockchip/spl.c) | `1ba847adf38a13663c15656e4ef8a0b94dedbf349f925c142cb3afe3f11ccb77` |

已有 `common/spl/spl.c`、`doc/develop/spl.rst` 和链接列表文件在前面模块登记。正文 C 模型实际可验证回退、全失败不交接以及失败后目标数组已改变；它没有执行存储驱动、DDR 初始化、ROM DCD 解释或目标机器码，也没有进行完整 U-Boot 构建。后续移植时需补所选板的最终配置、镜像打包身份、内存范围和实际启动结果。
