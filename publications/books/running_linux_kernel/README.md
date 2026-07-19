---
id: knowledge.kernel_subsystems.fundamentals.running_linux_kernel.readme
title: "奔跑吧 Linux 内核"
kind: publication
status: evolving
domains:
  - publication
  - linux
---

# 第1章\_奔跑吧Linux内核

> 阅读本目录内容前，请先查看独立的[版权与免责声明](copyright_and_disclaimer.md)。

1. 参考书籍：《奔跑吧 Linux 内核（入门篇·第 2 版）》；
2. 作者：笨叔、陈悦；
3. ISBN：978-7-115-55560-1。

## 1.1\_版权与性质声明

《奔跑吧 Linux 内核（入门篇·第 2 版）》不是本仓库作者创作或出版的作品。原书的著作权及其他相关权利归原作者、出版方和相应权利人所有。

本目录保存的是仓库作者阅读该书过程中形成的个人读书笔记，包括个人理解、知识梳理、实验记录、扩展说明和基于公开技术资料整理的学习大纲。这里不提供原书电子版、扫描件或原文复制品，也不是原书的官方配套资料。目录位于 `publications/books` 仅表示它按参考书籍组织，不表示本仓库拥有或发布该书。

笔记中涉及的原书名称、章节主题、技术概念及必要引用，其权利仍归各自权利人所有。本仓库根目录许可证只适用于仓库作者有权授权的原创笔记、分析和整理内容，不对原书内容、第三方图片、第三方源码或其他引用材料重新授权。

这些笔记仅用于个人学习、技术研究和知识整理，不能替代原书。建议读者通过正规渠道购买或借阅原书。如权利人认为本目录中的具体内容存在不当使用，可以通过仓库 Issue 或电子邮箱 [lizhaojun97@qq.com](mailto:lizhaojun97@qq.com) 联系处理。联系时请说明权利人身份、涉及的文件路径、具体内容和处理请求，以便及时核查。

## 1.2\_阅读说明

本组笔记以原书主题作为学习线索，并结合 Linux 内核源码、平台实践和 AI 辅助整理进行扩展。内容可能包含阶段性理解或错误，应结合原书、Linux 内核官方文档和对应版本源码交叉核对。

本笔记仓库的代码和环境为：

* 恩智浦代码仓库：https://github.com/nxp-imx/linux-imx.git；
* 分支为：imx_5.4.70_2.3.0；
* 操作系统：ubuntu22.04;

## 1.3\_大纲目录

## 1.4\_Linux\_Kernel\_数据结构学习大纲

## 1.5\_环境准备与基础
### 1.5.1\_开发环境搭建
- 获取内核源码（5.4/6.1版本）：参考 [imx_v8_config_kernel编译说明.md](../../../platforms/arm/nxp/imx6ull/porting/imx_v8_config_kernel编译说明.md)
- 配置编译环境：[imx6ull-移植u-boot-2025.04_and_kernel-6.1.md](../../../platforms/arm/nxp/imx6ull/porting/imx6ull-移植u-boot-2025.04_and_kernel-6.1.md)
- QEMU调试环境：问过AI后，感觉没有必要装这个调试环境；
- 内核模块开发基础

### 1.5.2\_必备C语言知识
- GNU C扩展语法
- container_of宏原理
- 内存对齐与填充
- 内联汇编基础

## 1.6\_核心容器数据结构
### 1.6.1\_链表(list.h)
- `struct list_head`双向循环链表
- 常用操作宏：
  - `LIST_HEAD`, `INIT_LIST_HEAD`
  - `list_add`, `list_del`
  - `list_for_each`, `list_for_each_entry`
  - `list_for_each_entry_safe`
- 应用场景：进程链表、设备链表

### 1.6.2\_哈希表(hashtable.h)
- `struct hlist_head`, `struct hlist_node`
- 哈希表初始化：`DEFINE_HASHTABLE`, `hash_init`
- 哈希函数：`hash_min`, `hash_ptr`
- 遍历：`hash_for_each`, `hash_for_each_safe`
- 应用场景：PID哈希表、dentry缓存

### 1.6.3\_红黑树(rbtree.h)
- `struct rb_root`, `struct rb_node`
- 基本操作：
  - `rb_insert_color`
  - `rb_erase`
  - `rb_first`, `rb_last`
  - `rb_next`, `rb_prev`
- 应用场景：虚拟内存区域、定时器

### 1.6.4\_XArray/基数树
- 5.4：基数树（radix tree）
- 6.1：XArray（新接口）
- 核心API：
  - `xa_store`, `xa_load`, `xa_erase`
  - `xa_for_each`遍历
- 应用场景：页缓存、文件映射

## 1.7\_内存管理数据结构
### 1.7.1\_页管理
- `struct page`页描述符
  - 标志位（flags）
  - 引用计数（_refcount）
  - 映射信息（mapping, index）
- `struct page`的union结构

### 1.7.2\_内存区域
- `struct vm_area_struct`
  - 虚拟地址范围（vm_start, vm_end）
  - 操作函数（vm_ops）
  - 红黑树节点（vm_rb）
