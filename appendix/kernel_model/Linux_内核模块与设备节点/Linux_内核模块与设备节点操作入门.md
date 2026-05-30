------

**内容摘抄自GPT。**

# 第 1 章 模块与设备节点基础

在 Linux 内核开发中，驱动模块（`.ko` 文件）和设备节点（`/dev/xxx`）是一对常常让初学者困惑的组合。
 许多同学第一次写好驱动、编译成 `.ko` 文件后，迫不及待地执行：

```bash
sudo insmod demo.ko
```

内核的确打印出了“驱动加载成功”，可当他们尝试：

```bash
echo "123" > /dev/demo
```

却发现什么日志都没有，甚至 `/dev/demo` 压根不存在。于是便会产生疑惑：*为什么我加载了驱动，设备文件却没有呢？*

要解开这个谜团，我们需要从“内核模块”和“设备节点”的关系说起。

------

## 1.1 内核模块的生命周期

一个内核模块是内核功能的扩展单元。它就像一块可以插拔的“乐高积木”，在需要时加载进内核，不需要时卸载出去。

模块的典型操作包括：

- **加载**：

  ```bash
  insmod demo.ko
  ```

  这条命令只是把 `demo.ko` 对应的代码映射进内核地址空间，并调用你在 `module_init()` 里注册的入口函数。

- **卸载**：

  ```bash
  rmmod demo
  ```

  它会调用 `module_exit()` 中定义的清理逻辑，并把模块从内核中移除。

- **查看状态**：

  ```bash
  lsmod | grep demo
  ```

  或者直接查看 `/proc/modules`。

- **调试日志**：
   模块里常用 `printk()` 输出，用户空间可用 `dmesg` 查看。

在这一层，模块只代表“内核里多了一段功能代码”。但是用户要访问驱动，**还需要一个通向它的入口** —— 设备节点。

------

## 1.2 设备号与设备节点

Linux 的设备访问机制是通过“主设备号 + 次设备号”来区分的。
 你可以把它类比为“电话区号 + 分机号”：

- 主设备号（major number）相当于区号，标识是哪一类设备驱动。
- 次设备号（minor number）相当于分机号，用于区分同类驱动下的不同实例。

在驱动中，我们通常这样申请设备号：

```c
ret = alloc_chrdev_region(&devnum, 0, 1, "mychardev");
```

这里 `devnum` 里就保存了主次设备号。例如主设备号可能是 `240`，次设备号是 `0`。

然而，仅有设备号还不够。用户空间无法直接通过数字访问设备，它需要一个 `/dev/xxx` 文件作为入口。这个入口就是 **设备节点**。

------

## 1.3 设备节点的创建

在 Linux 中，字符设备驱动的灵魂在于 **设备号** 与 **设备节点**。驱动注册了设备号，内核才知道有这样一个设备；而用户空间要访问这个设备，则必须通过 `/dev` 下的设备节点。
 因此，理解设备节点的创建方式，是从“模块”走向“驱动”的必经之路。

------

### 1.3.1 设备号的来源

设备号由 **主设备号（major）** 和 **次设备号（minor）** 组成，内核通过二者唯一标识一个设备。

在驱动里，设备号对应的数据类型是 `dev_t`，常见的申请方式有两种：

1. **静态分配（开发者指定设备号）**

   ```c
   #define DEMO_MAJOR 200
   #define DEMO_MINOR 0
   
   dev_t devnum = MKDEV(DEMO_MAJOR, DEMO_MINOR);
   ret = register_chrdev_region(devnum, 1, "demo");
   ```

   这里我们直接写死了主设备号 200，次设备号从 0 开始。
    这种方式的好处是 **设备号固定**，便于用户空间程序直接访问 `/dev/demo`。
    缺点是：如果 200 已被别的驱动占用，就会注册失败。

2. **动态分配（内核随机分配）**

   ```c
   ret = alloc_chrdev_region(&devnum, 0, 1, "demo");
   ```

   在这种方式下，内核会自动分配一个空闲的主设备号。
    结果保存在 `devnum` 里，主次设备号可通过 `MAJOR(devnum)` 和 `MINOR(devnum)` 提取。
    好处是避免冲突，缺点是用户无法事先知道 major 值，需要依赖内核日志或 `udev` 来创建设备节点。

------

### 1.3.2 手工创建设备节点：mknod

