---
id: research.source_reading.kconfig.navigation.linux_6_12_configuration
title: "Linux 6.12 配置求值工具阅读索引"
kind: source
status: evolving
domains:
  - linux
  - system
  - source_reading
---

# 第1章\_Linux\_6.12\_配置求值工具阅读索引

[Kconfig 基础语法](../../../../knowledge/system_software/kconfig/基础语法.md) 用一个总线与驱动例子区分规则、用户请求、有效值和构建输入。本页解释固定解析器中谁承担这些工作，并记录正文实验实际验证到哪一层；它不展开上游函数体。

## 1.1\_固定版本和工具边界

**讲解层次：实现层。** 来源为 NXP 官方 `linux-imx`，发布标签 `lf-6.12.20-2.0.0`，Git 提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，内核版本6.12.20。[Linux 源码基线](../../linux/SOURCE_BASELINE.md#1.1_当前来源) 保存来源与工作树访问边界。开发工作树的后续三个提交没有混入本次工具。

配置工具在宿主运行。正文的 [build_conf.sh](../../../../labs/system_software/kconfig/materials/build_conf.sh) 从固定 Git 对象导出工具源文件，再用 Bison、Flex 和宿主 C 编译器生成 conf。实际验证使用 MSYS2 GCC 15.3.0、Bison 3.8.2、Flex 2.6.4；未修改上游工具 C 源码来适配语义，也未切换或改写外部 Linux 工作树。

这项成功证明该固定配置工具能够处理本实验输入，不证明 Linux 内核整体可以在该宿主环境构建，更不证明目标板镜像能够启动。输出的 m 只是配置状态，正文的 C 程序只报告生成宏，没有加载模块。

## 1.2\_从定义输入到输出文件的阅读顺序

先看命令选择了哪种输入和输出模式，再看语言关系怎样保存与求值。否则很容易把生成 autoconf.h 的同步模式，误当成每次读取 Kconfig 都必然执行的动作。

| 当前问题 | 上游相对路径与本地原件 | 负责的关系 |
| --- | --- | --- |
| olddefconfig、syncconfig等模式怎样选择 | [scripts/kconfig/conf.c](../../linux/scripts/kconfig/conf.c) | 解析命令模式，安排已有配置读取、求值与输出 |
| Kconfig文本怎样成为条目 | [lexer.l](../../linux/scripts/kconfig/lexer.l)、[parser.y](../../linux/scripts/kconfig/parser.y) | 识别关键字和属性；if/source仍在语法处理过程中，不能作为跳过坏文本的预处理开关 |
| 菜单和父级依赖怎样关联 | [menu.c](../../linux/scripts/kconfig/menu.c)、[expr.h](../../linux/scripts/kconfig/expr.h) | 保存菜单、符号与表达式关系，组织直接及反向依赖 |
| 用户值与有效值怎样区分 | [symbol.c](../../linux/scripts/kconfig/symbol.c)、[expr.c](../../linux/scripts/kconfig/expr.c) | 结合类型、可见性、默认与依赖求值；表达式使用三态逻辑 |
| 规则中的变量与预处理函数怎样处理 | [preprocess.c](../../linux/scripts/kconfig/preprocess.c)、[preprocess.h](../../linux/scripts/kconfig/preprocess.h) | 处理工具支持的文本变量与函数；不能据此把Kconfig当Makefile执行 |
| 配置文件与生成头怎样读写 | [confdata.c](../../linux/scripts/kconfig/confdata.c) | 读取配置请求，输出有效配置、最小种子及同步派生文件 |
| 工具怎样构建 | [scripts/kconfig/Makefile](../../linux/scripts/kconfig/Makefile)、[scripts/Makefile.host](../../linux/scripts/Makefile.host) | 共同对象、Bison/Flex生成和宿主编译规则；正文脚本只构建conf所需部分 |

完整语法依据保留为 [kconfig-language.rst](../../linux/Documentation/kbuild/kconfig-language.rst)，目标模式见 [kconfig.rst](../../linux/Documentation/kbuild/kconfig.rst)，对象参与构建的规则见 [makefiles.rst](../../linux/Documentation/kbuild/makefiles.rst)。这些文件解释不同层次，不能用一个词“配置”把它们的职责合并。

## 1.3\_本次例子实际观察到的边界

普通七组输入覆盖：空配置的默认值、BUS=m限制DRIVER=y、BUS=n、用户关闭TRACE、用户直接写隐藏CHECKSUM、超范围BUFFER_SIZE、关闭MODULES后的m请求。观察到依赖限值、可覆盖imply、不可直接输入的隐藏项、范围下界及模块状态归一化，结果均来自本固定解析器。

三个负例分别给CHECKSUM添加未满足依赖、在恒不满足条件的if块内写坏语法、在同类if块内source缺失文件。第一例报告select导致的依赖警告，后两例解析失败。它们解释为什么不能只凭配置命令返回成功就忽略警告，也不能把Kconfig的if当作C预处理。

普通驱动配置经syncconfig生成autoconf.h和auto.conf，配套 [show_config.c](../../../../labs/system_software/kconfig/materials/show_config.c) 实际编译运行得到DRIVER=m、BUFFER_SIZE=256以及两项默认值10；savedefconfig得到只含DRIVER=m的种子。种子更换规则版本后仍需重新求值，不能把本次结果当成跨版本保证。

交互菜单的具体显示没有进行原生界面验收，正文主要验证配置语义及文件输出；没有执行真实驱动编译、链接、装载或硬件验证。回到 [完整实验](../../../../knowledge/system_software/kconfig/基础语法.md#1.5_把规则和求值工具放进独立实验目录) 可按同一输入重现工具行为。
