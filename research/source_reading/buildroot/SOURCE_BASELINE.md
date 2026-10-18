---
id: research.source_reading.buildroot.baseline
title: "Buildroot固定资料与验证边界"
kind: source
status: evolving
domains: [linux, buildroot, source_reading]
---

# 第1章\_Buildroot固定资料与验证边界

[Buildroot入门正文](../../../knowledge/system_software/buildroot/P01_从手工拼系统到可重复的构建输入.md) 用系统交付问题建立构建模型；本页负责固定实际核对的发布包、原件与运行边界。这里的文件不是独立可构建的Buildroot副本，也不应作为产品源码发行包使用。原件按上游路径保留在2025.02子目录，解释正文在知识专题中维护。

## 1.1\_解释版本与归档身份

本轮采用2025.02发布包，目的是让规则、配置与例子可回到同一历史版本核对。选择历史版本作为解释基线，不意味着推荐在新设备上直接使用该初始补丁状态；维护状态从 [官方发布页](https://buildroot.org/download.html) 取得。

| 项目 | 固定值或依据 |
| --- | --- |
| 官方项目仓库 | https://gitlab.com/buildroot.org/buildroot |
| 版本标识 | 2025.02 |
| 发布身份 | [2025.02公告](https://lists.buildroot.org/pipermail/buildroot/2025-March/775550.html)，2025-03-18；公告列出同名Git标签与归档 |
| 取得入口 | [官方tar.xz归档](https://buildroot.org/downloads/buildroot-2025.02.tar.xz) |
| 取得日期 | 2026-10-18 |
| 本次归档SHA-256 | `889249bdc79b0a32f2911c3f7c0049c60eb90b9c9ad75aab367022fb3e216e41` |
| 文件记录 | [source_manifest.json](2025.02/source_manifest.json)，各原件SHA-256 |
| 原件许可入口 | [COPYING](2025.02/COPYING)，保留上游文本；其他文件内声明照原件 |

摘要用于核对本次所用字节，不把自行计算摘要当作发布者签名验证。本次从官方HTTPS入口取得归档，未执行PGP签名验证，也未把Git标签解析成已核实的提交号。引用源码时使用“版本、归档摘要、上游相对路径”这一身份组合，不虚构提交哈希。

## 1.2\_每份原件承担哪个证据任务

| 上游相对位置及原件 | 本轮核对的事实 |
| --- | --- |
| [README](2025.02/README) | 普通用户构建、配置和镜像入口 |
| [Makefile](2025.02/Makefile) | 输出目录、配置目标、savedefconfig保存目的、构建与镜像阶段入口 |
| [Config.in](2025.02/Config.in) | 全局构建选项及可复现相关选择，不当作逐字节保证 |
| [docs/manual/introduction.adoc](2025.02/docs/manual/introduction.adoc) | 工具链、内核、引导程序、rootfs可按需要组合 |
| [docs/manual/quickstart.adoc](2025.02/docs/manual/quickstart.adoc) | 主要输出目录，staging链接和target不能直接部署的限制 |
| [docs/manual/prerequisite.adoc](2025.02/docs/manual/prerequisite.adoc) | 本版本Linux宿主必需/可选工具与界面依赖 |
| [docs/manual/getting.adoc](2025.02/docs/manual/getting.adoc) | 取得发布包与版本命名的背景 |
| [docs/manual/customize-configuration.adoc](2025.02/docs/manual/customize-configuration.adoc) | Buildroot精简配置与组件配置要分别保存 |
| [docs/manual/rebuilding-packages.adoc](2025.02/docs/manual/rebuilding-packages.adoc) | 增量记录不自动处理全部变更，取消包不自动卸载 |
| [docs/manual/how-buildroot-works.adoc](2025.02/docs/manual/how-buildroot-works.adoc) | Make和包基础设施组织，后续章节的调用入口 |
| [docs/manual/configure.adoc](2025.02/docs/manual/configure.adoc) | 内外工具链选择、外部工具链限制及目标配置 |
| [docs/manual/release-engineering.adoc](2025.02/docs/manual/release-engineering.adoc) | 当时随包说明；不替代当前维护公告 |
| [configs/qemu_aarch64_virt_defconfig](2025.02/configs/qemu_aarch64_virt_defconfig) | 入门实验的目标、内核6.12.9、ext4和宿主QEMU选择 |
| [board/qemu/aarch64-virt/linux.config](2025.02/board/qemu/aarch64-virt/linux.config) | 该实验配套内核配置 |
| [board/qemu/aarch64-virt/readme.txt](2025.02/board/qemu/aarch64-virt/readme.txt) | virt机器、内核和磁盘参数的上游启动说明 |
| [board/qemu/post-image.sh](2025.02/board/qemu/post-image.sh) | 从配置及板说明生成start-qemu.sh |
| [board/qemu/start-qemu.sh.in](2025.02/board/qemu/start-qemu.sh.in) | serial-only参数、host工具PATH与模拟器执行方式 |

工具链章节进一步核对下面这些配方。它们记录构建依赖和安装行为，不把源码中的设置扩大成所有工具链必须遵循的唯一实现。

| 上游相对位置及原件 | 本轮核对的事实 |
| --- | --- |
| [package/gcc/gcc.mk](2025.02/package/gcc/gcc.mk) | 公共宿主依赖、sysroot设置及短工具前缀别名 |
| [package/gcc/gcc-initial/gcc-initial.mk](2025.02/package/gcc/gcc-initial/gcc-initial.mk) | 初始GCC构建范围和libgcc |
| [package/gcc/gcc-final/gcc-final.mk](2025.02/package/gcc/gcc-final/gcc-final.mk) | 完整GCC依赖目标libc，宿主编译环境与目标运行库安装分工 |
| [package/gcc/Config.in.host](2025.02/package/gcc/Config.in.host)、[package/binutils/Config.in.host](2025.02/package/binutils/Config.in.host) | 固定版本的工具版本及语言选择条件 |
| [package/glibc/glibc.mk](2025.02/package/glibc/glibc.mk)、[package/musl/musl.mk](2025.02/package/musl/musl.mk)、[package/uclibc/uclibc.mk](2025.02/package/uclibc/uclibc.mk) | 各libc的初始编译器与内核头文件依赖，开发/运行安装边界 |
| [package/linux-headers/linux-headers.mk](2025.02/package/linux-headers/linux-headers.mk)、[Config.in.host](2025.02/package/linux-headers/Config.in.host) | UAPI导出、安装及来源选择 |
| [toolchain/toolchain-buildroot/toolchain-buildroot.mk](2025.02/toolchain/toolchain-buildroot/toolchain-buildroot.mk)、[Config.in](2025.02/toolchain/toolchain-buildroot/Config.in) | 内部工具链与libc配置关系 |
| [toolchain/Config.in](2025.02/toolchain/Config.in)、[package/Makefile.in](2025.02/package/Makefile.in) | 工具链公共能力和宿主/目标编译变量 |

外部工具链接入另核对以下原件。知识正文解释生产、声明、校验与导入的边界，不把现成工具链的能力声明当作重编配置。

| 上游相对位置及原件 | 本轮核对的事实 |
| --- | --- |
| [toolchain/toolchain-external/Config.in](2025.02/toolchain/toolchain-external/Config.in)、[Custom选项](2025.02/toolchain/toolchain-external/toolchain-external-custom/Config.in.options) | 接入方式、路径、无末尾连字符的前缀与真实能力声明 |
| [pkg-toolchain-external.mk](2025.02/toolchain/toolchain-external/pkg-toolchain-external.mk)、[toolchain-external.mk](2025.02/toolchain/toolchain-external/toolchain-external.mk)、[Custom配方](2025.02/toolchain/toolchain-external/toolchain-external-custom/toolchain-external-custom.mk) | 实际检查入口、sysroot与目标库导入、包装入口及虚拟包 |
| [toolchain/helpers.mk](2025.02/toolchain/helpers.mk)、[check-kernel-headers.sh](2025.02/support/scripts/check-kernel-headers.sh) | 版本、libc、语言及头文件系列检查范围 |
| [toolchain/toolchain-wrapper.c](2025.02/toolchain/toolchain-wrapper.c) | 参数包装和BR2_DEBUG_WRAPPER观察入口 |
| [package/pkg-generic.mk](2025.02/package/pkg-generic.mk) | show-info输出包元数据，不是能力校验执行入口 |
| [Bootlin配置](2025.02/toolchain/toolchain-external/toolchain-external-bootlin/Config.in) | 此版本内一个具体预定义工具链来源及宿主限制 |
| [docs/manual/using-buildroot-toolchain.adoc](2025.02/docs/manual/using-buildroot-toolchain.adoc)、[relocate-sdk.sh](2025.02/support/misc/relocate-sdk.sh) | SDK内容、移位脚本与应用开发用途 |
| [system/Config.in](2025.02/system/Config.in)、[fs/tar/Config.in](2025.02/fs/tar/Config.in) | 纯工具链生产配置关闭init、shell和rootfs tar的选项 |

不同来源说法冲突时先保留时间和具体字段。比如2025.02随包release-engineering仍用较早的LTS周期举例，2025.02发布公告讨论延长维护，当前下载页再给出系列维护终止时间；它们不能拼成一个没有日期的永久承诺。教程介绍发布模型，产品选择则核对当时有效公告。

## 1.3\_静态核对与实际运行分别登记

2026-10-18已下载并解出所列原件，计算归档与各文件摘要，阅读入门配置、打包脚本、Make目标及对应手册。知识正文中的构建和QEMU命令依据这些原件编写。

本次环境未运行Linux Buildroot配置、工具链构建、rootfs制作或QEMU启动，没有构建时长、产物大小或实际登录日志。正文预期与实测严格区分；当前Windows工具环境不作为Linux构建通过证据。此QEMU示例也不覆盖任何真实板卡的烧写或启动验证。

仓库中的NXP Linux 6.12.20源码研究有自己的固定提交和配置。本实验所选Linux 6.12.9仅属于Buildroot随包模拟机配置，二者不共用源码身份或运行结论。
