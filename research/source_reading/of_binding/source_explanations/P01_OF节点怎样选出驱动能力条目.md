---
id: research.source_reading.of_binding.match_implementation
title: "OF节点怎样选出驱动能力条目"
kind: source
status: evolving
domains: [linux, kernel, driver]
---

# 第1章\_OF节点怎样选出驱动能力条目

节点写了v2、v1，驱动表却先写v1；为何最终仍能取得v2能力？本篇把答案落实到字段和分数比较，并继续跟到probe读取data。**讲解层次为固定NXP Linux 6.12.20实现**，版本与配置见 [总索引](../navigation/P01_OF绑定与别名源码阅读索引.md#1.1_版本与阅读任务)，调用全景见 [模块导读](../navigation/P02_描述身份怎样交给匹配与编号消费者.md#2.1_先沿设备关联找到本次匹配对象)。

## 1.1\_描述节点与能力表保存不同的数据

节点来自固件描述的展开，匹配表来自驱动映像。前者保存实际实例声明的身份，后者保存当前驱动认可的身份及能力指针，不能互相代替。上游 [`include/linux/of.h`](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/include/linux/of.h) 中本次需要的字段如下；这是结构片段，省略架构专用字段，不能直接充当完整内核头文件：

```c
/**
 * @brief 仓库补充阅读说明：固件描述节点的通用关系与属性入口。
 * @note 片段省略CONFIG_SPARC字段，不代表可独立替换上游定义。
 */
struct device_node {
    const char *name;
    phandle phandle;
    const char *full_name;
    struct fwnode_handle fwnode;
    struct property *properties;
    struct property *deadprops;
    struct device_node *parent;
    struct device_node *child;
    struct device_node *sibling;
#if defined(CONFIG_OF_KOBJ)
    struct kobject kobj;
#endif
    unsigned long _flags;
    void *data;
    /* 此处省略CONFIG_SPARC的unique_id和irq_trans。 */
};
```

`properties`串起属性，`deadprops`保留移除的属性记录；`property`本身的核心字段是`name`、字节长度`length`、值指针`value`和`next`。`fwnode`把OF节点接入通用固件节点接口；`_flags`属于OF核心状态，不能由消费者任意改写。`data`也不是设备驱动的`dev.driver_data`。根节点的parent可以为空，无子节点的child为空同样正常。

本版本没有`device_node.type`，类型相关匹配读取的是相应属性；也不能给此结构凭空补一个独立kref。`kobj`按`CONFIG_OF_KOBJ`存在，节点get/put接口又有动态配置边界，使用者应遵守接口的引用契约而不是直接操纵内部字段。`OF_POPULATED`用于枚举状态，不能解释成probe已经成功。完整引用责任回到 [OF生命周期](../../../../knowledge/driver_model/device_tree/设备树+platform开发/P09_OF属性读取与节点引用生命周期.md)。

驱动表来自 [`include/linux/mod_devicetable.h`](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/include/linux/mod_devicetable.h)：

```c
/**
 * @brief 仓库补充阅读说明：一个可匹配身份和它关联的驱动能力数据。
 * @note 三个名字全空表示表尾；data本身不参加匹配评分。
 */
struct of_device_id {
    char name[32];
    char type[32];
    char compatible[128];
    const void *data;
};
```

数组长度包含字符串终止空间。普通驱动通常只填compatible和data，以全零项结束；name/type非空时会增加条件。能力数据通常是静态常量，返回指针不复制对象、不增加对象引用，也不解析DT参数。

## 1.2\_分数怎样表达节点兼容顺序和附加条件

上游位置 [`drivers/of/base.c`](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/drivers/of/base.c)。`__of_device_is_compatible()`为一项打分，`__of_match_node()`在当前表内保留最佳项。以下两个函数体保留上游控制流，中文注释为仓库补充。

```c
/**
 * @brief 仓库补充阅读说明：按一个表项的三个条件计算节点匹配分数。
 * @param device 本次描述节点。
 * @param compat/type/name 当前表项的条件；空串表示未施加该条件。
 * @return 0表示不匹配，正值供同一表内比较。
 * @note 内部函数，调用方承担节点树访问的同步。
 */
static int __of_device_is_compatible(const struct device_node *device,
                                     const char *compat, const char *type,
                                     const char *name)
{
    struct property *prop;
    const char *cp;
    int index = 0, score = 0;

    if (compat && compat[0]) {
        prop = __of_find_property(device, "compatible", NULL);
        for (cp = of_prop_next_string(prop, NULL); cp;
             cp = of_prop_next_string(prop, cp), index++) {
            if (of_compat_cmp(cp, compat, strlen(compat)) == 0) {
                score = INT_MAX/2 - (index << 2);
                break;
            }
        }
        if (!score)
            return 0;
    }
    if (type && type[0]) {
        if (!__of_node_is_type(device, type))
            return 0;
        score += 2;
    }
    if (name && name[0]) {
        if (!of_node_name_eq(device, name))
            return 0;
        score++;
    }
    return score;
}

/**
 * @brief 仓库补充阅读说明：遍历一张以空名字项结束的表，选择最高分。
 * @param matches 当前驱动的能力表，不是所有驱动表的集合。
 * @param node 待匹配的描述节点。
 * @return 表内条目地址；无匹配则NULL。
 */
static const struct of_device_id *
__of_match_node(const struct of_device_id *matches,
                const struct device_node *node)
{
    const struct of_device_id *best_match = NULL;
    int score, best_score = 0;

    if (!matches)
        return NULL;
    for (; matches->name[0] || matches->type[0] || matches->compatible[0]; matches++) {
        score = __of_device_is_compatible(node, matches->compatible,
                                          matches->type, matches->name);
        if (score > best_score) {
            best_match = matches;
            best_score = score;
        }
    }
    return best_match;
}
```

第一个函数通过`__of_find_property`定位属性，`of_prop_next_string`逐个取得字符串，`index`表示它在节点列表中的位置。每后移一位减4，而匹配type最多加2、name最多加1，因而这些附加分不会越过一个compatible位置的差距。非空type/name匹配失败立即归零，它们是约束，不只是额外奖励。

`__of_node_is_type`检查节点类型属性，`of_node_name_eq`执行节点名比较，`of_compat_cmp`是OF兼容字符串比较入口；本篇不把它们替换成只比较前缀的自创规则。第二个函数不在首个非零分数处返回，而是扫完整表；严格`>`使同分保留早项。没有终止项就没有合法扫描边界，不能靠数组相邻位置“碰巧为零”保证安全。

公开`of_match_node()`在调用内部扫描前取得`devtree_lock`并保存IRQ状态，返回前释放并恢复。这个锁保护相应节点树访问，不把整个设备绑定变成一个原子动作；返回条目属于调用者提供的表，后续使用仍要求表所在驱动有效。关闭OF配置时应看头文件桩路径，不能声称依旧执行了上述算法。

用节点v2、v1手算：若只填compatible，v2的分数为`INT_MAX/2`，v1低4。表先写v1会暂时选择它，扫描v2时再替换；交换表项顺序结果不变。删除v2表项才会落到v1。这个演算解释了正文实验，不能据此宣称多个驱动之间也按此分数排序。

## 1.3\_平台匹配与probe读取怎样接到同一张表

上游 [`drivers/base/platform.c`](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/drivers/base/platform.c) 的`platform_match()`负责候选入口，以下是完整函数体的中文注释版：

```c
/**
 * @brief 仓库补充阅读说明：按平台总线策略判断当前设备和候选驱动。
 * @note 本函数只作匹配判断，不调用probe或分配实例私有状态。
 */
static int platform_match(struct device *dev, const struct device_driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    if (pdev->driver_override)
        return !strcmp(pdev->driver_override, drv->name);
    if (of_driver_match_device(dev, drv))
        return 1;
    if (acpi_driver_match_device(dev, drv))
        return 1;
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;
    return (strcmp(pdev->name, drv->name) == 0);
}
```

两个`to_platform_*`从内嵌公共对象回到所属平台对象。override一旦存在便决定这一入口的结果；之后OF/ACPI成功立即返回。平台ID表存在却不匹配时返回假，不再尝试最后的名字回退。这些分支不是把多种身份分数加起来，也不会绕过之后的probe。

`of_driver_match_device`使用候选驱动的OF表进入`of_match_device`。后者在 [`drivers/of/device.c`](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/drivers/of/device.c) 检查表、节点以及`of_node_reused`，再调用公开表扫描。`of_node_reused`标记复用了另一设备节点的情形，该OF匹配入口会拒绝，不能只检查of_node非空。

```c
/** @brief 仓库补充阅读说明：将Linux设备关联接到节点匹配。 */
const struct of_device_id *of_match_device(const struct of_device_id *matches,
                                           const struct device *dev)
{
    if (!matches || !dev->of_node || dev->of_node_reused)
        return NULL;
    return of_match_node(matches, dev->of_node);
}

/**
 * @brief 仓库补充阅读说明：在已有当前driver关联的上下文中取得能力数据。
 * @note 如probe调用；不是对任意尚无driver的device都安全的查询。
 */
const void *of_device_get_match_data(const struct device *dev)
{
    const struct of_device_id *match;

    match = of_match_device(dev->driver->of_match_table, dev);
    if (!match)
        return NULL;
    return match->data;
}
```

`of_device_get_match_data`直接读取`dev->driver`，调用者必须处于保证它有效的上下文。无匹配和匹配项data本身为NULL都可能得到NULL；驱动应按自己的表协议判断，不能把返回值转换成无条件有效结构。能力指针不会自动存入driver_data，这一步由正文的probe明确执行。

因此S1匹配、S2读能力并验证参数、S3服务是连续但不同的完成点。修改这里的策略会影响所有平台候选；只是增加某芯片变体时，应先调整该驱动自己的表和能力结构，而不是改公共匹配算法。回到 [模块导读的反例](../navigation/P02_描述身份怎样交给匹配与编号消费者.md#2.3_用同一组输入覆盖正常缺失与失败路径) 核对表顺序、缺项、参数失败和解绑，避免只测试一条成功路径。
