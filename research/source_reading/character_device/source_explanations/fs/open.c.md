---
id: research.character_device.source.open
title: "Linux6.12文件打开与关闭入口实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
---

# 第1章\_Linux6.12文件打开与关闭入口实现

应用关闭一个描述符时，驱动的 flush 可能执行，release 却未必执行；应用打开失败时，情况又分成“驱动自己回滚”和“框架还会执行 release”。要写对清理，必须找出两条边界：关闭归还的是哪一份引用，打开回调何时把私有上下文交给框架。

上游相对位置为 `fs/open.c`，原文见[源码副本](../../../linux/fs/open.c)。版本采用[总索引](../../navigation/P01_Linux_6.12_字符设备源码阅读索引.md#1.1_版本和阅读边界)中的官方固定提交。[打开寿命模块导读](../../navigation/P03_文件操作与打开寿命导读.md#3.1_打开对象由谁持有)的角色图定位 file、操作表与私有上下文；其时序图步骤 1～6 对应本页两条关闭路径，步骤 7～9 对应通用 fput 的延后清理。下面 F0～F4 指文件寿命周期，中文 Doxygen 和行内注释是仓库补充，代码保留固定版本语句。

## 1.1\_filp\_close归还文件引用

先看已经持有有效 file 的内核调用者怎样归还一份打开引用。filp_close 将可选关闭钩子与引用归还放在一起，返回值来自钩子，但钩子报错也不能让已经消耗的引用“复活”。它不会等待 dup 或映射持有者退出。

```c
/**
 * @brief 仓库阅读说明：执行关闭钩子并归还调用者持有的文件引用。
 * @param filp 仍有效的文件对象；调用者将这份引用交给本函数归还。
 * @param id 关闭所属的锁所有者标识，不是用户 fd 数字。
 * @return filp_flush 的结果，不是最终 release 的返回值。
 * @note F3 的关闭与 F4 的最终清理不是同一事件。
 */
int filp_close(struct file *filp, fl_owner_t id)
{
    int retval;

    retval = filp_flush(filp, id); /* F3：可选 flush 及关闭关联的清理。 */
    fput(filp);                   /* 归还引用；不保证此处同步执行 F4。 */

    return retval;
}
```

filp_flush 在确认计数没有损坏后调用可选 f_op->flush；非 FMODE_PATH 路径还处理目录通知及 POSIX 锁。这里保存返回值以后仍执行 fput，不能根据 flush 报错认定文件引用尚未归还，更不能让驱动 flush 直接释放仍可能被 dup 使用的上下文。

fput 的实现位于 [fs/file_table.c](../../../linux/fs/file_table.c)：非最后引用只减少计数，最后引用才进入最终清理安排。最终 __fput 在适用时解除异步通知，再调用 release，然后 fops_put。普通 fput 可将这部分工作留给任务工作或延迟工作；源码阅读不能把一个函数名当成“所有资源已经同步释放”的证明。

## 1.2\_close系统调用的同步清理

用户 close 还需要先按 fd 数字撤掉表项。撤掉以后，其他线程可能立即复用这个数字，所以不能在后续钩子遇到可重启错误时重新执行整个 close。固定版本因此选择同步归还这份引用，并在返回前转换内部重启错误。下面保留完整入口，便于同时看清入口撤销和错误返回的先后。

SYSCALL_DEFINE1 是定义一个参数系统调用入口的内核宏，此处参数就是 fd。EBADF 是没有对应有效描述符的错误码。宏展开的入口包装不是驱动的 file_operations.close 成员；后者根本不在当前操作表中。

```c
/**
 * @brief 仓库阅读说明：从 fd 表撤出入口，再同步归还其文件引用。
 * @param fd 当前调用者的描述符数字。
 * @return 无对应文件时为 -EBADF，其余结果来自关闭钩子及错误转换。
 * @note 以下保留完整系统调用体，依赖上游宏与调用上下文。
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

    /* fd 已撤出，不能重新执行整个 close，改为向用户报告中断。 */
    if (unlikely(retval == -ERESTARTSYS ||
                 retval == -ERESTARTNOINTR ||
                 retval == -ERESTARTNOHAND ||
                 retval == -ERESTART_RESTARTBLOCK))
        retval = -EINTR;

    return retval;
}
```

__fput_sync 减少 f_count，仅当计数降到零才同步 __fput。若 VMA 或另一个描述符还持有 file，这次 close 依然不能触发 release。“同步”限定的是最后引用这一分支，不会把其他持有者强制清掉。

四个 `ERESTART...` 名称是内核内部不同的系统调用重启结果，`EINTR` 是交给用户的中断错误。这里转换它们的理由不是隐藏错误，而是数字入口已经撤销。应用不能因为收到这个错误，就无条件再次 close 同一个数字；它可能已经属于另一次打开。

该路径没有先调用 filp_close，也没有等待通用 fput 的任务工作。教材可以说明一般文件引用归还存在延迟，但在分析本版本一次实际 close 时必须保留这个分支差别。沿[模块导读](../../navigation/P03_文件操作与打开寿命导读.md#3.1_打开对象由谁持有)继续核对引用持有者与 fdinfo，再回到[总索引](../../navigation/P01_Linux_6.12_字符设备源码阅读索引.md)。

## 1.3\_打开回调失败与交付边界

现在倒回打开端：回调分配了一份 private_data，随后用户 open 失败。这份内存由谁释放，不能只看系统调用结果；必须判断回调是否已成功交付。do_dentry_open 在普通打开回调成功以后设置 `FMODE_OPENED`，最终清理再据此决定是否调用 release。单纯定位路径的 `O_PATH` 分支不调用这里的服务 open，也不在下面这条教学路径内。

以下固定版本裁剪保留回调、成功标记、后续可能失败的一例与错误清理。`O_DIRECT` 表示请求直接 I/O 的打开标志，`FMODE_CAN_ODIRECT` 表示框架认可该文件具备对应能力；这里仅用二者不匹配来展示“回调成功以后仍失败”，不展开直接 I/O 数据路径。省略的能力设置、预读初始化和大页缓存处理不是本节结论所需的实现，片段也不是可编译替代函数。

```c
/**
 * @brief 仓库阅读说明：驱动回调成功后才建立已打开标记。
 * @param f 正在初始化的file，尚未保证返回给用户态。
 * @param open 可选打开回调，为空时从f_op选择。
 * @return 本片段分别展示回调失败和成功后的O_DIRECT拒绝。
 * @note 对应模块导读F0到F1，回调内候选资源须在成功或失败时结算。
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

cleanup_all 没有调用驱动 release。回调内已取得的私有对象份额必须由失败分支归还；fops_put 归还操作表的代码引用，put_file_access 归还已建立的文件访问权，path_put 归还路径引用，最后清空路径及 inode 指针。这些动作都不会替驱动解释 private_data。WARN_ON_ONCE 将不合约的正错误值告警并改为负的 EINVAL，它不承担私有资源回滚。相反，设置 FMODE_OPENED 后再失败的路径，调用者仍须归还 file，并由最终清理处理已成功建立的驱动上下文。

在同一固定版本 fs/file_table.c 中，__fput 发现没有 FMODE_OPENED 会直接转向文件存储清理；已打开分支才执行 f_op->release 并随后 fops_put。故“用户 open 返回失败”不足以判定 release 是否发生，应先判断驱动回调是否已经交付成功。回到[模块导读](../../navigation/P03_文件操作与打开寿命导读.md#3.1_打开对象由谁持有)看框架责任，再用[P28完整实验](../../../../../knowledge/linux/object_lifetime/kref/P28_文件实例与私有对象持有模板.md#28.2.1_哪一种失败需要自己回滚)核对候选取得和回调失败回滚。

## 1.4\_把资源交接点作为修改边界

新增打开资源时，每一项都要在“回调失败”中自行归还，或在成功交付后由最终 release 配对归还；不能让两条路径都释放同一份，也不能两条都假定对方负责。若把分配搬到成功标记之后，还必须重新核对所有后续失败调用者怎样归还 file。

关闭端同样不能把上下文释放从 release 搬到 flush 来追求立即回收：两个 dup 描述符中只关闭一个就会破坏另一持有者。也不要随意把通用 fput 换成 __fput_sync；同步清理会在当前上下文调用可能睡眠和再次取锁的路径，需要重新证明锁顺序与调用环境。验证时分别覆盖回调失败、回调成功后框架失败、多个描述符、映射持有和最后一份引用，并观察回滚次数及 release 次数，而不是只观察 open/close 返回值。
