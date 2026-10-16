---
id: research.character_device.source.libfs
title: "Linux 6.12 libfs 有限缓冲区复制实现"
kind: source
status: evolving
domains: [linux, kernel, source_reading]
---

# 第1章\_Linux6.12libfs有限缓冲区复制实现

用户从位置 2 请求四字节，实际只复制三字节时，驱动必须同时交回返回量 3 和新位置 5。若二者不一致，下一次访问就会重复或跳过内容。有限缓冲区帮助函数抽出了范围裁剪和进度换算；本页沿读、写两个函数确认它怎样兑现这项责任，以及为什么它不能替设备保证快照或整条替换。

上游相对位置为 `fs/libfs.c`，版本采用[总阅读索引](../../navigation/P01_Linux_6.12_字符设备源码阅读索引.md#1.1_版本和阅读边界)中的官方固定提交。原文副本见[源码文件](../../../linux/fs/libfs.c)。角色与状态地址沿[参与者关系图](../../navigation/P02_Linux_6.12_有限缓冲区IO模块导读.md#2.1_参与者与状态落点)定位；本页函数实现的是[复制时序图](../../navigation/P02_Linux_6.12_有限缓冲区IO模块导读.md#2.3_复制层与调用者的交接)步骤 2～9 的范围、复制和提交箭头。S0～S4 是一次帮助函数调用，下面中文 Doxygen 与行内注释是仓库补充，函数语句来自固定版本。

## 1.1\_simple\_read\_from\_buffer

下面的 `EINVAL` 是参数无效的错误码名，此处对应负位置；`EFAULT` 是地址访问失败的错误码名，此处对应用户复制没有取得进度。它们属于回调返回协议，不是函数调用或缓冲区状态。

```c
/**
 * @brief 仓库阅读说明：从有限内核缓冲区向用户复制并提交实际位置。
 * @param to 用户目标地址；调用者仍须提供有效的内核源对象。
 * @param count 本次请求字节数，S1 会裁剪到可用范围。
 * @param ppos 调用者的位置地址，S3 仅按实际完成量更新。
 * @param from 内核源缓冲区。
 * @param available 源缓冲区有效长度，不是任意分配容量。
 * @return 完成量、末尾的零，或没有取得进度时的错误。
 * @note 本函数不取得设备锁，也不持有驱动对象引用。
 */
ssize_t simple_read_from_buffer(void __user *to, size_t count, loff_t *ppos,
                               const void *from, size_t available)
{
    loff_t pos = *ppos;                 /* S0：保存本次起点。 */
    size_t ret;

    if (pos < 0)                       /* S1：先拒绝负位置。 */
        return -EINVAL;
    if (pos >= available || !count)
        return 0;
    if (count > available - pos)
        count = available - pos;
    ret = copy_to_user(to, from + pos, count); /* S2：ret 是未复制量。 */
    if (ret == count)
        return -EFAULT;
    count -= ret;                      /* S3：只提交实际进度。 */
    *ppos = pos + count;
    return count;                      /* S4：把完成量交给调用者。 */
}
```

最值得停下来的判断是 `ret == count`，不是 `ret != 0`。只要已有部分进度，函数就报告这部分进度并推进位置，避免下一次重复读取。完全没有复制时，位置尚未写回，故保持原值。`from + pos` 使用内核构建所采用的 GNU C 指针运算形式，不应原样用它证明任意标准 C 环境都接受同样写法。

范围检查依赖调用者给出的 `available` 正确，而且复制期间源对象必须稳定。函数不认识设备断开标志、缓冲区锁或快照版本；这些状态必须在调用上下文中处理。

## 1.2\_simple\_write\_to\_buffer

现在把方向反过来：用户更新内核中的四字节记录。虽然“范围检查、复制、更新位置”的代码形状相同，复制目标已经成为共享内容；返回值形成之前，旧记录就可能被覆盖。因此先看这个函数是否提供回滚，再决定它能否直接用于整条配置更新。

```c
/**
 * @brief 仓库阅读说明：将用户字节写入有限内核缓冲区并推进位置。
 * @param to 内核目标地址，S2 可能在返回前已被部分修改。
 * @param available 目标可写容量。
 * @param ppos 调用者的位置地址。
 * @param from 用户源地址。
 * @param count 请求长度。
 * @return 实际复制量、无可写范围时的零，或错误。
 * @note 不更新驱动自己的有效长度，不保证整条记录原子替换。
 */
ssize_t simple_write_to_buffer(void *to, size_t available, loff_t *ppos,
                             const void __user *from, size_t count)
{
    loff_t pos = *ppos;                 /* S0：取本次位置。 */
    size_t res;

    if (pos < 0)                       /* S1：限定合法范围。 */
        return -EINVAL;
    if (pos >= available || !count)
        return 0;
    if (count > available - pos)
        count = available - pos;
    res = copy_from_user(to + pos, from, count); /* S2：目标可发生变化。 */
    if (res == count)
        return -EFAULT;
    count -= res;                      /* S3：按实际进度更新位置。 */
    *ppos = pos + count;
    return count;                      /* S4：报告完成量。 */
}
```

对称结构容易使人遗漏两个边界。第一，`available` 是容量，函数没有驱动的 `data_length` 字段，不能替调用者决定新有效长度。第二，位置不变并不表示目标内容不变：复制层可能已经写入部分字节或处理未复制的尾部。需要失败保留旧记录时，应把 `to` 指向请求私有区域，完成后由驱动提交，而不是期望函数隐式回滚。

这两个函数在上游通过导出符号供适用调用者使用。导出可用性只解决链接接口，不扩大它们的同步保证。读完实现后回到[模块导读](../../navigation/P02_Linux_6.12_有限缓冲区IO模块导读.md#2.3_复制层与调用者的交接)，用部分复制与零进度两条路径检验自己的回调。

## 1.3\_改调用方式时哪些配对不能拆

这两个函数的共同不变量是：裁剪后的复制量只覆盖合法范围，返回的正数等于已完成量，位置只按该完成量推进。把 `ret == count` 改成 `ret != 0` 会将已有进度藏在错误返回后；上层若重试整个请求，已交付部分就可能被重复处理。反过来，用原始请求量推进位置会跳过尚未交付的数据。

设备若需要满容量时报错、只接受整条记录或维护新的有效长度，最小修改面在驱动调用协议：先检查业务条件，选择请求私有区或受保护的共享区，核对复制结果，再提交本设备状态。不要为一个设备改动通用帮助函数的零返回或短传输约定。验证应覆盖负位置、末尾、零请求、跨末尾、部分复制和完全失败；真实用户复制失败需要目标内核环境，普通 C 模型只能核对进度计算。
