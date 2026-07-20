---
id: knowledge.linux.device_model.api_debug_boundaries
title: "设备模型 API、调试与选择边界"
kind: reference
status: evolving
domains: [linux, kernel]
topics: [device_model, api, debugging]
---
# 第16章\_设备模型\_API调试与选择边界

API 应按状态转换选择：初始化与注册、取得与释放引用、绑定与解绑、属性导出、资源托管、依赖链接。不得用 `device_create()` 代替理解 `device_initialize/device_add`，也不得把 `device_unregister()` 后的指针立即当成无引用内存。

调试按层定位：

- `/sys/bus/*/{devices,drivers}`：总线集合与绑定。
- `/sys/class`：功能分类视图。
- `.../driver`、`.../subsystem` 链接：关系是否建立。
- `uevent` 与 modalias：用户空间通知和模块加载。
- deferred probe/debugfs、动态调试、tracepoint：probe 为什么未发生。

代码审查必须分别核对对象引用、注册状态、绑定状态、devres 回滚、supplier 依赖和模块退出顺序。

上一篇：[复合设备、MFD 与 Component](P15_复合设备_MFD与Component.md)。
