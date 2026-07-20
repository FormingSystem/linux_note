---
id: knowledge.linux.device_model.deferred_remove
title: "Deferred Probe、解绑与 remove"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [deferred_probe, remove, unbind]
---
# 第9章\_Deferred\_Probe\_解绑与\_remove

返回 `-EPROBE_DEFER` 表示当前依赖尚未就绪，不是永久失败。Driver Core 将设备放入 deferred 队列，在新驱动或 supplier 出现等触发点重试。解绑反向执行：阻止新使用、调用 remove、释放 devres、删除绑定链接和模块引用，使设备回到已注册但未绑定状态；设备删除还会继续撤销总线、class、PM、sysfs 和父子关系。

源码：`drivers/base/dd.c` 的 deferred probe、`device_release_driver()` 与 remove 路径。

下一篇：[fwnode、OF 与 Platform 适配](P10_fwnode_OF与Platform适配.md)。
