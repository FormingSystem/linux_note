---
id: knowledge.linux.device_model.device_link_pm
title: "device link 与电源管理"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [device_link, power_management]
---
# 第13章\_device\_link与电源管理

父子层次不能表达所有 supplier/consumer 依赖。`device_link` 单独记录功能依赖，用于 probe 顺序、runtime PM、系统 suspend/resume 和删除约束。链接具有状态与 flags，不能只理解成一条静态指针。

PM 回调选择还会经过 domain、type、class、bus 和 driver 等层次；具体优先级应以 `drivers/base/power/` 源码为准。依赖图保证 supplier 在 consumer 需要时保持可用，并让 suspend/remove 按反向依赖推进。

下一篇：[热插拔与模块生命周期](P14_热插拔与模块生命周期.md)。
