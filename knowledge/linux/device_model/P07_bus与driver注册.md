---
id: knowledge.linux.device_model.bus_driver_registration
title: "bus 与 driver 注册"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [bus, device_driver, registration]
---

# 第7章\_bus与\_driver注册

`bus_register()` 创建总线私有状态、设备/驱动 kset 与 sysfs 表示；`driver_register()` 检查总线并调用 `bus_add_driver()`，把驱动接入总线驱动集合，创建属性后按策略触发 `driver_attach()`。

总线决定 `match`，而 Driver Core 决定何时遍历和怎样串行化绑定。驱动注册成功不等于已经绑定设备；零匹配也是合法稳定状态。

源码：`drivers/base/bus.c::bus_register/bus_add_driver`、`drivers/base/driver.c::driver_register`。

下一篇：[匹配、绑定与 probe 状态机](P08_匹配绑定与probe状态机.md)。
