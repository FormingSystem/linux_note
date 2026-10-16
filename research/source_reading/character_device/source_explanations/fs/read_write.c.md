---
id: research.character_device.source.read_write
title: "Linux6.12读取分派与同步适配"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
---

# 第1章\_Linux6.12读取分派与同步适配

驱动只实现 `.read_iter` 时，普通 read 仍能进入它；但回调拿到的是请求对象和目的缓冲迭代器，而不是直接拿到系统调用的四个参数。中间谁检查读取能力，谁构造临时对象，谁把位置送回？本页沿 vfs_read 与 new_sync_read 回答这些问题，并确定回调为什么不能把这个同步请求留待以后完成。

上游相对位置 `fs/read_write.c`，原文见[源码副本](../../../linux/fs/read_write.c)。版本采用[总索引](../../navigation/P01_Linux_6.12_字符设备源码阅读索引.md#1.1_版本和阅读边界)中的官方固定提交。调用者、位置地址和 R0～R4 见[请求位置导读](../../navigation/P03_文件操作与打开寿命导读.md#3.2_请求位置怎样回到调用方)：本页兑现其中时序图步骤 1～7，步骤 8、9 的共享或显式位置处理仍属上层。file 与操作表的持有关系沿[角色图](../../navigation/P03_文件操作与打开寿命导读.md#3.1_打开对象由谁持有)核对。下面中文 Doxygen 与行内注释由仓库补充，不是上游注释原文。

## 1.1\_vfs\_read选择回调

下面先分开权限和能力：FMODE_READ 表示打开时允许读，FMODE_CAN_READ 表示文件具备读取能力。EBADF、EINVAL、EFAULT 分别对应错误的打开方式、无效请求以及用户访问范围失败等分支；它们不是复制完成量。READ 是向范围检查函数传递的操作方向，MAX_RW_COUNT 是该路径采用的单次传输上限。

```c
/**
 * @brief 仓库阅读说明：在访问与范围检查后选择普通读取回调。
 * @param file 打开对象，f_mode 决定读权限与能力。
 * @param buf 用户目的地址。
 * @param count 请求长度，受 MAX_RW_COUNT 限制。
 * @param pos 此次调用的位置地址，不应假定就是 &file->f_pos。
 * @return 所选读取路径的实际进度或错误。
 * @note 下列为完整函数；R0 适配入口在 .read_iter 分支。
 */
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;

    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    if (!(file->f_mode & FMODE_CAN_READ))
        return -EINVAL;
    if (unlikely(!access_ok(buf, count)))
        return -EFAULT;

    ret = rw_verify_area(READ, file, pos, count);
    if (ret)
        return ret;
    if (count > MAX_RW_COUNT)
        count = MAX_RW_COUNT;

    if (file->f_op->read)         /* 此路径先考虑普通回调。 */
        ret = file->f_op->read(file, buf, count, pos);
    else if (file->f_op->read_iter)
        ret = new_sync_read(file, buf, count, pos);
    else
        ret = -EINVAL;
    if (ret > 0) {
        fsnotify_access(file);
        add_rchar(current, ret);
    }
    inc_syscr(current);
    return ret;
}
```

access_ok 的初步范围检查不保证真正复制一定完成，具体页面访问仍可能失败。rw_verify_area 进一步检查请求范围及安全访问；它也不把用户页固定为永不失效。正数结果进入 fsnotify_access 的访问通知和 add_rchar 的读取字节统计，完成分派后 inc_syscr 增加当前任务的读取调用统计；`current` 指本次执行任务。它们观察操作结果，不是设备内容锁或事务提交。负数和零不能被说成“同样取得了 count 字节”。本函数不直接把 pos 写成 file->f_pos，位置契约还取决于上层调用者。

同一文件中的 vfs_readv 优先使用 .read_iter，否则走旧回调的逐段路径。因此两套语义不同的 .read/.read_iter 会让不同用户接口观察到不同设备，不能只执行 cat 就宣称两条路径等价。为保持唯一实现展开，这里只给出 vfs_read 的函数体，readv 的协作阅读顺序由模块导读承担。

## 1.2\_new\_sync\_read构造请求

本函数建立栈上请求，要求回调在返回前交出最终结果。ITER_DEST 是迭代器的目的地类型标志；EIOCBQUEUED 表示请求已被接管、将异步完成，恰好与这里的同步约束冲突，所以 BUG_ON 宏把这一结果作为内核不变量被破坏来检查。

```c
/**
 * @brief 仓库阅读说明：把单缓冲同步读取适配为迭代回调。
 * @param filp 本次请求使用的打开对象。
 * @param buf 用户目的地址。
 * @param len 已由调用者裁剪的请求量。
 * @param ppos 本次位置地址，可为空；不是无条件共享位置。
 * @return 同步读取结果；不能将请求交给将来的异步完成者。
 * @note kiocb 和 iter 都是本次调用栈上的对象。
 */
static ssize_t new_sync_read(struct file *filp, char __user *buf,
                             size_t len, loff_t *ppos)
{
    struct kiocb kiocb;
    struct iov_iter iter;
    ssize_t ret;

    init_sync_kiocb(&kiocb, filp);
    kiocb.ki_pos = (ppos ? *ppos : 0);        /* R0：本次源位置。 */
    iov_iter_ubuf(&iter, ITER_DEST, buf, len); /* R0：本次目的缓冲。 */

    ret = filp->f_op->read_iter(&kiocb, &iter); /* 回调完成 R1～R3。 */
    BUG_ON(ret == -EIOCBQUEUED);
    if (ppos)
        *ppos = kiocb.ki_pos;                /* R4：回交请求位置。 */
    return ret;
}
```

ITER_DEST 表示迭代器承接读出的数据，与“内核从设备取数据”这个方向不要混淆。回调推进 ki_pos 与迭代器的缓冲进度，适配者只读取回调提交的 ki_pos。若回调复制失败且没有进度，就不应虚构新的位置。

BUG_ON 明确拒绝“本次同步调用返回后再完成”的结果；栈上请求也不能留给另一执行者在返回后继续使用。它不是一般异步 I/O 示例，不能用增加一个完成函数来绕开调用方约束。回到[模块导读](../../navigation/P03_文件操作与打开寿命导读.md#3.2_请求位置怎样回到调用方)比较 read、pread 与 readv，再按[总索引](../../navigation/P01_Linux_6.12_字符设备源码阅读索引.md)继续。

## 1.3\_修改回调时保留位置与完成归属

如果新增 `.read` 而保留原 `.read_iter`，普通 read 会优先走新回调，readv 却仍优先走迭代回调。两者必须兑现同一设备协议，或只提供一套能被适配的实现；不能只测试一个用户入口。位置方面，回调更新传入的请求位置，不能绕过上层直接改 file->f_pos，否则 pread 也可能意外改变共享位置。

把同步回调改为排队执行时，必须同时重新设计请求对象寿命、完成通知和调用入口，不能删掉 BUG_ON 后继续保存栈上 kiocb。最小验证集合应包含普通读取、显式位置读取、向量读取、零进度错误和部分进度；分别观察返回量、目的缓冲和共享位置。静态分支核对能说明当前选择顺序，不能替代真实用户缓冲与并发共享位置实验。