驱动虽然注册了设备号，但 `/dev` 下默认不会有对应节点。
 我们可以用 `mknod` 命令来手工创建：

```bash
sudo mknod /dev/demo c <major> <minor>
sudo chmod 666 /dev/demo
```

其中：

- `c` 表示字符设备；
- `<major>` 必须等于驱动注册的主设备号；
- `<minor>` 必须等于驱动注册的次设备号。

> 举例：如果驱动里静态注册了 `(200, 0)`，则命令为：
>
> ```bash
> sudo mknod /dev/demo c 200 0
> ```

如果使用动态分配，则必须先查看日志：

```bash
dmesg | tail
# demo: loaded, major=240
```

然后再执行：

```bash
sudo mknod /dev/demo c 240 0
```

这就是 **mknod 参数和驱动代码的直接对应关系**。

------

### 1.3.3 自动创建设备节点：class + device

在现代 Linux 中，更推荐使用自动创建机制：
 在驱动里调用 `class_create()` 和 `device_create()`，即可在加载时由 `devtmpfs`/`udev` 自动生成节点：

```c
cls = class_create(THIS_MODULE, "demo_class");
device_create(cls, NULL, devnum, NULL, "demo");
```

这样，加载模块时 `/dev/demo` 会自动出现，卸载模块时节点会自动清理。
 用户无需手工 mknod，大大提高了可用性。

------

### 1.3.4 两种方式的比较

| 方式                           | 优点                   | 缺点                         | 适用场景                 |
| ------------------------------ | ---------------------- | ---------------------------- | ------------------------ |
| **静态设备号 + 手工 mknod**    | 设备号固定，简单直观   | 可能冲突，需要人工创建节点   | 嵌入式系统、教学实验     |
| **动态设备号 + device_create** | 避免冲突，自动生成节点 | major 不固定，调试时需看日志 | 桌面 Linux，现代驱动开发 |

------

### 1.3.5 小结

设备号是驱动在内核中的“身份证”，而设备节点则是用户空间访问它的“入口”。

- **静态分配**强调确定性，但要开发者自己避开冲突；
- **动态分配**强调灵活性，但需要借助 `udev` 或 `device_create()` 生成节点。

无论哪种方式，`mknod` 的三个核心参数（类型、主设备号、次设备号）都与驱动中的 `dev_t` 成员一一对应，这一点一定要牢牢记住。

------

✅ 这样改写后的 **1.3 节** 就既有概念，又有代码，又有命令，还兼顾了静态/动态两种情况，并且明确了 `mknod` 与驱动成员之间的关系。

要不要我帮你把 **1.1 ~ 1.3 节** 整合成一个“书籍式的完整第一章”，这样你后面复习的时候就能直接拿来当教材用了？

------

## 1.4 模块与设备节点的关系

现在我们可以清楚地看到：

- `insmod demo.ko` 只会把驱动逻辑加载进内核，**不会自动有 `/dev/demo`**。
- 没有设备节点时，你往 `/dev/demo` 写，其实只是在普通文件 `/dev/demo` 上写数据，根本没触发驱动。
- 只有当 `/dev/demo` 是字符设备节点，并且 (major, minor) 对应你的驱动注册的设备号时，写操作才会进入驱动的 `my_write()`，读操作才会进入 `my_read()`。

------

✍️ 这一章我们从“为什么 `insmod` 后没有 `/dev/demo`”出发，讲清楚了模块和设备节点的区别与联系。下一章我可以继续写一份 **实验流程**：一步步加载模块 → 创建设备节点 → `echo/cat` 验证 → 对应的驱动代码走向。

------

# 第 2 章 实验流程：从 insmod 到设备访问

本章我们通过一个最小字符设备驱动，演示完整的加载、创建设备节点、读写验证的流程。这样可以把前一章的概念变成实操体验。

------

## 2.1 准备驱动代码

先写一个最简版的字符设备驱动 `demo.c`：

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

static dev_t devnum;
static struct cdev my_cdev;
static struct class *cls;

static int my_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "demo: open()\n");
    return 0;
}

static int my_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "demo: release()\n");
    return 0;
}

static ssize_t my_read(struct file *file, char __user *buf,
                       size_t count, loff_t *ppos) {
    printk(KERN_INFO "demo: read()\n");
    return 0;
}

static ssize_t my_write(struct file *file, const char __user *buf,
                        size_t count, loff_t *ppos) {
    printk(KERN_INFO "demo: write(), count=%zu\n", count);
    return count;
}

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_release,
    .read    = my_read,
    .write   = my_write,
};

