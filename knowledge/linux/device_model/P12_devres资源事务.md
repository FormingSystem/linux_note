---
id: knowledge.linux.device_model.devres
title: "devres 资源事务"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [devres, devm]
---
# 第12章\_devres资源事务

devm 接口把资源释放动作登记到 `device` 的 devres 链，而不是让资源“自动消失”。probe 失败或解绑时，`devres_release_all()` 逆序调用 release，形成与获取顺序相反的回滚。devres group 提供局部事务边界。

> **边界：** devres 生命周期绑定设备绑定关系；跨设备共享、需提前释放或必须严格控制时机的资源，仍需显式协议。

源码见 `drivers/base/devres.c` 与 `drivers/base/base.h`。

下一篇：[device link 与电源管理](P13_device_link与电源管理.md)。
