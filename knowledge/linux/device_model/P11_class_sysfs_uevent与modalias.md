---
id: knowledge.linux.device_model.class_sysfs_uevent
title: "class、sysfs、uevent 与 modalias"
kind: mechanism
status: evolving
domains: [linux, kernel]
topics: [class, sysfs, uevent, modalias]
---

# 第11章\_class\_sysfs\_uevent与modalias

前面的设备登记、匹配和固件适配解释了内核怎样找到设备并交给驱动。应用还需要按功能找到对象、读取属性，有时还需要字符设备节点。这几种用户入口彼此关联，却不由同一个函数保证同时可用。

bus 组织匹配与管理域，class 提供功能分类；sysfs 把对象属性接到 show/store，字符节点则通过号码进入文件操作。uevent 把对象动作及适用的环境信息传给用户空间，MODALIAS 为模块查找提供提示，不证明模块已加载或 probe 已成功。devtmpfs 的节点请求由内核直接提交，不能理解成它监听 uevent 后才建节点。

## 11.1\_沿两个完整实例展开

进入[class 与 sysfs 学习路线](class_sysfs/大纲.md#1.1_从分类观察到可控数据入口)：先创建两个没有设备节点的分类实例，再加入 enabled 属性控制字符读取，随后解释发布、权限、并发与撤销。系统属性参考把这些认识接到真实子系统、设备树、PM 与其他文件系统。

这个分支学习结束后，应能解释“目录有而节点无”“节点有而读取被拒绝”“删除目录还要等待回调”各发生在哪一层。成员查询保留在[class 参考](../../driver_model/fundamentals/kernel_driver_mechanisms/data_strcuture_说明/struct_class.md)，固定源码从[驱动入口索引](../../../research/source_reading/driver_entries/navigation/P01_Linux_6.12_驱动入口源码阅读索引.md)进入。

上一篇：[固件与 Platform 适配](P10_fwnode_OF与Platform适配.md)。完成分支后继续[devres 资源事务](P12_devres资源事务.md)，学习失败与解绑时资源怎样归还。
