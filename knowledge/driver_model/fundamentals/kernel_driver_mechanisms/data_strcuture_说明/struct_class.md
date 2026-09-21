---
id: knowledge.driver_model.fundamentals.kernel_driver_mechanisms.data_strcuture_说明.struct_class
title: "class 成员与接口参考"
kind: reference
status: evolving
domains: [linux, kernel, driver]
---

# 第1章\_class成员与接口参考

内容摘抄自GPT

以上保留原材料的来源批注。本页现承担固定版本查询；连续学习从[class 与 sysfs 教材](../../../../linux/device_model/class_sysfs/大纲.md)进入，避免把成员表当作前置知识。原稿涉及的完整实例、输入校验、发布、并发和系统属性，分别落在该教材的五篇正文与参考中。

## 1.1\_版本与对象边界

基线为 NXP 官方 linux-imx 的标签 lf-6.12.20-2.0.0、固定提交 dfaf2136deb2af2e60b994421281ba42f1c087e0，对应 Linux 6.12.20。先从[源码总索引](../../../../../research/source_reading/driver_entries/navigation/P01_Linux_6.12_驱动入口源码阅读索引.md#1.1_版本与范围)确认身份，再查[include/linux/device/class.h](../../../../../research/source_reading/linux/include/linux/device/class.h)。

公开的 class 提供策略；内部 subsys_private 管理 kset、设备集合和锁。不能套用旧稿的 owner、p、devices、subsys 字段。class 不是 cdev，也不是 kset 的别名；一个 device 的 class 指针描述一个分类，多功能硬件可以关联多个功能 device。

## 1.2\_按职责查询当前成员

| 成员 | 本基线类型或回调签名 | 用途与约束 |
| --- | --- | --- |
| name | const char * | 分类名字，描述存续期必须有效 |
| class_groups | const struct attribute_group ** | class 本身的默认属性组 |
| dev_groups | const struct attribute_group ** | 类中设备的默认属性组；与每个 device.groups 区分 |
| dev_uevent | int (*)(const struct device *, struct kobj_uevent_env *) | 按契约补充事件环境；不是字符数据通知队列 |
| devnode | char *(*)(const struct device *, umode_t *) | 提供节点名及模式；临时名字由调用路径释放，不能返回待释放的字符串常量 |
| class_release | void (*)(const struct class *) | class 最终释放；动态便利创建已指定回收函数 |
| dev_release | void (*)(struct device *) | 设备最终释放的类级回退；不覆盖 device/type 已提供的协议 |
| shutdown_pre | int (*)(struct device *) | 关机路径的类级预处理，不等于热拔出回调 |
| ns_type | const struct kobj_ns_type_operations * | 命名空间类型操作，与 namespace 成对配置 |
| namespace | const void *(*)(const struct device *) | 取得设备所属命名空间标识 |
| get_ownership | void (*)(const struct device *, kuid_t *, kgid_t *) | 指定相关 sysfs 目录属主；不能当节点 MODE 规则 |
| pm | const struct dev_pm_ops * | 类级电源管理操作，仍受具体 PM 路径选择规则约束 |

框架成员属于公共协议，不要从驱动直接修改内部类设备链表。类迭代器、查找和设备引用的取得归还应成对理解，不能保存一个遍历中偶然见到的裸指针就认为它永久有效。

## 1.3\_常用接口与配对

| 任务 | 接口与返回 | 配对或后续动作 |
| --- | --- | --- |
| 动态创建类 | class_create(name)：指针或错误指针 | class_destroy；本版本只接收一个参数 |
| 登记调用者提供的类描述 | class_register(const struct class *)：0 或负错误 | class_unregister；调用者提供正确释放和描述寿命 |
| 创建分类设备 | device_create / device_create_with_groups：指针或错误指针 | 成功后 device_unregister，或用唯一 devt 的 device_destroy |
| 初始化并加入自有 device | device_initialize + device_add，或 device_register | 严格按引用协议清理，不能重复初始化 |
| 查询类内设备 | class_find_device 及按名字/号等辅助 | 成功返回持有的设备引用，用完 put_device |
| 遍历设备 | class_for_each_device 或 class_dev_iter_* | 使用迭代器时 init/next/exit 成对；额外长期保存要另持引用 |
| 类属性 | class_create_file / class_remove_file | class_attribute 接收 class，不是 device_attribute 回调 |
| 设备属性 | DEVICE_ATTR_* 与 groups，或 device_create_file/remove_file | 优先将初始属性随注册发布，避免事后补属性的事件窗口 |
| 属性通知 | sysfs_notify | 提醒支持通知的读者重读，不携带无丢失事件历史 |

class_create 的错误保留在错误指针中；device_create 的 class 不能为空，但 parent 可以为空。devt=0 是合法的纯属性对象，不建立设备号节点。多个零 devt 实例按保存的具体指针注销，不能靠 device_destroy(class, 0) 指定目标。具体创建与属性适配直达[唯一实现讲解](../../../../../research/source_reading/driver_entries/source_explanations/drivers/base/core.c.md#1.1_设备创建便利函数的所有权)。

## 1.4\_原接口速查的权威去向

| 查询内容 | 阅读位置与应核对的契约 |
| --- | --- |
| 模块入口、参数、许可证、别名与构建 | [模块构建与部署](../../../../../engineering/build/kernel_modules/大纲.md)；模块参数不是可任意变更硬件的通用运行期协议 |
| alloc_chrdev_region、号码、cdev | [设备号、注册与节点](../../../character_device/P02_设备号_注册与设备节点.md)；号码预留与字符分派分开 |
| open/read/write、部分用户复制 | [读写契约](../../../character_device/P05_文件操作契约与数据路径.md)；copy_*_user 返回未复制量，锁与位置按业务定义 |
| file_operations 扩展成员 | [成员参考](struct_file_operations.md)与[打开、迭代、映射教材](../../../file_operations/大纲.md) |
| show/store、解析、范围和互斥 | [属性控制实例](../../../../linux/device_model/class_sysfs/P02_让属性控制字符读取.md)；kstrtoint 接受单个尾换行，不应对 const 输入调用破坏式修剪 |
| uevent、devtmpfs、权限与节点 | [发布与排错](../../../../linux/device_model/class_sysfs/P03_从发布到设备节点.md)；uevent 文件不是事件历史 |
| 活动排空、热拔插、通知与成本 | [并发与撤销](../../../../linux/device_model/class_sysfs/P04_并发访问与撤销.md)；业务、对象、代码和硬件寿命分别证明 |
| 设备树、PM、cgroup、命名空间及其他文件系统 | [系统属性参考](../../../../linux/device_model/class_sysfs/P05_系统属性与接口选择参考.md)；按真实接口和配置限定，不凭路径前缀猜实现 |

错误码还应区分指针返回和整数返回；不能把 alloc_chrdev_region 的 int 当错误指针检查。根因分析从实际失败步骤开始，不把所有异常都归到权限、udev 或“内核版本不兼容”。

