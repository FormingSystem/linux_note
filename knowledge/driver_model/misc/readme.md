---
id: knowledge.driver_model.misc.readme
title: "用misc发布一个小型字符设备"
kind: subsystem
status: evolving
domains:
  - linux
  - driver
---

# 第1章\_用misc发布一个小型字符设备

在字符设备实验中，即使只想返回一句问候，也要考虑号段、cdev、class 与节点。那些步骤分别解决真实问题，不能凭空省去；但多个小型服务能不能复用其中的公共设施？

misc 是 miscellaneous 的缩写，这里指 Linux 的一套小型字符设备登记机制。它复用主设备号和分派入口，替每个服务安排次设备号及设备模型入口。本章以固定只读问候为完整例子，承接[字符设备读写契约](../character_device/P05_文件操作契约与数据路径.md)与[设备号登记](../character_device/P02_设备号_注册与设备节点.md)。若刚读过[kobject 属性](../fundamentals/framework_model/kobject讲解.md)，还可以比较两个文件入口的不同用途。

## 1.1\_哪些工作适合共享

一个字符节点通过主、次设备号寻找服务。可以让许多小服务使用同一个主号，再由公共代码根据次号找出各自的操作表。各服务不必独占一整套号码登记与 class，但仍须说明自己的文件操作是什么。

misc 就采用这个办法：固定主号为 10，各服务用 `struct miscdevice` 描述自己的名字、次号和 `file_operations` 操作表。请求动态次号时填写 `MISC_DYNAMIC_MINOR`；它的数值 255 是“请分配”的标记，不是承诺服务以后一定用 255。注册成功后，该结构的 minor 被写成实际号码。

公共分派表与编号分配器都在内核里，锁保护注册、撤销和打开时的查找。节省的工作转移到了这套公共设施，没有消失。驱动仍要维护内容、并发与生命周期；如果需要一整段连续号码、特殊的多实例组织或已有领域子系统协议，应先判断 misc 是否真的合适。

## 1.2\_从一次打开看到分派

用户打开 /dev/note_misc 时，VFS 按字符设备号进入主号 10 的公共 misc_open。它读取次号，在全局 misc_list 中找到匹配的 miscdevice，取得该服务操作表的模块引用，把 file 的操作表换成服务自己的表，然后才调用服务的 open。

公共代码还会把 `file->private_data` 设成该 miscdevice 的地址。本章没有私有可变状态，不使用它。若以后把 miscdevice 内嵌到自定义对象，可在自己的 open 中由这个指针找到外层实例；不能先无条件覆盖它，再希望框架帮你找回实例。

```mermaid
flowchart LR
    node["字符节点<br/>主号 10 / 实际次号"] -->|"open 依据号码分派"| common["misc_open"]
    common -->|"持 misc_mtx 查找次号"| list["misc_list<br/>note_device.list"]
    list -->|"取得 note_device.fops"| ops["note_fops<br/>owner / open / read"]
    common -->|"替换 f_op，设置 private_data"| file["本次打开的 struct file"]
    file -->|"后续 read 调用"| cb["note_read"]
    cb -->|"读取不变内容，推进位置"| data["greeting / file 的位置"]
```

如果没有找到服务，公共路径可能请求装载能处理该设备号的模块后再查一次；仍找不到才失败。这不是“用户任意创建一个节点，内核就会凭名字生成驱动”。本实验显式 insmod，避免把自动装载配置作为隐藏前提。

共享分派也有成本：本版本 misc_open 持有 misc_mtx 调用服务自己的 open。某个服务的 open 很慢，会让其他 misc 服务的注册、注销和打开查找一起等待。服务 open 中若又同步注册或注销 misc，还会回头请求同一把锁；不能把公共设施当作完全独立于自身回调的后台服务。

## 1.3\_完整程序与返回契约

保存为 note_misc.c，或使用[同名材料](../../../labs/kernel/driver_entries/materials/note_misc.c)。程序首行的 SPDX-License-Identifier 标明示例使用 GPL-2.0 许可证，模块元信息沿用 Hello 工程。它没有写操作，不访问硬件，唯一数据是 16 字节的 hello from misc 加换行。

```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

static const char greeting[] = "hello from misc\n";

static int note_open(struct inode *inode, struct file *file)
{
    int ret = nonseekable_open(inode, file);

    if (ret)
        return ret;
    /* 同一个打开文件被共享时，串行化常规 read 的位置更新。 */
    file->f_mode |= FMODE_ATOMIC_POS;
    return 0;
}

static ssize_t note_read(struct file *file, char __user *buf,
                         size_t count, loff_t *pos)
{
    return simple_read_from_buffer(buf, count, pos,
                                   greeting, sizeof(greeting) - 1);
}

static const struct file_operations note_fops = {
    .owner = THIS_MODULE,
    .open = note_open,
    .read = note_read,
};

static struct miscdevice note_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "note_misc",
    .fops = &note_fops,
    .mode = 0600,
};

static int __init note_misc_init(void)
{
    return misc_register(&note_device);
}

static void __exit note_misc_exit(void)
{
    misc_deregister(&note_device);
}

module_init(note_misc_init);
module_exit(note_misc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("固定只读内容的 misc 教学设备");
```

