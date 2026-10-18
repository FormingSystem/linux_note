---
id: research.source_reading.printk.navigation.linux_6_12_logging
title: "Linux 6.12 日志路径源码阅读索引"
kind: source
status: evolving
domains:
  - linux
  - kernel
  - source_reading
---

# 第1章\_Linux\_6.12\_日志路径源码阅读索引

已经理解 [Linux 内核日志](../../../../knowledge/kernel_subsystems/tracing/logging/Linux_内核日志.md) 的 L0～L3 后，本页回答这些角色在固定源码中落在哪里。它组织模块职责、状态落点和阅读顺序，不展开上游函数体，也不把正文改成源码目录清单。

## 1.1\_先固定源码身份和运行边界

**讲解层次：实现层。** 来源为 NXP 官方 `linux-imx`，标签 `lf-6.12.20-2.0.0`，提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，Linux 6.12.20。官方远端与配置规则见 [Linux 源码阅读基线](../../linux/SOURCE_BASELINE.md#1.1_当前来源)。

本次可访问的开发工作树 HEAD 比固定提交多三个提交，相关内容均从固定 Git 对象读取。开发配置启用 PRINTK、MODULES、PROC_FS、DEBUG_FS、TINY_RCU 和 PREEMPT_NONE，未启用 SMP、DYNAMIC_DEBUG 和 DYNAMIC_DEBUG_CORE；不能把下面动态调试路径说成该配置已经实际运行过。

正文配套模块在现有 ARM 生成头环境进行普通、定义 DEBUG、强制选择动态调试三个分支的 Clang 语法检查，使用到的366个内核头文件与固定提交无内容差异。后两种是编译分支检查，没有改变外部工作树配置，也不是完整 Kbuild、链接、装载或目标机验证。

## 1.2\_L0到L3分别由谁推进

从“看不到一条调试日志”进入，先判断是否生成记录，再追消费者。否则在 L0 已经被抑制的情况下研究串口驱动，方向就错了。

| 阶段与问题 | 固定源码入口 | 状态与协作关系 |
| --- | --- | --- |
| L0：宏是否发出日志 | [include/linux/printk.h](../../linux/include/linux/printk.h)、[include/linux/dev_printk.h](../../linux/include/linux/dev_printk.h) | `pr_debug` 与 `dev_dbg` 选择动态、DEBUG 或不输出分支；`pr_fmt` 与 `dev_fmt` 分开作用 |
| L0：动态开关写到哪里 | [include/linux/dynamic_debug.h](../../linux/include/linux/dynamic_debug.h)、[lib/dynamic_debug.c](../../linux/lib/dynamic_debug.c) | 调用点描述符保存身份与标志；控制查询选择描述符，更新后续调用读取的打印条件 |
| L0：限流额度归谁 | [include/linux/ratelimit_types.h](../../linux/include/linux/ratelimit_types.h)、[include/linux/ratelimit.h](../../linux/include/linux/ratelimit.h)、[lib/ratelimit.c](../../linux/lib/ratelimit.c) | `ratelimit_state` 保存锁、窗口、打印与抑制计数；普通限流宏在调用点定义局部静态实例 |
| L0：一次性状态归谁 | [include/linux/once_lite.h](../../linux/include/linux/once_lite.h) 与上述打印宏 | `pr_*_once` 使用调用点状态；设备日志另有调用点布尔状态，均不自动变成每设备状态 |
| L1：记录保存在哪里 | [kernel/printk/printk.c](../../linux/kernel/printk/printk.c)、[kernel/printk/printk_ringbuffer.h](../../linux/kernel/printk/printk_ringbuffer.h) | `vprintk_store` 组织正文与 `printk_info` 元信息，预留并提交环形缓冲区记录；不是往用户文件直接写入 |
| L2：用户读取进度在哪里 | 同一 `printk.c` 的 `devkmsg_read`、`struct devkmsg_user` | 每次打开的私有状态保存读取序号及输出缓冲区；没有新消息时可等待 `log_wait`，记录丢失时有错误处理 |
| L2/L3：控制台何时消费 | 同一 `printk.c` 的 `vprintk_emit`、控制台刷新与 `suppress_message_printing` 路径 | 记录保存后，根据上下文与控制台能力选择输出安排；普通过滤比较级别与门槛，不改变用户读取路径 |

表中的限流状态由尝试输出的调用路径更新，动态标志由控制命令更新，读取序号由消费者更新，不能把这些都画成一个“日志开关”。一条消息可能已经经过 L1，而某个控制台尚未输出；也可能在 L0 就没有进入记录函数。

## 1.3\_按一次触发与一次读取核对源码

先查看两个头文件中的普通与动态打印分支，再读动态控制查询如何匹配模块和格式串。对正文的 `trigger_set()`，分别追踪 raw DEBUG、`pr_debug` 和直接十六进制转储；它们即使最后带相同级别，也没有相同的 L0 条件。

沿已经允许输出的调用进入 `vprintk_emit()` 和 `vprintk_store()`，核对递归保护、默认级别、记录预留、正文填充与提交。没有换行的记录允许后续延续，带换行的记录可最终提交；正文实验使用完整行。长消息截断、预留失败与 panic 抑制说明“调用了打印接口”并非无条件留存保证。

随后分两条路径读：`devkmsg_read()` 按该打开实例的序号取记录；控制台路径按自己的状态取记录并决定是否输出。用户读取发现旧序号已经丢失时会报告相应情况，不会凭空恢复被覆盖的文本。控制台刷新安排受传统控制台、新控制台支持和上下文影响，不能把某一个分支替代全部 Linux 6.12 配置。

最后回看 `___ratelimit()`：间隔为零、局部状态锁竞争、窗口推进、额度判断和抑制计数是不同出口。它不保存每条被抑制消息的参数，也不保证未来补发。`printk_ratelimit_state` 是另外的共享实例，不会因为修改了它就改变每个宏展开点的局部实例。

## 1.4\_接口核对与实验材料

格式化和命令语法有固定版本的原始说明，可以与正文表格逐项核对：

- [printk-basics.rst](../../linux/Documentation/core-api/printk-basics.rst)：级别、门槛、前缀与调用接口。
- [printk-formats.rst](../../linux/Documentation/core-api/printk-formats.rst)：错误指针、普通指针、地址与位图参数。
- [dynamic-debug-howto.rst](../../linux/Documentation/admin-guide/dynamic-debug-howto.rst)：选择条件、`p/f/s/l/m/t` 标志、模块加载参数与 CORE 选择性接入。
- [lib/hexdump.c](../../linux/lib/hexdump.c)：缓冲区分组与逐行转储；动态控制的宏选择另在 `printk.h`。
- [drivers/base/core.c](../../linux/drivers/base/core.c)、[include/linux/device.h](../../linux/include/linux/device.h)、[include/linux/device/class.h](../../linux/include/linux/device/class.h)：设备日志上下文、`dev_err_probe`、根设备与类接口。
- [include/linux/moduleparam.h](../../linux/include/linux/moduleparam.h)、[kernel/params.c](../../linux/kernel/params.c)：参数声明与 sysfs 读写回调关系。

实验源码是 [loglab.c](../../../../labs/kernel/logging/materials/loglab.c)，构建入口是 [Makefile](../../../../labs/kernel/logging/materials/Makefile)。完整的创建、观察、动态开关、恢复门槛和卸载步骤均在 [正文实验](../../../../knowledge/kernel_subsystems/tracing/logging/Linux_内核日志.md#1.3_先做一个可以反复触发的日志入口)。这是教学程序，不是上游源码摘录；没有创建额外的真实硬件设备或字符设备读写接口。
