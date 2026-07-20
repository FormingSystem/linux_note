---
id: knowledge.driver_model.character_device.p01_设备号框架简介.附录a_同步机制中的_interruptible
title: "附录A interruptible 等待语义"
kind: mechanism
status: maintained
domains:
  - linux
  - kernel
  - driver
---

# 第1章\_附录A\_interruptible\_等待语义

## 1.1\_它表达的是等待能否被信号打断

`mutex_lock_interruptible()` 和 `wait_event_interruptible()` 中的 `interruptible`，表示任务在等待资源或条件期间可以因待处理信号提前返回。它不表示临界区内的代码会被随意中断，也不保证调用最终取得锁或满足等待条件。

```c
ret = mutex_lock_interruptible(&dev->lock);
if (ret)
    return ret; /* 没有持锁，必须把错误返回给上层 */

/* 已取得锁，访问受保护状态 */
mutex_unlock(&dev->lock);
```

## 1.2\_驱动为什么常用可中断等待

用户对阻塞式字符设备执行 `read()` 时，通常希望 `Ctrl+C`、进程退出或其他信号能够取消等待。如果使用不可中断等待，设备失联或唤醒条件永远不成立时，任务可能长期停留在 D 状态。

可中断接口的返回值必须检查。等待队列接口通常把信号路径表现为负错误码，驱动应传播该错误，不能继续假装条件已经满足。

## 1.3\_它不能替代并发设计

- 可中断等待不保护共享数据；共享状态仍需 mutex、自旋锁或明确的无锁协议。
- 等待返回成功后仍要在正确锁或内存顺序下重新检查业务条件。
- 不要持有生产者也需要的 mutex 再等待事件，否则生产者无法改变条件。
- 是否允许取消由接口契约决定；极短且必须完成的内部临界区可以使用普通 `mutex_lock()`。

更完整的条件等待、唤醒和返回值规则见[等待队列](../../linux/waiting_notification/P01_等待队列.md)，字符设备中的锁与等待边界见[文件操作并发与生命周期](P04_文件操作并发与生命周期.md)。
