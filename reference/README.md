---
id: reference.index
title: "参考资料导航"
kind: reference
status: evolving
domains:
  - reference
---

# 第1章\_参考资料导航

## 1.1\_定位与使用边界

`reference/` 保存查阅型材料及其入口，包括标准说明、外部规范和厂商资料索引。这里的内容用于回答“依据在哪里、版本是什么、怎样取得”，不承担知识专题的完整推演；机制解释仍由 `knowledge/` 的权威正文负责，特定版本源码证据由 `research/source_reading/` 负责，平台差异由 `platforms/` 负责。

外部文档的公开下载、允许个人使用和允许本仓库再次分发是三件不同的事。进入具体资料前，应同时核对来源、版本、适用平台、版权与许可边界，不因文件能够下载就默认可以上传到 GitHub。

本页中的 GNU 通用公共许可证（GNU General Public License，GPL）是许可证名称；Arm 通用中断控制器（Generic Interrupt Controller，GIC）是硬件架构名称；Rockchip RK3588 与 NXP i.MX 6UltraLite（i.MX6ULL）是片上系统产品型号。型号不是可按首字母展开的机制缩写，必须回到对应厂商资料确认具体能力。

## 1.2\_当前入口

| 类型 | 入口 | 回答的问题 |
| --- | --- | --- |
| 许可证说明 | [GPL 协议说明（总论）](standards/gpl/GPL协议说明.md) | GPL 家族在常见工程场景中的适用边界与合规检查点是什么 |
| 架构、GIC 与 SoC 外部资料 | [Arm、RK3588 与 i.MX6ULL 外部资料索引](external_resources/arm/README.md#1.1_索引定位与存储边界) | 按对象存放、按学习资料组选取；当前优先 RK3588 的 GIC-600 r1p6 / GICv3.0 |
| GIC 学习组织 | [GICv3 物理中断专题](../platforms/arm/architecture/gic/大纲.md#1.2_因果阅读地图) | 十篇独立概念讲解，术语中英文对照并追溯官方章节；虚拟化与 GICv4 后学 |

## 1.3\_外部资料取得与维护

仓库对新增厂商可移植文档格式（Portable Document Format，PDF）文件默认采用“公开清单 + 本地忽略缓存”策略：Git 只保存来源页面、版本、预期文件名、完整性信息和下载工具；原文件由使用者从权利人入口取得，并保存到 Git 忽略的 `.cache/`。下载界面和共享日志使用仓库相对位置，不固化盘符、用户名、网络共享或其他机器专属地址。

Arm、GIC、RK3588 与 i.MX6ULL 的机器可读清单、版权声明、下载命令和校验规则统一由[外部资料索引](external_resources/arm/README.md#1.5_下载与校验)维护。更新链接时必须同时复核版本、文件长度、页数和安全散列算法 256 位摘要（Secure Hash Algorithm 256-bit，SHA-256），不能让新链接继续使用旧文件的校验值。

## 1.4\_历史资料边界

`external_resources/makefile/` 中已有的 Makefile 文档和转换产物早于当前外部资料清单策略。本次只登记其历史存在，不删除、不重新许可，也不把它们作为新资料进入仓库的范例；其来源和再分发许可需要另行逐份审计。

以后新增参考材料时，应先判断它是仓库可维护的说明、可以合法再分发的原件，还是只应登记来源并由使用者本地取得的第三方文件，再选择存储方式。
