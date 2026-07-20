---
id: knowledge.linux.device_model.match_bind_probe
title: "匹配、绑定与 probe 状态机"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [probe, binding, matching]
---

# 第8章\_匹配绑定与\_probe状态机

设备先到走 `device_attach()`，驱动先到走 `driver_attach()`；二者最终进入 `driver_match_device()` 与 `driver_probe_device()`。匹配成功只表示候选关系，`really_probe()` 成功后才建立 `dev->driver`、sysfs 双向链接并调用 `driver_bound()`。

```mermaid
sequenceDiagram
    participant C as Driver Core
    participant B as bus->match
    participant D as device
    participant R as driver->probe
    C->>B: 判断候选设备与驱动
    B-->>C: 匹配／不匹配／错误
    C->>D: 设置临时绑定状态并准备依赖
    C->>R: probe(dev)
    alt 成功
        C->>D: 建立bound状态和sysfs链接
    else 失败
        C->>D: 按阶段回滚
    end
```

源码见 `drivers/base/dd.c::device_attach/driver_attach/driver_probe_device/really_probe/driver_bound`。

下一篇：[deferred probe、解绑与 remove](P09_deferred_probe解绑与remove.md)。
