---
id: knowledge.linux.device_model.device_lifecycle
title: "device 注册与生命周期"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [device, lifetime, registration]
---

# 第6章\_device注册与生命周期

`device_register()` 是 `device_initialize()` 后调用 `device_add()` 的便捷组合；失败后的引用处理必须遵守接口契约，不能混用 `device_del()` 与 `put_device()`。

```text
未初始化
→ device_initialize：kobject、锁、链表、PM等本地状态
→ device_add：父子关系、总线/class集合、sysfs、uevent、匹配
→ registered
→ device_del：撤销系统可见关系
→ put_device：最后引用调用 dev->release/type/class release
```

`get_device()/put_device()` 保护对象内存生命期，不表示设备仍注册或仍绑定驱动。源码见 `drivers/base/core.c::device_add/device_del/device_register/device_unregister`。

下一篇：[bus 与 driver 注册](P07_bus与driver注册.md)。
