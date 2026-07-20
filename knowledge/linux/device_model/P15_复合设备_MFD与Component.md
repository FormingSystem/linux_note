---
id: knowledge.linux.device_model.composite_mfd_component
title: "复合设备、MFD 与 Component"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [mfd, component, composite_device]
---
# 第15章\_复合设备\_MFD与\_Component

MFD 将一个物理芯片拆成多个可独立绑定的子设备，复用父设备资源；component framework 则等待多个独立组件就绪后再 bind 主设备。二者解决相反方向的问题，不能混为“多设备组合”。

```text
MFD：一个父设备 → 创建多个cell/子设备
Component：多个已注册组件 → master匹配完成 → aggregate bind
```

源码分别位于 `drivers/mfd/`、`drivers/base/component.c`；生命周期必须保证子对象/组件引用不越过父对象或 master 的 teardown。

下一篇：[API、调试与选择边界](P16_API调试与选择边界.md)。
