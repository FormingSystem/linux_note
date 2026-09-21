---
id: knowledge.driver_model.fundamentals.kernel_driver_mechanisms.data_strcuture_说明.struct_file_operations
title: "file_operations成员与版本边界"
kind: reference
status: evolving
domains:
  - linux
  - driver
---

# 第1章\_file\_operations成员与版本边界

内容摘抄至GPT。

上面保留原稿的来源批注。本页现为 Linux 6.12.20 固定版本的成员查询入口；连续学习从[文件操作教材](../../../file_operations/大纲.md)开始，依次完成打开、迭代读取和只读映射实验。不要把函数指针表当作所有驱动必须填写的清单。

## 1.1\_表在哪里以及怎样进入

struct file_operations 定义在 include/linux/fs.h，struct file 的 f_op 指向某套操作表。VFS 根据具体路径选择回调，并在此前后执行权限、访问范围、位置和生命周期处理。缺少某个成员时，结果可能是公共实现、其他适配路径或错误；不是统一“什么都不做”，更不是保证自动回退。

本页依据 NXP 官方提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`，身份和读取顺序见[字符设备源码总索引](../../../../../research/source_reading/character_device/navigation/P01_Linux_6.12_字符设备源码阅读索引.md)，本轮核对点见[文件操作导读](../../../../../research/source_reading/character_device/navigation/P03_文件操作与打开寿命导读.md)。不再把未核对的 Linux 6.1 结构体和本地实验 HEAD 混成同一版本。

## 1.2\_按需求查询当前成员

| 成员 | 承担的契约与容易误判的边界 |
| --- | --- |
| owner | 操作表实现所属模块；配合取得/归还表的路径保活代码，不等于自动锁住业务对象 |
| fop_flags | 当前表声明的文件操作能力位；只声明实现确实满足的能力 |
| open | 建立本次打开上下文；fget、dup 不会重新调用它，失败须自行回滚 |
| flush | 关闭路径的可选钩子；共享 file 可多次调用，不可在此无条件释放仍被使用的上下文 |
| release | 最后 file 引用退出后的最终回调；可能晚于最后 fd 关闭 |
| show_fdinfo | 向目标 file 的 /proc fdinfo 追加观察信息，业务字段仍需相应同步 |
| llseek | 处理文件位置；不是每个 whence 都必须支持，边界与溢出必须明确 |
| read / write | 单缓冲字节操作；提交实际进度，区分 EOF、暂空和复制失败 |
| read_iter / write_iter | 以 kiocb 和 iov_iter 表达请求；更新 ki_pos，不直接代替为共享 f_pos；并非自动异步或零拷贝 |
| iopoll | 参与后端完成轮询；不是普通就绪查询 poll |
| iterate_shared | 枚举目录项；目录实现管理 ctx->pos 与并发，不能仅靠名称推断 RCU 安全 |
| poll | 登记等待入口并报告就绪；就绪观察不是数据预留 |
| unlocked_ioctl | 处理控制命令；自行验证 ABI、用户数据及同步 |
| compat_ioctl | 兼容用户态命令入口；是否可复用取决于指针/标量与结构布局 |
| mmap | 建立文件/设备与 VMA 的关系；需证明后备页、范围、权限与寿命 |
| get_unmapped_area | 特殊的虚拟地址范围选择；不能仅 ALIGN 一个原本可用的起点 |
| mmap_capabilities | 仅 !CONFIG_MMU 下的能力声明；采用 NOMMU_MAP_* 语义，不是任意 PROT_* 组合 |
| fsync | 数据及所需元数据的持久化协议；不等于 flush 或 release |
| fasync | 建立/解除异步信号通知登记；真实事件条件仍由驱动维护 |
| lock / flock | 记录锁与整文件锁的特定实现入口；不代替驱动内部锁，未提供时可有公共实现 |
| setlease | 租约协议；此版本参数涉及 struct file_lease，不能照抄旧签名 |
| check_flags | 检查设置的文件状态标志；返回状态，不通过值传递参数修改调用者的 flags |
| splice_read / splice_write | 文件与管道的数据传递；需要匹配页片段及持有协议，不保证没有复制 |
| splice_eof | 为相应 splice 输出路径处理输入结束事件，不是 read 的 EOF 返回值 |
| fallocate | 预分配或范围操作；修改可见大小不等于完成空间保证 |
| copy_file_range | 文件范围复制；具体实现可能复制、克隆或卸载处理 |
| remap_file_range | 克隆/去重的范围回调；不是同名用户系统调用，不保证不支持时自动回退 |
| fadvise | 访问模式/缓存使用提示；不是持久化或性能保证 |
| uring_cmd / uring_cmd_iopoll | io_uring 专用命令与对应轮询；必须明确立即完成与接管后完成的互斥协议 |

成员顺序不决定调用顺序。只需处理小型字符服务时，可以沿用 read/write；实际需要多段请求或相关调用路径时再选择 iter。硬件块驱动还有块层自己的操作接口，文件系统、块设备文件与底层硬件驱动不能仅因为都涉及磁盘就当作同一张表的填写者。

## 1.3\_不能从旧版本清单直接搬来的内容

本版本没有 file_operations.iterate、sendpage 和 mmap_supported_flags；目录使用 iterate_shared，映射同步能力由 fop_flags 中的 FOP_MMAP_SYNC 等当前规则表达。旧稿把 mmap_supported_flags 一会儿写成整数、一会儿写成函数指针，两者都不能作为本基线的声明。

旧稿列出的 no_llseek 也不能照抄到本基线。不可定位语义应结合打开时的能力设置及当前 llseek 分派设计，完整示例分别见[不可定位的 misc](../../../misc/readme.md)与[可定位 iter](../../../file_operations/P02_迭代读取与请求位置.md)。不要用返回任意当前位置的空函数假装拒绝定位。

无 MMU 与 32 位兼容用户态属于不同轴。前者描述有没有虚拟内存管理硬件支持，后者描述内核与用户程序 ABI 的关系。mmap_capabilities 缺省时，nommu 路径还会按对象类型选默认能力，不能断言未实现就必然拒绝；compat_ioctl 的需要也不能由“嵌入式还是桌面”决定。

## 1.4\_原有例子应怎样继续学习

打开/关闭和观察沿[第一章](../../../file_operations/P01_一次打开与最后一次释放.md)完成；普通缓冲与流的两个完整模型分别由[有限窗口](../../../character_device/P10_字符设备驱动模板.md)和[环形流](../../../character_device/P13_流式字符设备与等待通知模板.md)维护。旧稿把 readable 与 f_pos 混成一个模型的程序不再作为模板。

迭代读取沿[第二章](../../../file_operations/P02_迭代读取与请求位置.md)，映射沿[第三章](../../../file_operations/P03_只读映射与后备页寿命.md)。控制命令、目录、调试、通知、文件锁、持久化、范围操作及高效搬运的有效知识由[扩展接口参考](../../../file_operations/P04_扩展接口的契约与选择.md)承接，保留适用条件，撤销伪函数和未经证明的“必用”“零拷贝”承诺。

参考问题：只填 .owner/.open/.release 是否能保证可读？不能，可打开与提供读数据是不同契约。只因为 poll 返回可读，下一次 read 就一定取得数据吗？也不能，在多读者场景下条件可能已改变。先确定服务语义，再选择实现所需的成员。
