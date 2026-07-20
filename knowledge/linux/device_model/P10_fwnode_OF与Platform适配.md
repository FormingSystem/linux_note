---
id: knowledge.linux.device_model.fwnode_platform
title: "fwnode、OF 与 Platform 适配"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [fwnode, device_tree, platform]
---
# 第10章\_fwnode\_OF与\_Platform适配

固件节点描述硬件，`struct device` 表示内核运行对象，两者不能混为一谈。OF/platform 路径把 `device_node` 转为 `platform_device`，保存 `dev.of_node/dev.fwnode`，解析资源，再由 `platform_bus_type.match` 按 OF、ACPI、ID table 或名称等规则匹配。

```text
DT节点 → of_platform_populate → platform_device → device_add
驱动of_match_table → platform_match → of_driver_match_device
```

通用属性访问应优先理解 `fwnode_handle` 抽象；具体证据见 `drivers/of/platform.c`、`drivers/base/platform.c`、`drivers/base/property.c`。

下一篇：[class、sysfs、uevent 与 modalias](P11_class_sysfs_uevent与modalias.md)。
