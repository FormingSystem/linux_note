---
id: knowledge.linux.device_model.kobject_sysfs
title: "kobject、kset 与 sysfs 对象树"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [kobject, kset, sysfs]
---

# 第5章\_kobject\_kset与\_sysfs对象树

`kobject` 同时承载名称、父子关系、kset 归属、sysfs/kernfs 节点和引用计数；`kobj_type` 提供 release 与属性操作。`kset` 是带自身 kobject 的对象集合，并可提供 uevent 过滤与环境变量。

```text
kobject_init → 仅初始化并持有初始引用
kobject_add  → 命名、接入父对象并在 sysfs 可见
kobject_del  → 从对象树删除可见关系
kobject_put  → 最后引用触发 ktype->release
```

删除 sysfs 节点不等于包含对象已经释放。源码见 `lib/kobject.c`、`include/linux/kobject.h`、`fs/sysfs/` 与 kernfs。

下一篇：[device 注册与生命周期](P06_device注册与生命周期.md)。
