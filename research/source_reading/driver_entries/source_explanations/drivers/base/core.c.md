---
id: research.source_reading.driver_entries.core_class_attributes
title: "core.c 的设备创建与属性分派"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_core.c的设备创建与属性分派

[模块导读](../../../navigation/P03_分类对象与属性事务导读.md)已经把 S1 发布、S3 请求与 S4 撤销定位到固定源码。本篇只展开设备便利创建和属性适配函数。上游位置为 [drivers/base/core.c](../../../../linux/drivers/base/core.c)，版本身份见[总阅读索引](../../../navigation/P01_Linux_6.12_驱动入口源码阅读索引.md)。以下中文 Doxygen 和行内中文注释均为本仓库补充；函数语句保留原实现，不是可独立编译的驱动模板。

## 1.1\_设备创建便利函数的所有权

`device_create` 与 `device_create_with_groups` 处理可变参数后都进入 device_create_groups_vargs。后者把传入的关系、属性和私有数据放进分配的 device，再调用 device_add，而不是调用 device_register。因为它已经执行 device_initialize，重复走 register 会破坏“只初始化一次”的前提。

这里沿用错误指针章节的 IS_ERR_OR_NULL 与 ERR_PTR；初始 -ENODEV 表示没有可接受的分类对象，分配失败改为 -ENOMEM。GFP_KERNEL 允许按可睡眠的普通分配路径工作，不能把这套创建流程移到硬中断中。

```c
/**
 * 仓库阅读说明：创建带初始引用和公共释放函数的 device。
 * @class: 有效分类，不能为空或错误指针
 * @parent: 可选父设备
 * @devt: 设备号；零表示本例不建立号码属性和节点
 * @drvdata: 调用者持有的私有数据地址，不复制内容
 * @groups: 在发布之前安装的属性组指针
 * @fmt: 名字格式
 * @args: 名字参数
 * 返回：已成功添加的 device，或者保留实际原因的错误指针。
 */
static __printf(6, 0) struct device *
device_create_groups_vargs(const struct class *class, struct device *parent,
                           dev_t devt, void *drvdata,
                           const struct attribute_group **groups,
                           const char *fmt, va_list args)
{
    struct device *dev = NULL;
    int retval = -ENODEV;

    if (IS_ERR_OR_NULL(class))
        goto error;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) {
        retval = -ENOMEM;
        goto error;
    }

    device_initialize(dev);
    dev->devt = devt;
    dev->class = class;
    dev->parent = parent;
    dev->groups = groups;
    dev->release = device_create_release;
    dev_set_drvdata(dev, drvdata);

    retval = kobject_set_name_vargs(&dev->kobj, fmt, args);
    if (retval)
        goto error;

    /* 所有回调需要的指针已经准备好，现在才发布。 */
    retval = device_add(dev);
    if (retval)
        goto error;

    return dev;

error:
    put_device(dev);
    return ERR_PTR(retval);
}
```

device_initialize 建立初始引用；device_create_release 是本文件里的公共释放函数，最终释放动态分配的 device。失败标签对 NULL 调用 put_device 也合法，尚未分配时无需另写分支；已分配并初始化后则归还创建引用。调用者拿到错误指针不再重复 put 或 unregister。

这份所有权只涵盖 device，不涵盖 drvdata 指向的业务对象。后者在 S1 公开之前就必须有效，并保持到所有相关访问结束。把静态数据改成动态分配时，要连同属性、字符打开、后台工作一起重新证明回收点。

可修改边界：调用方可以改变名字、有效父对象、设备号和已准备好的 groups，不应在发布后才补齐 show/store 所需的数据。若不需要 class，使用适合场景的分步 device 初始化或已有子系统接口，不能给这个便利函数传 NULL 后期待它替你创建匿名分类。直接更换 release 也不是普通扩展点：必须先明确谁分配、谁最终归还外层对象。

## 1.2\_从通用属性回到设备回调

sysfs 使用通用 kobject 与 attribute，业务回调却需要 device 与 device_attribute。两组结构通过内嵌成员的地址关系适配。这里的 to_dev_attr 宏在同一上游文件中用 container_of 找外层对象，kobj_to_dev 找到包含 kobject 的 device；不能将非 device 类型的 kobject 配上这套操作表。

回退值 -EIO 是输入输出错误，PAGE_SIZE 是当前内核页大小常量。它们在这里分别标记无法分派的回调和异常输出长度，不是业务字段范围。

```c
/**
 * 仓库阅读说明：调用设备属性 show，返回实际格式化字节数或错误。
 * @kobj: device 内嵌的对象
 * @attr: device_attribute 内嵌的通用属性
 * @buf: sysfs 提供的输出缓冲
 */
static ssize_t dev_attr_show(struct kobject *kobj, struct attribute *attr,
                             char *buf)
{
    struct device_attribute *dev_attr = to_dev_attr(attr);
    struct device *dev = kobj_to_dev(kobj);
    ssize_t ret = -EIO;

    if (dev_attr->show)
        ret = dev_attr->show(dev, dev_attr, buf);
    if (ret >= (ssize_t)PAGE_SIZE) {
        printk("dev_attr_show: %pS returned bad count\n",
                dev_attr->show);
    }
    return ret;
}

/**
 * 仓库阅读说明：将已经复制到内核的输入交给设备属性 store。
 * @kobj: device 内嵌的对象
 * @attr: 指定本次设备属性
 * @buf: 只读输入，不能靠破坏缓冲来修剪字符串
 * @count: 本次传入字节数
 * 返回：业务回调结果；没有回调时为 -EIO。
 */
static ssize_t dev_attr_store(struct kobject *kobj, struct attribute *attr,
                              const char *buf, size_t count)
{
    struct device_attribute *dev_attr = to_dev_attr(attr);
    struct device *dev = kobj_to_dev(kobj);
    ssize_t ret = -EIO;

    if (dev_attr->store)
        ret = dev_attr->store(dev, dev_attr, buf, count);
    return ret;
}
```

dev_sysfs_ops 把通用 show/store 指向以上函数。sysfs 的 kernfs 节点保存属性地址，父节点保存对象地址，因此 S3 能从文件请求回到原设备，并进一步由 dev_get_drvdata 找业务状态。普通读路径经 sysfs_kf_seq_show，把 show 结果交给 seq_file；写路径经 sysfs_kf_write 传入同一组对象和输入。

这两个适配函数既没有获取业务 mutex，也没有检验数值范围。缺少回调时的错误回退不是注册错误的通用检测器：属性文件是否可读写还经过模式与操作选择，不能保证用户一定能走到这条 -EIO 分支。show 的长度检查只报警，不会把一个越界写自动变安全；调用方应使用 sysfs_emit 并正确约束输出。

修改回调时应同时检查 S3 的失败不提交、共享状态同步及 S4 的活动排空。增加另一个属性不应复制一套状态，而应根据实例所属把它接到同一或明确分离的业务对象。通过 device_remove_file、组移除或 device 注销撤属性时，公共框架管理的是属性活动，并不替调用者取消任意工作队列或关闭字符文件。

返回[模块导读](../../../navigation/P03_分类对象与属性事务导读.md#3.3_一次属性写入的状态落点)、[总索引](../../../navigation/P01_Linux_6.12_驱动入口源码阅读索引.md)或[教材](../../../../../../knowledge/linux/device_model/class_sysfs/大纲.md)。
