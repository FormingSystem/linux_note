---
id: research.character_device.source.open
title: "Linux6.12文件打开与关闭入口实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
---

# 第1章\_Linux6.12文件打开与关闭入口实现

上游相对位置为 `fs/open.c`，原文见[源码副本](../../../linux/fs/open.c)。版本采用[总索引](../../navigation/P01_Linux_6.12_字符设备源码阅读索引.md#1.1_版本和阅读边界)中的官方固定提交。先读[打开寿命模块导读](../../navigation/P03_文件操作与打开寿命导读.md#3.1_打开对象由谁持有)，下面 S3/S4 指该导读的文件寿命周期。中文 Doxygen 和行内注释是仓库补充，代码保留固定版本语句。

## 1.1\_filp\_close归还文件引用

```c
/**
 * @brief 仓库阅读说明：执行关闭钩子并归还调用者持有的文件引用。
 * @param filp 仍有效的文件对象；调用者将这份引用交给本函数归还。
 * @param id 关闭所属的锁所有者标识，不是用户 fd 数字。
 * @return filp_flush 的结果，不是最终 release 的返回值。
 * @note S3 的关闭与 S4 的最终清理不是同一事件。
 */
int filp_close(struct file *filp, fl_owner_t id)
{
    int retval;

    retval = filp_flush(filp, id); /* S3：可选 flush 及关闭关联的清理。 */
    fput(filp);                   /* 归还引用；不保证此处同步执行 S4。 */

    return retval;
}
```

filp_flush 在确认计数没有损坏后调用可选 f_op->flush；非 FMODE_PATH 路径还处理目录通知及 POSIX 锁。这里保存返回值以后仍执行 fput，不能根据 flush 报错认定文件引用尚未归还，更不能让驱动 flush 直接释放仍可能被 dup 使用的上下文。

fput 的实现位于 [fs/file_table.c](../../../linux/fs/file_table.c)：非最后引用只减少计数，最后引用才进入最终清理安排。最终 __fput 在适用时解除异步通知，再调用 release，然后 fops_put。普通 fput 可将这部分工作留给任务工作或延迟工作；源码阅读不能把一个函数名当成“所有资源已经同步释放”的证明。

## 1.2\_close系统调用的同步清理

固定版本 close 系统调用的关键部分如下，裁去的是后续将内部重启错误换成 EINTR 的分支和最终返回，不是改变引用处理顺序：

SYSCALL_DEFINE1 是定义一个参数系统调用入口的内核宏，此处参数就是 fd。EBADF 是没有对应有效描述符的错误码。宏展开的入口包装不是驱动的 file_operations.close 成员；后者根本不在当前操作表中。

```c
/**
 * @brief 仓库阅读说明：从 fd 表撤出入口，再同步归还其文件引用。
 * @param fd 当前调用者的描述符数字。
 * @return 无对应文件时为 -EBADF，其余结果来自关闭钩子及错误转换。
 * @note 以下为系统调用体的裁剪片段，不是独立可编译替代实现。
 */
SYSCALL_DEFINE1(close, unsigned int, fd)
{
    int retval;
    struct file *file;

    file = file_close_fd(fd);     /* 先撤销数字入口，取得待归还的引用。 */
    if (!file)
        return -EBADF;

    retval = filp_flush(file, current->files);
    __fput_sync(file);            /* 若是最后引用，直接执行 __fput。 */

    /* 裁剪：原文接着转换不可重启的错误，再 return retval。 */
}
```

__fput_sync 减少 f_count，仅当计数降到零才同步 __fput。若 VMA 或另一个描述符还持有 file，这次 close 依然不能触发 release。“同步”限定的是最后引用这一分支，不会把其他持有者强制清掉。

该路径没有先调用 filp_close，也没有等待通用 fput 的任务工作。教材可以说明一般文件引用归还存在延迟，但在分析本版本一次实际 close 时必须保留这个分支差别。沿[模块导读](../../navigation/P03_文件操作与打开寿命导读.md#3.1_打开对象由谁持有)继续核对引用持有者与 fdinfo，再回到[总索引](../../navigation/P01_Linux_6.12_字符设备源码阅读索引.md)。

## 1.3\_打开回调失败与交付边界

固定提交dfaf2136的fs/open.c中，do_dentry_open把驱动回调的成功与后续打开步骤分开。以下裁剪只保留回调、成功标记、后续可能失败的一例与错误清理；省略的模式能力设置、预读初始化和大页缓存处理仍须在原文阅读。它不是可编译替代函数。中文Doxygen与注释是仓库补充。

```c
/**
 * @brief 仓库阅读说明：驱动回调成功后才建立已打开标记。
 * @param f 正在初始化的file，尚未保证返回给用户态。
 * @param open 可选打开回调，为空时从f_op选择。
 * @return 本片段分别展示回调失败和成功后的O_DIRECT拒绝。
 * @note 对应模块导读S0到S1，教材F1/F2候选份额应在回调内结算。
 */
/* do_dentry_open函数内片段，前面已取得操作表并完成部分检查。 */
if (!open)
    open = f->f_op->open;
if (open) {
    error = open(inode, f);
    if (error)
        goto cleanup_all;      /* 尚未设置FMODE_OPENED，回调须自行回滚。 */
}
f->f_mode |= FMODE_OPENED;      /* 回调已经成功，随后失败也按已打开文件清理。 */
/* 裁剪：设置能力、清理标志并初始化预读。 */
if ((f->f_flags & O_DIRECT) && !(f->f_mode & FMODE_CAN_ODIRECT))
    return -EINVAL;
/* 裁剪：剩余成功路径及return 0。 */
cleanup_all:
if (WARN_ON_ONCE(error > 0))
    error = -EINVAL;
fops_put(f->f_op);              /* 回调失败时归还操作表代码引用。 */
put_file_access(f);
cleanup_file:
path_put(&f->f_path);
f->f_path.mnt = NULL;
f->f_path.dentry = NULL;
f->f_inode = NULL;
return error;
```

cleanup_all没有调用驱动release。回调内已取得的私有对象份额必须由失败分支归还；VFS清理操作表和路径不会替驱动解释private_data。相反，设置FMODE_OPENED后再失败的路径，调用者仍须归还file，并由最终清理处理已成功建立的驱动上下文。

在同一固定版本fs/file_table.c中，__fput发现没有FMODE_OPENED会直接转向文件存储清理；已打开分支才执行f_op->release并随后fops_put。故“用户open返回失败”不足以判定release是否发生，应先判断驱动回调是否已经交付成功。回到[模块导读](../../navigation/P03_文件操作与打开寿命导读.md#3.1_打开对象由谁持有)看框架责任，再用[P28完整实验](../../../../../knowledge/linux/object_lifetime/kref/P28_文件实例与私有对象持有模板.md#28.2.1_哪一种失败需要自己回滚)核对候选取得和回调失败回滚。