`simple_read_from_buffer` 根据当前位置和请求长度，把固定内核缓冲区的一段复制给用户并推进位置。传给它的总长度减去字符串末尾的零字符，所以应用不会读到 C 字符串的终止零。读到 16 字节末尾后再读返回 0，即本次打开已到 EOF；重新打开有新的文件位置，又可从头读。这是有限内容，不是“当前暂时无数据”的事件流。

open 中用 nonseekable_open 禁止该文件的定位读取等操作；这里没有为内容提供 seek。FMODE_ATOMIC_POS 要求常规 read 在同一打开文件被共享时协调位置更新，这延续前置窗口章已经解释的位置保护。内容本身是不变数组，无需再为数组加互斥锁；若把它改成可写缓冲区，就必须重新设计内容同步，不能把位置保护当作万能锁。

操作表的 owner 指向本模块。公共打开路径取得它的引用后，普通卸载会受到尚未关闭的文件约束，防止操作表代码先于读者消失。没有 owner 不是“减少一些开销”的等价改写。

name 决定发布名称，mode 0600 使默认节点只对所有者开放读写权限；本例没有 write 回调，所以即使权限允许，写操作仍不在服务契约内。模式控制的是访问许可，操作表决定的是可执行功能。用户空间设备管理规则可能调整最终权限，应以实际节点为准。

## 1.4\_登记成功之前与注销之后

用一轮模块生存期给过程编号。S0 只有静态 note_device，S1 取得次号并创建模型入口，S2 加入可分派集合，S3 用户打开后持有文件，S4 模块退出时撤销登记。号码、列表、模型节点、打开文件引用不是同一份状态。

```mermaid
sequenceDiagram
    autonumber
    participant mod as 模块初始化或退出
    participant misc as misc 核心
    participant model as 次号与设备模型
    participant app as 应用
    mod->>misc: S0 misc_register(note_device)
    misc->>model: S1 取得次号，创建模型入口
    alt 创建成功
        misc->>misc: S2 持 misc_mtx 加入 misc_list
        misc-->>mod: 返回 0
        app->>misc: S3 open 按次号查找
        misc->>app: 交付持模块引用的文件操作
        app->>app: read 推进位置，最终 close
        mod->>misc: S4 普通卸载调用 misc_deregister
        misc->>misc: 持锁从列表移除
        misc->>model: 撤销设备入口，归还次号
    else 模型入口失败
        misc->>model: 归还次号，恢复动态请求标记
        misc-->>mod: 返回错误，模块初始化失败
    end
```

在这个版本里，创建模型入口发生在加入 misc_list 之前，但这段过程持有 misc_mtx；同时到来的 open 必须等它释放锁，才能看见成功后的分派状态。不能把“节点曾出现”直接当作锁内事务已经结束。

图中步骤 1、2 准备登记，步骤 3、4 才完成成功发布；步骤 5～7 是打开使用，步骤 8～10 是普通退出的撤销。步骤 11、12 展示模型入口失败后的回滚；若更早就分配不到次号，也会直接失败，不会继续创建设备。已打开文件不会在每次 read 时重新查 misc_list，因此后面讨论热拔插时，还必须单独处理旧引用。

调用方只在注册成功之后才负责注销。注册失败时内部回滚自己已取得的号码等状态，初始化函数返回错误即可；不应无条件再调用 misc_deregister 清理一项根本没成功的登记。

普通模块卸载与运行中热拔插也要分开。这个例子的对象是模块静态数据，打开文件会阻止普通卸载，因而 exit 不会在这些引用仍在时正常运行。但 **misc_deregister 本身并不关闭已有文件**。若在设备热拔插时调用它，旧文件仍可能调用已经保存的操作表，驱动必须另外保活外层对象、标记离线并协调在途操作。不能把这里的静态示例改成注销后立即 kfree 的模板。

## 1.5\_用三轮观察核对身份\_位置与寿命

本轮先比较“设备身份与内容”，再比较“同一次打开的短读与再次打开”，最后保留一个 file 观察普通卸载的限制。下图沿用本节小标题作为实验位置；每一阶段都要先完成前一阶段的清理，不能把还持有 fd 的旧进程忘在后台。

```mermaid
flowchart LR
    a["1.5.1 构建、加载与身份"] -->|"节点与问候符合预期"| b["1.5.2 分段读取、重开并卸载"]
    b -->|"重新装载"| c["1.5.3 保留打开者、比较两次卸载"]
```