- `struct mm_struct`
  - 进程地址空间描述
  - 内存区域链表和红黑树

### 1.7.3\_内存分配器
- `struct slab`和`struct kmem_cache`
- Buddy System相关结构
- `struct zone`内存区域

## 1.8\_进程管理数据结构
### 1.8.1\_进程描述符
- `struct task_struct`核心字段：
  - 状态（state）
  - 标识（pid, tgid）
  - 调度（prio, static_prio）
  - 内存（mm, active_mm）
  - 文件系统（fs, files）
  - 信号（signal, sighand）

### 1.8.2\_进程关系
- 链表关系：`tasks`, `children`, `sibling`
- 命名空间：`struct pid`
- 进程组和会话

### 1.8.3\_调度器结构
- `struct sched_entity`
- `struct rq`运行队列
- CFS红黑树
- 实时调度相关结构

## 1.9\_文件系统数据结构
### 1.9.1\_文件对象
- `struct file`
  - `f_path`（路径）
  - `f_inode`（inode指针）
  - `f_op`（文件操作）
  - `private_data`（私有数据）

### 1.9.2\_inode和dentry
- `struct inode`
  - `i_mode`（模式）
  - `i_op`（inode操作）
  - `i_fop`（文件操作）
  - `i_sb`（超级块）
- `struct dentry`
  - 目录项缓存
  - 哈希表组织

### 1.9.3\_超级块
- `struct super_block`
- `struct super_operations`
- 文件系统挂载信息

## 1.10\_网络子系统数据结构
### 1.10.1\_套接字缓冲区
- `struct sk_buff`
  - 数据区指针：`head`, `data`, `tail`, `end`
  - 协议头：`network_header`, `transport_header`
  - 链表：`next`, `prev`
- sk_buff分配与释放

### 1.10.2\_网络设备
- `struct net_device`
  - 设备名称和配置
  - 操作函数（net_device_ops）
  - 统计信息
- 网络设备队列

### 1.10.3\_套接字
- `struct socket`
- `struct sock`
- 协议相关结构

## 1.11\_并发与同步数据结构
### 1.11.1\_锁机制
- `spinlock_t`自旋锁
- `struct mutex`互斥锁
- `rwlock_t`读写锁
- `seqlock_t`顺序锁

### 1.11.2\_RCU机制
- `struct rcu_head`
- RCU读端和写端
- 同步机制

### 1.11.3\_每CPU数据
- `DEFINE_PER_CPU`宏
- `get_cpu_var`, `put_cpu_var`
- 每CPU变量的应用场景

## 1.12\_内核对象与设备模型
### 1.12.1\_kobject/ktype/kset
- `struct kobject`内核对象基础
- `struct kobj_type`对象类型
- `struct kset`对象集合
- sysfs集成

### 1.12.2\_设备与驱动
- `struct device`
- `struct device_driver`
- `struct bus_type`
- 设备树相关结构

## 1.13\_时间管理数据结构
### 1.13.1\_定时器
- `struct timer_list`
- 高精度定时器（hrtimer）
- 时间轮（timer wheel）

### 1.13.2\_时间表示
- `struct timespec64`
- `ktime_t`
- jiffies与HZ

## 1.14\_调试与追踪数据结构
### 1.14.1\_调试接口
- `struct proc_dir_entry`
- sysfs属性（`struct attribute`）
- debugfs文件系统

### 1.14.2\_追踪机制
- tracepoint相关结构
- ftrace数据结构
- BPF映射结构

## 1.15\_学习项目与实践
### 1.15.1\_基础实践
1. 编写内核模块，操作链表和哈希表
2. 实现简单的红黑树操作
3. 创建proc/sysfs接口

### 1.15.2\_中级实践
1. 分析进程创建过程的数据结构
2. 追踪文件打开过程的数据结构流
3. 网络数据包路径分析

### 1.15.3\_高级实践
1. 实现自定义内核数据结构
2. 修改调度器数据结构
3. 内存管理数据结构分析

## 1.16\_学习工具与资源
### 1.16.1\_代码分析工具
- cscope/ctags代码索引
- LXR/Elixir在线源码查看
- QEMU+GDB内核调试

### 1.16.2\_调试工具
- printk与动态调试
- ftrace性能分析
- BPF/eBPF动态追踪
- SystemTap内核探测

### 1.16.3\_文档资源
- 内核源码Documentation目录
- LWN.net内核文章
- Kernel Newbies网站
- 内核邮件列表归档

## 1.17\_学习计划建议
### 1.17.1\_时间安排
- 基础阶段（2-3周）：环境+链表+哈希表
- 核心阶段（4-6周）：内存+进程+文件系统
- 进阶阶段（3-4周）：网络+并发+设备模型
- 实践阶段（持续）：项目开发与源码分析

### 1.17.2\_学习方法
1. 先理解结构定义和关系
2. 通过简单示例理解基本操作
3. 在实际内核代码中查找应用
4. 编写测试代码验证理解
5. 阅读相关内核补丁了解演变

这个大纲从基础到高级系统性地覆盖了Linux内核主要数据结构。建议按照顺序学习，并结合实际内核代码进行实践。
