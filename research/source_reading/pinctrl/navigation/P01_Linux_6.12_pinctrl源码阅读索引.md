---
id: research.source_reading.pinctrl.index
title: "Linux 6.12 pinctrl源码阅读索引"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - driver
source_project: linux
source_version: "6.12.20"
source_tree: "NXP linux-imx lf-6.12.20-2.0.0 @ dfaf2136deb2af2e60b994421281ba42f1c087e0"
---

# 第1章\_Linux\_6.12\_pinctrl源码阅读索引

设备树能描述一组引脚配置，驱动又能选择这个状态；读源码要补上的环节是：描述被谁保存、选择被谁执行、错误出现时哪些状态已改变。本索引组织从consumer到提供者的阅读，不要求先背完整pinctrl目录。

## 1.1\_版本边界与阅读路线

**讲解层次：实现层，NXP Linux 6.12.20。** 证据固定在官方 `nxp-imx/linux-imx` 发布标签 `lf-6.12.20-2.0.0` 的提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，统一身份见 [Linux源码基线](../../linux/SOURCE_BASELINE.md)。当前核对使用该提交的官方源码文件；不据此宣称本机或目标板的运行配置已经核实。

通用路径涉及 `CONFIG_PINCTRL`、`CONFIG_OF`、`CONFIG_PINMUX`、`CONFIG_PINCONF`；PM辅助路径涉及 `CONFIG_PM`，debugfs观察涉及 `CONFIG_DEBUG_FS`。i.MX6UL实例限定该SoC的MMIO提供者及对应DTS。关闭功能配置的stub、没有可选状态和没有调试入口是不同情况，阅读时分别处理。

```mermaid
flowchart LR
    Q[描述存在但引脚没有变化] -->|先定位状态与调用者| N[模块导读]
    N -->|P0映射及P2选择| C[通用core实现]
    C -->|setting交给提供者| I[i.MX6UL实现]
    I -->|区分软件记录与硬件读回| D[诊断正文]
```

默认先读唯一的模块导读 [从描述到选择与硬件落点](P02_从描述到选择与硬件落点导读.md#2.1_一条配置如何经过三个持有者)，再按当前问题进入下面的实现标题。知识入门由 [引脚配置正文](../../../../knowledge/driver_model/device_tree/P02_引脚配置怎样连接外设与焊盘.md) 承担，完整消费示例由 [状态切换正文](../../../../knowledge/driver_model/device_tree/P03_引脚状态切换与GPIO占用交接.md) 承担。

| 当前问题 | 上游位置及实现簇 | 唯一实现讲解 |
| --- | --- | --- |
| 句柄、state、setting如何分层 | `drivers/pinctrl/core.h`；`core.c:create_pinctrl/get/put/free` | [状态对象](../source_explanations/P01_状态映射选择与探测交接.md#1.1_句柄状态与设置分别保存什么) |
| DT属性如何形成配置 | `devicetree.c:pinctrl_dt_to_map`；`core.c:add_setting` | [映射与状态](../source_explanations/P01_状态映射选择与探测交接.md#1.2_从连续属性建立映射与状态) |
| 为什么错误后不能假定原状 | `core.c:pinctrl_select_state/commit_state` | [选择与恢复](../source_explanations/P01_状态映射选择与探测交接.md#1.3_选择过程为何不是原子事务) |
| default/init是谁调用 | `drivers/base/pinctrl.c`、`drivers/base/dd.c`、`core.c:pinctrl_init_done` | [probe交接](../source_explanations/P01_状态映射选择与探测交接.md#1.4_设备探测怎样交接init与default) |
| sleep是否自动执行 | `core.c:pinctrl_pm_select_*`；`drivers/tty/serial/imx.c`调用点 | [PM责任](../source_explanations/P01_状态映射选择与探测交接.md#1.5_PM辅助接口只执行被调用的选择) |
| 控制器自身配置何时选择 | `core.c:pinctrl_enable/pinctrl_claim_hogs` | [自身状态消费者](../source_explanations/P01_状态映射选择与探测交接.md#1.6_控制器怎样成为自己的状态消费者) |
| 六cell、pin编号和SION如何解释 | `freescale/pinctrl-imx.c:imx_pinctrl_parse_groups/parse_pin_mmio` | [组与逐pin数据](../source_explanations/P02_iMX6UL配置解析与寄存器落点.md#2.1_六个cell怎样成为组和逐pin配置) |
| 提供者的map包含什么 | `imx_dt_node_to_map/imx_dt_free_map` | [map建立](../source_explanations/P02_iMX6UL配置解析与寄存器落点.md#2.2_映射回调生成设置而没有执行选择) |
| 何时写哪个地址 | `imx_pmx_set/one_pin_mmio`、`imx_pinconf_set_mmio` | [寄存器落点](../source_explanations/P02_iMX6UL配置解析与寄存器落点.md#2.3_选择时分别写MUX输入选择与PAD) |
| GPIO申请是否自动切复用 | `core.c:pinctrl_gpio_request/free`；`pinmux.c:pin_request/free` | [GPIO交界](../source_explanations/P02_iMX6UL配置解析与寄存器落点.md#2.4_GPIO请求怎样经过pin占用而不承诺自动复用) |

## 1.2\_核对结果能证明到哪一层

源码静态核对证明该版本具有这些结构、调用与分支；正文的离线DT练习检验编码、phandle及属性值；debugfs和日志属于运行观察。三者都不能独自证明焊盘电平、板级连线或休眠唤醒成功。

本组重构没有完成目标内核Kbuild、目标板运行或全套DT Schema验证。涉及实际硬件时仍应核对对应内核配置、最终DT、驱动绑定与测量结果。现有 [GPIO源码证据](../../gpiolib/navigation/P01_Linux_6.12_GPIO源码阅读索引.md#1.1_版本身份与阅读路线) 负责gpiolib对象和事件，本专题只展开与pin占用相接的部分。

返回 [专题大纲](../大纲.md)，或按 [观察位置与证明边界](P02_从描述到选择与硬件落点导读.md#2.4_观察位置与不能证明的结论) 选择排查入口。