### 1.5.1\_先核对身份再读取

可以使用完整[材料目录](../../../labs/kernel/driver_entries/materials/README.md)，也可在专用练习目录保存上面的 note_misc.c，并建立以下独立 Makefile：

```make
# 当前练习只生成 misc 问候模块。
obj-m += note_misc.o
```

下面默认在具有本机编译器和匹配构建树的目标 Linux 上构建。先确定运行内核的构建入口：

```bash
# 执行位置：目标 Linux 练习目录，已有 note_misc.c 和 Makefile。
export KDIR="/lib/modules/$(uname -r)/build"
test -f "$KDIR/Makefile"
```

KDIR 保存运行内核配套构建树的路径；检查失败时先补齐环境，不要随意换成另一内核的头文件。若在主机交叉构建，按已学的[模块工程](../../../engineering/build/kernel_modules/大纲.md)设置目标匹配的 KDIR、ARCH 与 CROSS_COMPILE，传送产物后再从下面加载步骤继续。主机不负责运行目标模块。

```bash
# 执行位置：准备好的构建环境中的本章练习目录。
make -C "$KDIR" M="$PWD" note_misc.ko
```

`-C` 进入内核构建树，`M` 指出当前外部模块目录；只请求本章的 .ko。生成目标产物后加载：

```bash
# 执行位置：目标的匹配产物目录，尚未加载 note_misc。
sudo insmod ./note_misc.ko
```

成功返回说明初始化登记已完成。接着读取发布身份：

```bash
# 执行位置：同一目标会话，sysfs 已挂载。
cat /sys/class/misc/note_misc/dev
```

应显示 10:实际次号；实际值由本次登记决定，不要把“实际次号”几个字抄进 mknod。随后核对节点类型与主次号：

```bash
# 执行位置：目标会话，节点管理已处理本次创建设备事件。
test -c /dev/note_misc
ls -l /dev/note_misc
```

ls 显示的号码应与 sysfs 一致；test 确认它确实是字符设备。若 sysfs 入口存在而节点不存在，先检查 devtmpfs 挂载和节点管理，具体排错沿[节点排错](../character_device/P12_常见故障与排查.md)，不要用重定向创建一个同名普通文件。核对通过后读取：

```bash
# 执行位置：目标会话，节点身份已经确认。
sudo cat /dev/note_misc
```

应打印 hello from misc 和换行，然后因读到结尾而结束。这是一次新的打开，下一步程序会独立创建自己的位置。

### 1.5.2\_比较短读与再次打开

再运行完整的分段读取观察程序。O_RDONLY 表示只读打开；返回值和字节内容分别检查，out 清理路径归还已经打开的描述符。两次非空读取应为 5 与 11 字节，随后是 0；再次打开后应读到完整 16 字节：

保存下面完整程序为 `misc_probe.c`。它使用 Linux 用户空间头文件和 C11 编译器，编译后在装有对应教学模块的目标运行：

```c
/* 每次打开各自维护读取位置，错误时也关闭已有描述符。 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int read_part(int fd, size_t requested, const char *expected)
{
    char data[64];
    size_t length = strlen(expected);
    ssize_t count = read(fd, data, requested);
    if (count < 0) { perror("read"); return 0; }
    if ((size_t)count != length || memcmp(data, expected, length)) {
        fputs("读取内容或长度不符\n", stderr);
        return 0;
    }
    printf("read: %zd bytes\n", count);
    return 1;
}

int main(void)
{
    int fd = open("/dev/note_misc", O_RDONLY), result = 1;
    if (fd < 0) { perror("open"); return 1; }
    if (!read_part(fd, 5, "hello") || !read_part(fd, 64, " from misc\n") ||
        !read_part(fd, 64, ""))
        goto out;
    if (close(fd) < 0) { perror("close"); return 1; }
    fd = open("/dev/note_misc", O_RDONLY);
    if (fd < 0) { perror("reopen"); return 1; }
    if (read_part(fd, 64, "hello from misc\n"))
        result = 0;
out:
    if (close(fd) < 0) { perror("close"); result = 1; }
    return result;
}
```

```bash
# 执行位置：有本机 C11 编译器的目标练习目录。
cc -std=c11 -Wall -Wextra -Werror -pedantic misc_probe.c -o misc_probe
```

这编译的是用户程序，不使用 Kbuild；交叉环境应使用匹配目标用户空间的编译器并传送程序。在目标运行：

```bash
# 执行位置：目标练习目录，note_misc 仍处于加载状态。
sudo ./misc_probe
```

预期四行读取字节数依次是 5、11、0、16，程序还逐字节核对 hello、剩余内容和完整问候。第一次打开的两个非空读取合起来正好 16 字节，第三次为空；新打开又得到完整内容。零字节结果在本服务中表示结尾，不表示设备断线。记录结果后结束本轮：

