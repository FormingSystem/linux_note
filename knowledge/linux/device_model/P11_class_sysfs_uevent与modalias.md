---
id: knowledge.linux.device_model.class_sysfs_uevent
title: "class、sysfs、uevent 与 modalias"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [class, sysfs, uevent, modalias]
---
# 第11章\_class\_sysfs\_uevent与\_modalias

bus 表达匹配域，class 表达面向功能的用户视图，两者不是父子替代关系。设备注册把 kobject 属性组导出到 sysfs；读取属性时 kernfs/sysfs 根据 attribute 找到对应 `show/store` 回调。uevent 则把对象动作和环境变量发送到用户空间，`MODALIAS` 可触发模块自动加载，但 udev 创建设备节点不等于内核完成 probe。

源码：`drivers/base/class.c`、`core.c::dev_uevent`、`fs/sysfs/file.c`、`lib/kobject_uevent.c`。

下一篇：[devres 资源事务](P12_devres资源事务.md)。