static int __init my_init(void) {
    int ret;

    /* 1. 申请设备号 */
    ret = alloc_chrdev_region(&devnum, 0, 1, "demo");
    if (ret < 0) return ret;

    /* 2. 注册 cdev */
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;
    ret = cdev_add(&my_cdev, devnum, 1);
    if (ret < 0) {
        unregister_chrdev_region(devnum, 1);
        return ret;
    }

    /* 3. 创建设备节点 */
    cls = class_create(THIS_MODULE, "demo_class");
    if (IS_ERR(cls)) {
        cdev_del(&my_cdev);
        unregister_chrdev_region(devnum, 1);
        return PTR_ERR(cls);
    }
    device_create(cls, NULL, devnum, NULL, "demo");  // → /dev/demo

    printk(KERN_INFO "demo: loaded, major=%d\n", MAJOR(devnum));
    return 0;
}

static void __exit my_exit(void) {
    device_destroy(cls, devnum);
    class_destroy(cls);
    cdev_del(&my_cdev);
    unregister_chrdev_region(devnum, 1);
    printk(KERN_INFO "demo: unloaded\n");
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
```

> 和你上一版的区别是：这里用 `class_create + device_create`，模块加载时会自动生成 `/dev/demo` 节点；卸载时自动删除。

------

## 2.2 编译驱动模块

编写 `Makefile`：

```make
obj-m += demo.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```

执行：

```bash
make
```

成功后会得到 `demo.ko`。

------

## 2.3 加载模块

加载模块：

```bash
sudo insmod demo.ko
```

查看日志：

```bash
dmesg | tail
```

会看到类似：

```
demo: loaded, major=240
```

确认设备节点：

```bash
ls -l /dev/demo
# crw------- 1 root root 240, 0 ...
```

注意这里第一列是 `c`，说明它是字符设备节点。

------

## 2.4 读写验证

写入：

```bash
echo "hello" > /dev/demo
```

查看内核日志：

```bash
dmesg | tail
# 会看到：demo: write(), count=6
```

读取：

```bash
cat /dev/demo
```

查看内核日志：

```bash
dmesg | tail
# 会看到：demo: read()
```

由于我们 `my_read()` 直接返回 0，`cat` 不会输出内容，但日志能证明已经进入驱动。

------

## 2.5 卸载模块

卸载：

```bash
sudo rmmod demo
```

查看日志：

```bash
dmesg | tail
# demo: unloaded
```

同时 `/dev/demo` 节点会被自动删除。

------

## 2.6 小结

通过这个实验我们完整走了一遍流程：

1. 编译驱动得到 `.ko` 模块
2. `insmod` 加载模块 → 驱动注册字符设备
3. `device_create` 自动生成 `/dev/demo` 节点
4. 通过 `echo/cat` 与节点交互 → 驱动函数被调用
5. `rmmod` 卸载 → 节点消失

这就把“模块”与“设备节点”两个概念清晰地连了起来。

------

👉 下一章，我可以帮你写 **常见错误与排查**，比如：

- 为什么 `/dev/demo` 没有出现？
- 为什么写入没触发驱动？
- 为什么卸载失败（device busy）？

要不要我继续？

好👌 那我们继续第三章，把常见问题和排查方法写出来，延续前面教材化的风格。

------

# 第 3 章 常见问题与排查

虽然我们在第二章已经能够顺利加载模块、自动生成 `/dev/demo` 并进行读写，但在实际开发中，驱动新手往往会遇到各种困惑。本章我们就像“常见疑难解答”一样，总结几个高频问题，并给出排查思路。

------

## 3.1 为什么 `insmod demo.ko` 后没有 `/dev/demo`？

**原因 1：驱动代码里没有 `device_create()`**

- 如果你的模块只做了 `alloc_chrdev_region + cdev_add`，而没有调用 `class_create` / `device_create`，那么内核只知道设备号，却没有告诉用户空间要创建设备节点。
- 结果就是：`insmod` 成功，但 `/dev` 下没有入口。

**解决办法：**

- 手工创建：

  ```bash
  sudo mknod /dev/demo c <major> 0
  sudo chmod 666 /dev/demo
  ```

- 或在驱动里加上自动创建设备节点的逻辑（第二章给的 `device_create()` 方法）。

------

## 3.2 为什么 `echo "111" > /dev/demo` 没有任何日志？

**可能情况：**

1. `/dev/demo` 不是字符设备节点，而是一个普通文件。

   - 你可能在之前手工 `touch /dev/demo` 过。

   - 检查方法：

     ```bash
     ls -l /dev/demo
     ```

     如果第一列是 `-` 而不是 `c`，那就是普通文件。

2. 节点的设备号与驱动注册的不一致。

   - 比如你的驱动申请到的 major 是 240，但你用 `mknod` 建的是 241。
   - 这时候写入的请求就不会进入驱动。

**解决办法：**

- 确认 `/dev/demo` 是 `c` 开头。
- 确认 `ls -l /dev/demo` 打印的 `(major, minor)` 与 `dmesg` 里驱动申请到的完全一致。

------

## 3.3 为什么 `cat /dev/demo` 会打印出 “111”？

这其实是一个 **典型的误会**。

- 如果 `/dev/demo` 是普通文件，那么 `echo "111" > /dev/demo` 只是往这个文件写入了内容。
- 当你再执行 `cat /dev/demo`，它当然会打印出 "111"，但这和你的驱动毫无关系。
- 这正是你在一开始遇到的现象。

**教训**：一定要确认 `/dev/demo` 是“字符设备”，而不是普通文件。

------

## 3.4 为什么卸载模块 `rmmod demo` 失败，提示 “Device or resource busy”？

**原因：**

- 有进程仍然打开着 `/dev/demo`。
- 内核引用计数没有归零。

**解决办法：**

- 找到占用进程：

  ```bash
  fuser /dev/demo
  ```

  或者：

  ```bash
  lsof /dev/demo
  ```

- 杀掉对应进程，再执行 `rmmod`。

------

## 3.5 为什么 `rmmod demo` 后 `/dev/demo` 节点还在？

**原因：**

- 你手工用 `mknod` 创建的节点，和驱动卸载时的生命周期没有关系。它就是个普通的文件系统节点，除非手工删除，否则不会消失。

**解决办法：**

- 手工删除：

  ```bash
  sudo rm -f /dev/demo
  ```

- 如果是 `device_create()` 自动生成的节点，就会随着 `device_destroy()` 在 `my_exit()` 中被清理掉。

------

## 3.6 为什么卸载再加载，会报 “File exists”？

**原因：**

- 你没有在 `my_exit()` 里正确清理资源。
- 常见错误是 `cdev_del()`、`unregister_chrdev_region()`、`device_destroy()`、`class_destroy()` 漏掉了。
- 上一次的设备号还占着，下一次再 `insmod` 时就冲突了。

**解决办法：**

- 在退出函数 `my_exit()` 里对称清理所有分配的资源。
- 确认卸载日志里有 `"demo: unloaded"`，再 `insmod`。

------

## 3.7 为什么 `modinfo demo.ko` 报错？

如果你在目标板上执行 `modinfo demo.ko`，报错：

```
modinfo: can't open '/lib/modules/6.1.xx/modules.dep'
```

**原因：**

- 板子根文件系统没有 `/lib/modules/<uname -r>/` 目录，也没有生成 `modules.dep`。
- `modinfo` 默认会查这里。

**解决办法：**

- 用绝对路径：

  ```bash
  modinfo ./demo.ko
  ```

  这样直接解析 ELF 里的信息，不依赖 `modules.dep`。

- 如果希望 `modinfo demo` 这种形式也能用，需要在宿主机 `make modules_install INSTALL_MOD_PATH=<rootfs>` 并执行 `depmod -b`，把 `/lib/modules/<uname -r>/` 拷到目标板。

------

✍️ 本章我们从实际操作中常遇到的坑出发，讲了 **节点不存在、节点错建、驱动未清理、设备被占用** 等问题的成因和解决方法。

------

👉 下一章我可以写 **一个完整复盘示例**：从编译 demo.ko，到 insmod、echo/cat、dmesg，再到 rmmod 清理，全过程像做实验报告一样串起来。你要我继续吗？

好👌 那我们写 **第 4 章：完整实验复盘**。这一章就像一本教材里的“实验报告”，带你从零走一遍：编译、加载、交互、卸载、清理。

------

# 第 4 章 完整实验复盘

在这一章，我们将把前面讲解的内容串成一条完整实验路线。你可以在开发板或虚拟机上按步骤操作，并对照日志，验证驱动和设备节点的行为。

------

## 4.1 准备工作

1. **源文件：`demo.c`**

   使用我们在第二章写好的字符设备驱动（带自动创建设备节点功能）：

   ```c
   #include <linux/init.h>
   #include <linux/module.h>
   #include <linux/fs.h>
   #include <linux/cdev.h>
   #include <linux/device.h>
   #include <linux/uaccess.h>
   
   static dev_t devnum;
   static struct cdev my_cdev;
   static struct class *cls;
   
   static int my_open(struct inode *inode, struct file *file) {
       printk(KERN_INFO "demo: open()\n");
       return 0;
   }
   
   static int my_release(struct inode *inode, struct file *file) {
       printk(KERN_INFO "demo: release()\n");
       return 0;
   }
   
   static ssize_t my_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos) {
       printk(KERN_INFO "demo: read()\n");
       return 0;
   }
   
   static ssize_t my_write(struct file *file, const char __user *buf,
                           size_t count, loff_t *ppos) {
       printk(KERN_INFO "demo: write(), count=%zu\n", count);
       return count;
   }
   
   static struct file_operations fops = {
       .owner   = THIS_MODULE,
       .open    = my_open,
       .release = my_release,
       .read    = my_read,
       .write   = my_write,
   };
   
   static int __init my_init(void) {
       int ret;
   
       ret = alloc_chrdev_region(&devnum, 0, 1, "demo");
       if (ret < 0) return ret;
   
       cdev_init(&my_cdev, &fops);
       ret = cdev_add(&my_cdev, devnum, 1);
       if (ret < 0) {
           unregister_chrdev_region(devnum, 1);
           return ret;
       }
   
       cls = class_create(THIS_MODULE, "demo_class");
       if (IS_ERR(cls)) {
           cdev_del(&my_cdev);
           unregister_chrdev_region(devnum, 1);
           return PTR_ERR(cls);
       }
   
       device_create(cls, NULL, devnum, NULL, "demo"); // 自动创建设备节点
       printk(KERN_INFO "demo: loaded, major=%d\n", MAJOR(devnum));
       return 0;
   }
   
   static void __exit my_exit(void) {
       device_destroy(cls, devnum);
       class_destroy(cls);
       cdev_del(&my_cdev);
       unregister_chrdev_region(devnum, 1);
       printk(KERN_INFO "demo: unloaded\n");
   }
   
   module_init(my_init);
   module_exit(my_exit);
   MODULE_LICENSE("GPL");
   ```

2. **Makefile**

   ```make
   obj-m += demo.o
   
   all:
   	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
   
   clean:
   	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
   ```

3. **编译**

   ```bash
   make
   ```

   成功后会得到 `demo.ko`。

------

## 4.2 加载模块

```bash
sudo insmod demo.ko
```

检查日志：

```bash
dmesg | tail
```

输出示例：

```
[ 1234.567890] demo: loaded, major=240
```

------

## 4.3 确认设备节点

```bash
ls -l /dev/demo
```

输出示例：

```
crw------- 1 root root 240, 0 Jan  1 00:00 /dev/demo
```

- `c` → 字符设备
- `240,0` → 主设备号 240，次设备号 0

说明节点已经自动创建成功。

------

## 4.4 写入测试

```bash
echo "hello" > /dev/demo
```

查看内核日志：

```bash
dmesg | tail
```

输出示例：

```
[ 1235.123456] demo: write(), count=6
```

这说明写入操作成功进入了驱动的 `my_write()`。

------

## 4.5 读取测试

```bash
cat /dev/demo
```

查看日志：

```bash
dmesg | tail
```

输出示例：

```
[ 1236.654321] demo: read()
```

由于 `my_read()` 返回 0，`cat` 会立刻结束，但日志证明驱动确实被调用。

------

## 4.6 卸载模块

```bash
sudo rmmod demo
```

查看日志：

```bash
dmesg | tail
```

输出示例：

```
[ 1237.987654] demo: unloaded
```

再次检查 `/dev/demo`：

```bash
ls -l /dev/demo
```

会提示：

```
ls: cannot access '/dev/demo': No such file or directory
```

说明节点已随模块卸载被自动清理。

------

## 4.7 实验总结

通过这次实验，你已经完成了一个**完整的生命周期**：

1. 编译驱动 → 得到 `demo.ko`
2. `insmod` → 驱动注册字符设备
3. 自动生成 `/dev/demo` → 用户空间可访问
4. `echo/cat` → 分别触发写/读函数
5. `rmmod` → 驱动退出，节点清理

这条链路将“模块”与“设备节点”牢牢地串联了起来。以后当你在实际项目中遇到“为什么节点没出现”“为什么写入没触发驱动”时，就能有条不紊地去定位问题。

------

✍️ 到这里，我们已经把最小实验完整复盘了。
 👉 下一章我可以写 **扩展实验**：比如给驱动加一个内存缓冲区，真正实现读写数据（`echo` 写入什么，`cat` 就能读出来），这样驱动就不仅仅是日志了。

要不要我继续？

好👌 那我们进入 **第 5 章：扩展实验**，让驱动真正能保存并返回数据，而不仅仅是打印日志。这样你用 `echo` 写入的内容，再用 `cat` 就能读出来，设备行为就更像一个“真正的文件”。

------

# 第 5 章 扩展实验：实现内存缓冲区

在前面的实验里，我们的 `my_read()` 总是返回 0（EOF），`my_write()` 只是打印日志。虽然能证明驱动逻辑没问题，但并没有形成数据交互。
 本章我们将为字符设备增加一个内存缓冲区，完成“写入-读取”的闭环。

------

## 5.1 驱动代码修改

在 `demo.c` 里，增加一个静态缓冲区和长度记录：

```c
#define BUF_SIZE 1024