```bash
# 执行位置：目标会话，探针已退出且没有其他打开者。
sudo rmmod note_misc
```

卸载返回成功后检查模型入口：

```bash
# 执行位置：同一目标会话。
test ! -e /sys/class/misc/note_misc && echo removed
```

出现 removed 表示这个入口已消失；如果曾手工建立节点，其路径可能仍在，不能由路径残留推断服务还在运行。

### 1.5.3\_让打开者阻止普通卸载

最后单独观察模块引用。上一组已经卸载，因此先重新装载；在终端 A 执行以下完整程序，它打开设备后等待回车，并保证退出时关闭描述符：

```bash
# 执行位置：目标终端 A 的匹配产物目录，前一轮已卸载。
sudo insmod ./note_misc.ko
```

加载成功并确认 `/dev/note_misc` 已恢复后，再建立下面的持有者。不要把输入重定向到已经结束的文件，否则 getchar 会立即结束，无法保留观察现场。

保存下面完整程序为 `hold_open.c`。它使用 Linux 用户空间头文件和 C11 编译器，编译后在装有对应教学模块的目标运行：

```c
/* 保留打开文件，供另一个终端观察普通模块卸载的引用限制。 */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    int fd = open("/dev/note_misc", O_RDONLY);
    int result = 0;
    if (fd < 0) { perror("open"); return 1; }
    puts("设备保持打开；现在可在终端 B 尝试普通卸载。");
    puts("按回车关闭设备：");
    if (getchar() == EOF && ferror(stdin)) {
        perror("stdin");
        result = 1;
    }
    if (close(fd) < 0) { perror("close"); result = 1; }
    return result;
}
```

```bash
# 执行位置：有本机 C11 编译器的目标终端 A 练习目录。
cc -std=c11 -Wall -Wextra -Werror -pedantic hold_open.c -o hold_open
```

程序生成后运行，保留 A 的交互输入：

```bash
# 执行位置：目标终端 A 的练习目录。
sudo ./hold_open
```

A 显示保持打开的提示后，不按回车，在同一目标的 B 观察卸载：

```bash
# 执行位置：目标终端 B，A 的持有程序仍在等待。
sudo rmmod note_misc
```

未关闭时应拒绝卸载，因为 file 保存的操作表仍持有模块引用。回到 A 按回车并确认程序结束，再在 B 清理：

```bash
# 执行位置：目标终端 B，A 已关闭文件且没有其他打开者。
sudo rmmod note_misc
```

此时应成功；若仍失败，先检查其他打开者并正常关闭，不能据此直接断言引用泄漏。这里只使用普通卸载，不把强制卸载作为恢复手段。

本例源码按 NXP Linux 6.12.20 固定提交核对；先从[源码索引](../../../research/source_reading/driver_entries/navigation/P01_Linux_6.12_驱动入口源码阅读索引.md)进入，再读[misc 分派的唯一实现](../../../research/source_reading/driver_entries/source_explanations/P01_对象创建与misc分派.md#1.2_misc登记与失败回滚)。读回调使用的公共辅助函数见[simple_read_from_buffer 实现](../../../research/source_reading/character_device/source_explanations/fs/libfs.c.md#1.1_simple_read_from_buffer)。这里列的是预期观察，ARM 语法检查不能替代实际构建、装载和卸载。

## 1.6\_回顾与练习

misc 省去的是每个小服务重复搭建的编号与分派设施。返回数据的规则、每次打开的位置、模块代码寿命和热拔插后的对象寿命仍要分别设计。sysfs 适合本章之前那种小属性；misc 提供一般字符文件操作，不能仅按代码哪份更短来选择。

1. 预测：注册后 minor 不是 255，是否分配失败？
2. 小修改：把问候改成两个汉字和换行，应该用字数还是 sizeof 减一作为字节长度？
3. 排错：移除 owner 后，打开设备期间普通卸载不再可靠受阻。为何这比“读不到问候”更严重？
4. 迁移：把程序用于可拔出的硬件，注销后立即释放包含 miscdevice 的私有结构，旧文件可能在哪里继续引用它？

参考思路：第一题不是，255 是请求标记，应检查返回状态与实际分配值。第二题传输单位是字节，源码编码下的 sizeof 计算数组大小；不能把字符数量当字节数。第三题旧文件保存的回调可能指向已卸载代码。第四题 private_data 或驱动自行保存的实例指针仍可能指向它，须另建旧打开者的生命期协议。

上一篇：[kobject 与属性](../fundamentals/framework_model/kobject讲解.md) · [驱动框架大纲](../fundamentals/framework_model/大纲.md) · 接着观察：[打开、迭代与映射](../file_operations/大纲.md#1.1_沿对象寿命逐步增加约束) · 延伸：[完整字符设备专题](../character_device/大纲.md)。