static char device_buf[BUF_SIZE];
static size_t data_size = 0;  // 缓冲区里已有数据大小
```

修改 `my_read()` 和 `my_write()`：

```c
static ssize_t my_read(struct file *file, char __user *buf,
                       size_t count, loff_t *ppos) {
    size_t to_copy;

    printk(KERN_INFO "demo: read(), count=%zu, ppos=%lld\n", count, *ppos);

    if (*ppos >= data_size) {
        return 0;  // 已经读到结尾
    }

    to_copy = min(count, data_size - *ppos);

    if (copy_to_user(buf, device_buf + *ppos, to_copy)) {
        return -EFAULT;
    }

    *ppos += to_copy;
    return to_copy;
}

static ssize_t my_write(struct file *file, const char __user *buf,
                        size_t count, loff_t *ppos) {
    size_t to_copy;

    printk(KERN_INFO "demo: write(), count=%zu\n", count);

    to_copy = min(count, BUF_SIZE);

    if (copy_from_user(device_buf, buf, to_copy)) {
        return -EFAULT;
    }

    data_size = to_copy;  // 更新已写入的数据长度
    return to_copy;
}
```

现在：

- 写入时，数据会被保存到 `device_buf`。
- 读取时，返回缓冲区的内容，并支持多次分块读取。

------

## 5.2 重新编译并加载

```bash
make clean && make
sudo insmod demo.ko
```

确认节点：

```bash
ls -l /dev/demo
```

------

## 5.3 写入数据

```bash
echo "Hello Kernel" > /dev/demo
```

查看日志：

```bash
dmesg | tail
```

输出：

```
demo: write(), count=13
```

------

## 5.4 读取数据

```bash
cat /dev/demo
```

屏幕输出：

```
Hello Kernel
```

日志：

```
demo: read(), count=65536, ppos=0
```

说明数据被正确保存和返回。

------

## 5.5 多次写入与覆盖

再次写入：

```bash
echo "Second Line" > /dev/demo
cat /dev/demo
```

输出变为：

```
Second Line
```

日志显示：

```
demo: write(), count=12
demo: read(), count=65536, ppos=0
```

说明写入操作覆盖了原有缓冲区。

------

## 5.6 卸载与清理

```bash
sudo rmmod demo
```

日志：

```
demo: unloaded
```

节点 `/dev/demo` 消失。

------

## 5.7 小结

通过增加一个缓冲区，我们把最初只能“打印日志”的 demo 驱动，扩展成了一个真正能“存取数据”的字符设备：

- `echo` 写入 → 数据进入内核缓冲区。
- `cat` 读取 → 数据从缓冲区返回到用户空间。

这就是一个最小的“内存型字符设备”的雏形。

------

👉 下一章，我可以帮你写 **更进一步的扩展**：比如

- 支持多进程并发访问（加锁 `mutex`），
- 支持 `ioctl` 实现控制命令，
- 甚至挂接到 sysfs，导出属性。

要不要我继续写 **第 6 章：并发与锁**？

好👌 那我们进入 **第 6 章：并发与锁**。在前一章，我们的字符设备已经能存取数据，但它还是“单线程思维”，如果多个进程同时访问，就可能出现数据错乱。本章我们将讨论并发访问带来的问题，并通过 **互斥锁（mutex）** 解决。

------

# 第 6 章 并发与锁

## 6.1 为什么要考虑并发？

Linux 是一个多任务操作系统。想象这样一个场景：

1. 进程 A 执行 `echo "AAAA" > /dev/demo`，正在往驱动写数据；
2. 此时进程 B 执行 `echo "BBBB" > /dev/demo`，也在写数据；
3. 如果驱动里没有同步机制，两份数据就可能在缓冲区里交错，结果既不是 "AAAA" 也不是 "BBBB"，而是一团乱码。

这就是典型的**竞争条件（race condition）**。

类似地，读操作也可能在写操作还没完成时就被触发，从而读到半成品数据。

------

## 6.2 内核同步原语

Linux 内核为并发访问提供了多种同步机制：

- **spinlock**：自旋锁，适用于短时间的原子操作（不可睡眠）。
- **mutex**：互斥锁，适用于需要睡眠的场景（比如字符设备的读写）。
- **semaphore**：信号量，早期常用，现在更推荐 `mutex`。
- **rwlock**：读写锁，适合读多写少的情况。

对于字符设备的读写，最常见的做法是用 **mutex**。

------

## 6.3 在 demo 驱动中加入互斥锁

在 `demo.c` 中加入一个全局的 `struct mutex`：

```c
#include <linux/mutex.h>

#define BUF_SIZE 1024

static char device_buf[BUF_SIZE];
static size_t data_size = 0;
static DEFINE_MUTEX(demo_mutex);  // 定义并初始化互斥锁
```

修改 `my_read()`：

```c
static ssize_t my_read(struct file *file, char __user *buf,
                       size_t count, loff_t *ppos) {
    size_t to_copy;

    if (mutex_lock_interruptible(&demo_mutex))
        return -ERESTARTSYS;

    printk(KERN_INFO "demo: read(), count=%zu, ppos=%lld\n", count, *ppos);

    if (*ppos >= data_size) {
        mutex_unlock(&demo_mutex);
        return 0;
    }

    to_copy = min(count, data_size - *ppos);

    if (copy_to_user(buf, device_buf + *ppos, to_copy)) {
        mutex_unlock(&demo_mutex);
        return -EFAULT;
    }

    *ppos += to_copy;
    mutex_unlock(&demo_mutex);
    return to_copy;
}
```

修改 `my_write()`：

```c
static ssize_t my_write(struct file *file, const char __user *buf,
                        size_t count, loff_t *ppos) {
    size_t to_copy;

    if (mutex_lock_interruptible(&demo_mutex))
        return -ERESTARTSYS;

    printk(KERN_INFO "demo: write(), count=%zu\n", count);

    to_copy = min(count, BUF_SIZE);

    if (copy_from_user(device_buf, buf, to_copy)) {
        mutex_unlock(&demo_mutex);
        return -EFAULT;
    }

    data_size = to_copy;

    mutex_unlock(&demo_mutex);
    return to_copy;
}
```

------

## 6.4 测试并发场景

1. 编译并加载新模块：

   ```bash
   make clean && make
   sudo insmod demo.ko
   ```

2. 打开两个终端，同时执行：

   ```bash
   echo "AAAA" > /dev/demo
   echo "BBBB" > /dev/demo
   ```

3. 再读取：

   ```bash
   cat /dev/demo
   ```

结果要么是 `AAAA`，要么是 `BBBB`，不会出现 `ABAB` 之类的乱序。

日志：

```bash
demo: write(), count=5
demo: write(), count=5
demo: read(), count=65536, ppos=0
```

说明互斥锁正确保护了读写操作。

------

## 6.5 小结

通过引入 `mutex`，我们解决了并发访问导致的数据错乱问题。这里有几点要特别注意：

- 使用 `mutex_lock_interruptible()`，避免进程在等待锁时无法响应信号；
- 每一个加锁的地方必须对应一次 `mutex_unlock()`；
- 锁的粒度要适度，既要保证数据一致性，又不要过度影响性能。

------

✍️ 本章我们让 demo 驱动从“单线程玩具”升级成了“多进程安全”的字符设备。

