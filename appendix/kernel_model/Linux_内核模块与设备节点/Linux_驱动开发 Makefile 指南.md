**注意**：以下内容出自GPT。



# 第 1 章 走进驱动的 Makefile

## 为什么它和应用程序不一样？

写过用户态应用的同学都知道：应用的 Makefile 通常很啰嗦。
 你得告诉 `gcc`：源文件在哪、库路径在哪、要链接哪些库，最后要生成什么可执行文件。

可一旦进入驱动世界，情况大变。你打开别人的驱动目录，看到的 Makefile 可能只有寥寥几行：

```makefile
obj-m := mydriver.o
KDIR  := /lib/modules/$(shell uname -r)/build
PWD   := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
```

是不是有点惊讶？看起来几乎没做什么，却能编译出 `.ko` 模块。
 原因就在于：驱动的编译并不是我们直接去调用 `gcc`，而是交给 **内核的 Kbuild 系统**来完成。
 我们写的 Makefile，只是把“我要编译哪些源文件”的需求告诉 Kbuild，剩下的事情都由内核顶层的 Makefile 接管。

------

## 那么，`KDIR` 究竟是什么？

很多初学者一开始都会搞糊涂：`KDIR` 到底是 **宿主机的内核目录**，还是 **目标板的内核目录**？

答案是：**永远指向宿主机上的那份内核源码树**。
 区别只是：

- 如果你在 PC 上编 PC 的驱动，那就写：

  ```makefile
  KDIR := /lib/modules/$(shell uname -r)/build
  ```

  它会自动找到宿主机当前正在运行的内核头文件。

- 如果你是嵌入式交叉编译，就要让 `KDIR` 指向你在宿主机准备好的 **目标板内核源码**。

很显然，我们是嵌入式，采用第二种情况。

比如你现在的环境里，内核源码路径是：

```
/home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
```

那么你的 Makefile 就要写成：

```makefile
obj-m := mydriver.o

# 指向宿主机上的 i.MX6ULL 内核源码
KDIR := /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- clean
```

这里多了两个关键字：

- `ARCH=arm` —— 告诉内核我们要编译的是 ARM 架构的模块；
- `CROSS_COMPILE=arm-none-linux-gnueabihf-` —— 使用交叉工具链，而不是宿主机的 gcc。

这样编译出来的 `.ko`，才能放到目标板上去运行。

------

## 编译之前的准备工作

在 `KDIR` 指向的内核目录里，还需要做一些准备工作：

```bash
cd /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1

# 确保 .config 和目标板一致
# 可以从目标板导出 /proc/config.gz
zcat /proc/config.gz > .config

# 让内核准备好模块编译需要的头文件和符号
make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- olddefconfig
make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- prepare modules_prepare
```

这两步很重要，否则在编译模块时可能会遇到 “符号未定义”、“头文件缺失” 之类的报错。

------

## 编译与加载

准备好之后，在你的驱动目录里：

```bash
make        # 生成 mydriver.ko
make clean  # 清理
```

把生成的 `mydriver.ko` 拷贝到目标板上：

```bash
scp mydriver.ko root@目标板IP:/tmp/
```

然后在目标板上加载：

```bash
insmod /tmp/mydriver.ko
```

如果成功，你会在 `dmesg` 里看到对应的日志输出。

------

## 本章小结

- 驱动的 Makefile 之所以简洁，是因为真正的编译由内核的 Kbuild 完成；
- `KDIR` 永远是宿主机上的内核源码路径，交叉编译时要指向目标板的那份源码树；
- 嵌入式开发时，记得在编译模块前先 `prepare` 和 `modules_prepare`；
- 最终生成的 `.ko` 文件，才是能在目标板上被 `insmod` 的模块。

------

# 第 2 章 从单文件到多文件：Makefile 的演进

在上一章里，我们用一个最小的 Makefile 就编译出了 `mydriver.ko`。
 那只是最简单的情况：一个 `.c` 文件对应一个 `.ko` 模块。

可实际开发时，情况往往复杂得多：

- 可能驱动逻辑拆分在多个 `.c` 文件里；
- 也可能一个目录里放了好几个不同的驱动，每个都要编译成独立的 `.ko`。

这一章我们就来看看 Makefile 在不同场景下是如何演进的。

------

## 2.1 单文件驱动（回顾）

目录结构：

```
mydriver/
├── Makefile
└── mydriver.c
```

Makefile 内容（适配你的环境 `/home/lizhaojun/linux/nxp/kernel/linux-imx-6.1`）：

```makefile
# 单文件驱动：mydriver.c -> mydriver.ko
obj-m := mydriver.o

KDIR := /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- clean
```

这就是我们上一章的最终形态。

------

## 2.2 多文件合并成一个模块

假设你把驱动分成了三个文件：

- `mydriver_main.c` —— 主入口
- `foo.c` —— 辅助功能
- `bar.c` —— 另一个辅助功能

你希望它们一起编译，最终仍然只生成一个 `mydriver.ko`。

目录结构：

```
mydriver/
├── Makefile
├── mydriver_main.c
├── foo.c
└── bar.c
```

Makefile 写法：

```makefile
# 模块名：mydriver.ko
obj-m := mydriver.o

# mydriver.ko 由多个 .o 组合而成
# 注意这里的写法是 "模块名-objs"
mydriver-objs := mydriver_main.o foo.o bar.o

KDIR := /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- clean
```

最终会生成一个 **单一的 `mydriver.ko`**，里面打包了 `mydriver_main.o`、`foo.o`、`bar.o`。

------

## 2.3 多个独立模块

再进一步：假设你写了两个驱动，`foo.c` 和 `bar.c`，希望分别编译成 `foo.ko` 和 `bar.ko`，而不是一个大模块。

目录结构：

```
drivers/
├── Makefile
├── foo.c
└── bar.c
```

Makefile 写法：

```makefile
# 同时生成两个模块：foo.ko 和 bar.ko
obj-m := foo.o bar.o

KDIR := /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- clean
```

这样一次编译，就会在目录下看到：

```
foo.ko
bar.ko
```

------

## 2.4 小结

- **单文件驱动**：最常见的入门案例，直接 `obj-m := mydriver.o`。
- **多文件合并成一个模块**：用 `模块名-objs := file1.o file2.o ...`。
- **多个独立模块**：在 `obj-m` 里一次性列出多个 `.o`，每个都会生成对应的 `.ko`。

从这里可以看出：

> **驱动 Makefile 的精髓在于“声明”，而不是写编译命令。**
>  Kbuild 会自动识别并完成剩下的工作。

------

# 第 3 章 把驱动集成进内核源码树

在前两章里，我们的驱动都是**外部模块**（out-of-tree module）。
 它们像插件一样，单独编译成 `.ko` 文件，再手动用 `insmod` 或 `modprobe` 加载。

这种方式非常灵活，适合调试和开发。
 但在一些场景下，你可能会希望驱动**直接编译进内核源码树**，与内核一起构建：

- 产品固件需要内核 + 驱动一次性编译打包；
- 驱动需要 `y`（内建）或 `m`（模块）选项来切换；
- 驱动开发进入“产品化阶段”，要让它被 `make menuconfig` 管理。

这一章，我们就来看看 **如何把驱动放进内核源码树**。

------

## 3.1 内核源码树中的目录结构

假设我们要把驱动放在源码树的：

```
drivers/mydevice/
```

目录结构大概是这样：

```
linux-imx-6.1/
├── drivers/
│   ├── mydevice/
│   │   ├── Makefile
│   │   ├── Kconfig
│   │   ├── mydriver_main.c
│   │   ├── foo.c
│   │   └── bar.c
```

------

## 3.2 内核树内的 Makefile

在这个目录下，我们写的 Makefile 和外部模块有些不同：

```makefile
# 内核树内的写法，不再使用 obj-m
# CONFIG_MYDEVICE 是我们在 Kconfig 里定义的配置开关
# y  -> 编译进内核
# m  -> 编译成模块 .ko
# n  -> 不编译
obj-$(CONFIG_MYDEVICE) += mydevice.o

# 如果 mydevice.ko 由多个源文件组成，就用 xxx-objs 方式列出来
mydevice-objs := mydriver_main.o foo.o bar.o
```

这里最关键的是：

- `obj-m` 适用于外部模块；
- `obj-$(CONFIG_xxx)` 适用于内核源码树内的模块。

------

## 3.3 配套的 Kconfig

为了让这个 `CONFIG_MYDEVICE` 出现在 `make menuconfig` 里，我们要写一个 `Kconfig` 文件：

```Kconfig
config MYDEVICE
	tristate "My Device Driver support"
	help
	  This is a sample driver for My Device.
	  Say Y here to compile it into the kernel,
	  or M to build it as a module,
	  or N to exclude it.
```

解释：

- `tristate` 表示三态选项：`y/m/n`。
- `help` 段会显示在 menuconfig 里，作为帮助说明。

------

## 3.4 把 Kconfig 接到主菜单里

单独写了 `drivers/mydevice/Kconfig` 还不够，我们要在上一级目录（比如 `drivers/Kconfig`）里把它“挂上去”：

在 `drivers/Kconfig` 里加一行：

```Kconfig
source "drivers/mydevice/Kconfig"
```

这样在 `make menuconfig` 里，你就能看到新的一栏 `My Device Driver support`。

------

## 3.5 使用 menuconfig 选择

当你在 `make menuconfig` 里操作时：

- 选 `Y`：编译进内核镜像；
- 选 `M`：生成 `.ko` 模块；
- 选 `N`：不编译。

Kbuild 会根据这个选项，自动控制 Makefile 里的 `obj-$(CONFIG_MYDEVICE)`。

------

## 3.6 小结

- **外部模块**：灵活，适合开发调试，Makefile 用 `obj-m`。
- **源码树内模块**：规范，适合产品化，Makefile 用 `obj-$(CONFIG_xxx)`，配合 `Kconfig`。
- 内核的 `menuconfig` 就是靠这种 `Kconfig + obj-$(CONFIG_xxx)` 的机制，把成千上万个驱动统一管理起来的。

------

# 第 4 章 常见问题与排查技巧

驱动编译、加载过程中，常常会遇到一些“看似玄学”的报错：

- 符号找不到？
- vermagic 不匹配？
- 内核头文件缺失？

这一章我们把常见问题逐一梳理，并给出排查思路。

------

## 4.1 “Unknown symbol” 报错

**现象：**
 在目标板上执行 `insmod mydriver.ko` 后，`dmesg` 打印：

```
mydriver: Unknown symbol xxx (err 0)
```

**原因：**

- 模块调用了内核里没有导出的符号（比如内核函数没用 `EXPORT_SYMBOL` 或 `EXPORT_SYMBOL_GPL` 暴露）。
- 你的模块和目标板内核不完全匹配，符号表对不上。

**解决思路：**

1. 确认符号是否被导出：

   ```bash
   grep xxx /proc/kallsyms
   ```

   找不到说明内核没导出，驱动就不能直接用。

2. 确认内核源码和目标板内核一致：版本号、配置、编译器必须相同。

3. 如果是你自己的函数，记得在源码里加：

   ```c
   EXPORT_SYMBOL(my_func);
   ```

------

## 4.2 “Invalid module format” / vermagic 不匹配

**现象：**
 `insmod` 时提示：

```
mydriver.ko: invalid module format
```

`dmesg` 打印：

```
vermagic: 5.10.72 SMP mod_unload ARMv7 p2v8 … 
```

**原因：**
 `vermagic` 是内核为每个模块生成的“版本魔数”，里面包含：

- 内核版本号
- SMP/Preempt 配置
- 编译器版本

如果 `.ko` 的 vermagic 和目标板正在运行的内核 vermagic 不一致，就会拒绝加载。

**解决思路：**

1. 在目标板查看运行内核的 vermagic：

   ```bash
   modinfo /lib/modules/$(uname -r)/kernel/drivers/xxx/some.ko | grep vermagic
   ```

2. 在宿主机编译时，确保 `KDIR` 指向的源码与目标板内核完全一致：

   - 版本号
   - `.config`
   - `make prepare modules_prepare` 必须做过
   - 编译器前缀、版本一致

------

## 4.3 缺少内核头文件

**现象：**
 编译时出现：

```
fatal error: linux/module.h: No such file or directory
```

**原因：**
 说明内核源码没有准备好头文件。

**解决方法：**
 在内核目录执行：

```bash
make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- prepare modules_prepare
```

这一步会生成 `include/generated` 等必要文件。

------

## 4.4 模块安装路径与加载问题

**场景：**
 有时我们希望 `modprobe` 自动加载模块，而不是手动 `insmod`。

**步骤：**

1. 在宿主机执行：

   ```bash
   make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- modules_install INSTALL_MOD_PATH=/home/user/rootfs
   ```

   这会把 `.ko` 文件和依赖信息安装到指定根文件系统的 `lib/modules/<kernel-version>/` 下。

2. 在目标板运行 `depmod -a` 更新模块依赖。

3. 之后就可以用：

   ```bash
   modprobe mydriver
   ```

   来自动加载。

------

## 4.5 Kbuild 报错排查技巧

有时 `make` 报的错误很长，让人摸不着头脑。排查时可以这样做：

1. **增加详细输出**

   ```bash
   make V=1
   ```

   可以看到完整的编译命令行，方便对照 `ARCH`、`CROSS_COMPILE` 是否正确。

2. **确认路径**
    确保 `KDIR` 指向的是真正准备好的源码树，而不是空目录或不完整的内核头文件包。

3. **最小化测试**
    如果复杂模块编译有问题，可以先写一个最简单的 `hello.c` 驱动（只 `printk` 一句），用最简 Makefile 测试编译链路是否正常。

------

## 4.6 本章小结

- **Unknown symbol** → 符号没导出或版本不一致；
- **Invalid module format** → vermagic 不匹配；
- **缺少头文件** → 没做 `prepare modules_prepare`；
- **模块安装/加载** → 用 `modules_install` + `depmod`；
- **排查技巧** → `make V=1` + 最小化测试。

掌握了这些，就能在驱动编译和加载的道路上少踩很多坑。

------

# 第 5 章 Makefile 进阶技巧

在前几章，我们写的 Makefile 都是“能用”的版本。
 但如果项目一大、驱动一多，你会发现：

- 每次都要手动修改 `ARCH`、`CROSS_COMPILE`，很麻烦；
- 同一个模块可能需要在 PC（x86）上测试，也要在 ARM 板子上运行；
- 不同内核版本有时 API 改动，想要条件编译怎么办？

这一章，我们来聊聊 **如何让 Makefile 更聪明、更通用**。

------

## 5.1 参数可配置化

之前我们把 `ARCH` 和 `CROSS_COMPILE` 写死了：

```makefile
ARCH=arm
CROSS_COMPILE=arm-none-linux-gnueabihf-
```

这样不灵活。更好的办法是：**支持命令行覆盖**。

```makefile
# 模块名
obj-m := mydriver.o

# 内核源码路径
KDIR ?= /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
PWD  := $(shell pwd)

# 允许从命令行传入 ARCH 和 CROSS_COMPILE
ARCH ?= arm
CROSS_COMPILE ?= arm-none-linux-gnueabihf-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
```

这样写的好处是：

- 默认情况下用 ARM 架构和 `arm-none-linux-gnueabihf-` 工具链；

- 如果要在本机（x86）测试，可以直接覆盖：

  ```bash
  make ARCH=x86 CROSS_COMPILE=
  ```

------

## 5.2 支持多平台构建

有些人希望在同一个项目里，同时支持：

- x86_64（宿主机调试用）；
- ARM（目标板运行用）。

可以这样：

```makefile
ifeq ($(ARCH),arm)
CROSS_COMPILE ?= arm-none-linux-gnueabihf-
KDIR ?= /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
else
ARCH := x86
CROSS_COMPILE :=
KDIR ?= /lib/modules/$(shell uname -r)/build
endif
```

这样，你只需要执行：

```bash
make ARCH=arm      # 交叉编译 ARM 驱动
make ARCH=x86      # 编译宿主机调试用驱动
```

------

## 5.3 条件编译（兼容不同内核版本）

驱动开发经常遇到 API 改动，比如老版本内核用 `getnstimeofday()`，新版本改成了 `ktime_get_real_ts64()`。

解决办法是：利用 `KERNELRELEASE` 或 `LINUX_VERSION_CODE` 来写条件判断。

```c
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,20,0)
    getnstimeofday(&ts);
#else
    ktime_get_real_ts64(&ts);
#endif
```

如果需要在 Makefile 中做版本判断：

```makefile
KERNEL_VER := $(shell make -s -C $(KDIR) kernelversion)

ifeq ($(shell expr $(KERNEL_VER) \< 5.0),1)
ccflags-y += -DUSE_OLD_API
endif
```

这样在代码里就能用：

```c
#ifdef USE_OLD_API
    // 老版本代码
#else
    // 新版本代码
#endif
```

------

## 5.4 公共 Makefile 模板

当驱动越来越多时，通常会写一个“顶层 Makefile”，所有子目录共享：

```
drivers/
├── Makefile
├── mydriver1/
│   └── Makefile
├── mydriver2/
│   └── Makefile
```

顶层 Makefile 可以这样写：

```makefile
subdir-ccflags-y := -Wall -Wextra

obj-m += mydriver1/
obj-m += mydriver2/
```

每个子目录下只需写自己的 Makefile：

```makefile
obj-m := foo.o
```

这样整个目录一起构建，非常适合大项目。

------

## 5.5 小结

- `ARCH`、`CROSS_COMPILE` 可以写成可配置的，避免硬编码。
- 可以在一个 Makefile 里支持多平台：x86 调试，ARM 发布。
- 内核版本差异可以通过 `LINUX_VERSION_CODE` 做条件编译。
- 大项目可以用“顶层 Makefile + 子目录 Makefile”方式组织。

------

# 第 6 章 完整案例：Hello 驱动

到目前为止，我们讲了很多 Makefile 的写法、变体和技巧。
 但纸上得来终觉浅，最好的办法是跟着一个实际例子走一遍。

这一章，我们写一个最简单的 **“Hello, kernel” 驱动**，用它串起整个编译和加载过程。

------

## 6.1 驱动源码 hello.c

新建一个目录 `hello_driver/`，里面放一个 `hello.c`：

```c
#include <linux/init.h>      // __init 和 __exit 宏
#include <linux/module.h>    // 模块加载与卸载的核心接口
#include <linux/kernel.h>    // printk

// 模块加载函数
static int __init hello_init(void)
{
    printk(KERN_INFO "Hello, kernel! This is my first driver.\n");
    return 0;  // 0 表示加载成功
}

// 模块卸载函数
static void __exit hello_exit(void)
{
    printk(KERN_INFO "Goodbye, kernel! Driver removed.\n");
}

// 指定模块加载/卸载函数
module_init(hello_init);
module_exit(hello_exit);

// 模块描述信息
MODULE_LICENSE("GPL");                  // 避免内核标记 tainted
MODULE_AUTHOR("lizhaojun");             // 作者
MODULE_DESCRIPTION("A simple Hello driver");  // 简要说明
MODULE_VERSION("1.0");                  // 模块版本
```

这是一个典型的最小驱动：加载时打印一句话，卸载时再打印一句话。

------

## 6.2 配套 Makefile

在同一目录下写一个 `Makefile`：

```makefile
# 编译目标：hello.c -> hello.ko
obj-m := hello.o

# 内核源码路径（改成你自己的）
KDIR := /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
PWD  := $(shell pwd)

# 默认目标：交叉编译 ARM 模块
ARCH ?= arm
CROSS_COMPILE ?= arm-none-linux-gnueabihf-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
```

这样写支持命令行覆盖：

- 默认交叉编译 ARM 版本；

- 如果要在 PC 上测试，可以：

  ```bash
  make ARCH=x86 CROSS_COMPILE=
  ```

------

## 6.3 编译

在宿主机进入驱动目录：

```bash
cd hello_driver
make
```

如果一切正常，会生成：

```
hello.ko
```

这就是最终要放到目标板上的驱动模块。

------

## 6.4 拷贝到目标板

用 `scp` 或者 U 盘都行。示例（假设目标板 IP 是 192.168.31.50）：

```bash
scp hello.ko root@192.168.31.50:/tmp/
```

------

## 6.5 在目标板上加载与卸载

在目标板上执行：

```bash
# 加载模块
insmod /tmp/hello.ko

# 查看 dmesg
dmesg | tail -n 5
```

你应该能看到：

```
[   45.123456] Hello, kernel! This is my first driver.
```

卸载模块：

```bash
rmmod hello
dmesg | tail -n 5
```

你会看到：

```
[   60.987654] Goodbye, kernel! Driver removed.
```

------

## 6.6 小结

通过这个 Hello 驱动，我们完整跑通了一个流程：

1. 编写最小驱动代码（hello.c）；
2. 写 Makefile，让 Kbuild 帮我们编译；
3. 在宿主机交叉编译，生成 `.ko`；
4. 把 `.ko` 拷贝到目标板；
5. 用 `insmod` / `rmmod` 加载与卸载；
6. 在 `dmesg` 中看到我们自己的日志输出。

这就是最小的 **驱动开发闭环**。

------

👉 下一章（第 7 章），我可以写 **“进阶案例：字符设备驱动 + Makefile”**，展示如何在 `/dev/` 下创建设备节点，并且解释 Makefile 如何随着驱动逻辑的复杂化而扩展。

要不要我继续写第 7 章？

好👌，那我们继续写 **第 7 章：进阶案例：字符设备驱动 + Makefile**。这一章我会在前面 Hello 驱动的基础上，给你展示一个稍微复杂一点的 **字符设备驱动**，并且解释对应的 Makefile 写法。

------

# 第 7 章 进阶案例：字符设备驱动

在第 6 章里，我们写了一个最小的 “Hello 驱动”，加载和卸载时在 `dmesg` 里打印信息。
 但那个驱动没有和用户态交互，只能算是“能跑”。

这一章我们写一个 **字符设备驱动**，让它能在 `/dev/` 下生成一个节点，用户态可以用 `echo`、`cat` 来读写它。

------

## 7.1 驱动源码 chardev.c

在 `chardev_driver/` 目录下写一个 `chardev.c`：

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>          // 注册字符设备
#include <linux/uaccess.h>     // copy_to_user, copy_from_user

#define DEVICE_NAME "chardev_example"
#define BUF_LEN 100

static int major;              // 主设备号
static char msg[BUF_LEN];      // 驱动内部缓冲区
static int msg_len;

// 打开设备
static int chardev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "chardev: device opened\n");
    return 0;
}

// 释放设备
static int chardev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "chardev: device closed\n");
    return 0;
}

// 读操作
static ssize_t chardev_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    int bytes_read = 0;

    if (*offset >= msg_len)
        return 0; // 已经读完

    while (len && *offset < msg_len) {
        put_user(msg[*offset], buf++);
        len--;
        (*offset)++;
        bytes_read++;
    }

    printk(KERN_INFO "chardev: read %d bytes\n", bytes_read);
    return bytes_read;
}

// 写操作
static ssize_t chardev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    int i;

    if (len > BUF_LEN)
        len = BUF_LEN;

    if (copy_from_user(msg, buf, len))
        return -EFAULT;

    msg_len = len;
    for (i = 0; i < msg_len; i++)
        if (msg[i] == '\n') msg[i] = '\0'; // 去掉换行

    printk(KERN_INFO "chardev: written %d bytes: %s\n", msg_len, msg);
    return msg_len;
}

// 文件操作集
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .read    = chardev_read,
    .write   = chardev_write,
    .open    = chardev_open,
    .release = chardev_release,
};

// 模块加载函数
static int __init chardev_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "chardev: failed to register device\n");
        return major;
    }

    printk(KERN_INFO "chardev: registered with major number %d\n", major);
    printk(KERN_INFO "chardev: create device file with: mknod /dev/%s c %d 0\n", DEVICE_NAME, major);
    return 0;
}

// 模块卸载函数
static void __exit chardev_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "chardev: unregistered device\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lizhaojun");
MODULE_DESCRIPTION("A simple character device driver example");
MODULE_VERSION("1.0");
```

这个驱动做的事情：

1. 加载时动态分配主设备号，并提示你如何在 `/dev/` 下创建设备节点。
2. 提供 open/read/write/release 四个基本接口。
3. 写入的数据会存到 `msg` 缓冲区，再通过 `read` 读出来。

------

## 7.2 配套 Makefile

```makefile
# 编译目标：chardev.c -> chardev.ko
obj-m := chardev.o

# 内核源码路径（改成你自己的）
KDIR := /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
PWD  := $(shell pwd)

ARCH ?= arm
CROSS_COMPILE ?= arm-none-linux-gnueabihf-

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
```

------

## 7.3 编译与安装

在宿主机：

```bash
make
```

生成：

```
chardev.ko
```

拷贝到目标板：

```bash
scp chardev.ko root@192.168.31.50:/tmp/
```

------

## 7.4 在目标板上加载

1. 加载模块：

   ```bash
   insmod /tmp/chardev.ko
   dmesg | tail -n 5
   ```

   你会看到类似：

   ```
   chardev: registered with major number 240
   chardev: create device file with: mknod /dev/chardev_example c 240 0
   ```

2. 创建设备节点：

   ```bash
   mknod /dev/chardev_example c 240 0
   ```

------

## 7.5 用户态测试

写数据：

```bash
echo "hello driver" > /dev/chardev_example
```

读数据：

```bash
cat /dev/chardev_example
```

你会在 `dmesg` 里看到读写日志：

```
chardev: written 13 bytes: hello driver
chardev: read 13 bytes
```

------

## 7.6 小结

通过这个例子，你学会了：

- 驱动里如何注册一个字符设备；
- 如何在 `/dev/` 下创建设备节点；
- 用户态可以通过 `echo` 和 `cat` 与驱动交互；
- Makefile 依旧简单，只是编译目标换成了 `chardev.o`。

到这里，你已经能写出一个最小的、能和用户态交互的驱动了。

------

👉 下一章（第 8 章），我可以写 **“模块调试与优化：Makefile 中的 ccflags-y、调试日志、符号导出”**，帮你把调试技巧和 Makefile 的一些高级写法结合起来。

要不要我继续这样展开？

好👌，那我来继续写 **第 8 章：模块调试与优化**。这一章我们把重点放在 **调试技巧** 和 **Makefile 的一些进阶写法**上，结合前面写的驱动案例，让调试更高效。

------

# 第 8 章 模块调试与优化

驱动开发的难点不在“能编译”，而在“能跑”。
 一个 `.ko` 编出来，能不能正常插拔、和用户态交互、在不同内核版本下稳定工作，才是考验。

这一章我们来看看，如何通过 **Makefile 与代码结合**，让驱动更容易调试。

------

## 8.1 ccflags-y：给模块加编译选项

在普通应用程序的 Makefile 里，我们经常用 `CFLAGS` 来加警告开关、调试宏。
 在内核模块里，对应的变量是 **`ccflags-y`**。

示例：

```makefile
obj-m := chardev.o

# 给本模块加上额外的警告选项
ccflags-y := -Wall -Wextra -DDEBUG
```

这里：

- `-Wall -Wextra` → 打开更多编译警告，提前发现问题。
- `-DDEBUG` → 定义一个 `DEBUG` 宏，可以在代码里条件编译调试信息。

在代码里用：

```c
#ifdef DEBUG
printk(KERN_INFO "Debug: function=%s line=%d\n", __func__, __LINE__);
#endif
```

这样只要在 Makefile 里加 `-DDEBUG`，就能打开额外的日志，而不用反复改代码。

------

## 8.2 调试日志优化：宏封装

写驱动时，我们常用 `printk` 打日志，但裸写 `printk` 会让日志乱七八糟。
 建议写一个宏：

```c
#define DRIVER_NAME "chardev_example"

#define log_info(fmt, ...) \
    printk(KERN_INFO DRIVER_NAME ": " fmt, ##__VA_ARGS__)

#define log_err(fmt, ...) \
    printk(KERN_ERR DRIVER_NAME ": " fmt, ##__VA_ARGS__)
```

之后用：

```c
log_info("device opened\n");
log_err("failed to register device\n");
```

这样日志输出会更整齐，也方便一眼定位。

------

## 8.3 导出符号（EXPORT_SYMBOL）

有时候你写的驱动模块里有一些函数，想让其他模块调用，就要导出符号。

示例：

```c
int my_helper_function(int x)
{
    return x * 2;
}
EXPORT_SYMBOL(my_helper_function);
```

另一个模块就可以 `extern int my_helper_function(int x);` 并直接调用。

> 如果只想给 GPL 许可证的模块用，可以用 `EXPORT_SYMBOL_GPL`。

**提示**：如果忘了导出符号，加载另一个模块时就会报 “Unknown symbol”。

------

## 8.4 查看模块信息

编译好的 `.ko` 可以用 `modinfo` 查看：

```bash
modinfo chardev.ko
```

输出示例：

```
filename:       /tmp/chardev.ko
license:        GPL
description:    A simple character device driver example
author:         lizhaojun
version:        1.0
srcversion:     1234567890ABCDEF1234567
depends:        
vermagic:       6.1.22 SMP mod_unload ARMv7
```

其中 `vermagic` 必须和目标板内核一致，否则会报 “invalid module format”。

------

## 8.5 让 Makefile 帮你快速调试

在 Makefile 里，我们还可以加一些快捷目标：

```makefile
# 加载模块
load:
	sudo insmod chardev.ko || dmesg | tail -n 10

# 卸载模块
unload:
	sudo rmmod chardev || dmesg | tail -n 10

# 重新编译 + 卸载旧模块 + 加载新模块
reload: all unload load
```

这样每次只要：

```bash
make reload
```

就能一键完成“重新编译 + 替换驱动”的过程，省去很多手动操作。

------

## 8.6 本章小结

- `ccflags-y` 可以给模块加编译参数，比如 `-Wall`、`-DDEBUG`；
- 建议用日志宏封装 `printk`，让输出更规范；
- 如果要跨模块调用函数，记得用 `EXPORT_SYMBOL`；
- `modinfo` 可以快速查看 `.ko` 的依赖和 vermagic；
- 可以在 Makefile 里加 `load/unload/reload` 目标，提高调试效率。

------

👉 下一章（第 9 章），我可以写 **“进阶实践：同时管理多个驱动目录”**，比如一个项目里有几十个驱动，要如何用 Makefile 组织起来，不至于乱成一锅粥。

要不要我继续这样写下去？

好👌，那我接着写 **第 9 章**。这一章我们讨论更大规模的场景 —— 一个项目里不止一个驱动，而是几十个驱动甚至整个子系统。这时候 **Makefile 的组织方式**就很关键了。

------

# 第 9 章 同时管理多个驱动目录

到目前为止，我们的案例都是“单个驱动目录 + 一个 Makefile”。
 但在实际项目里，驱动往往会被拆分成多个目录：

- 不同功能模块独立维护；
- 驱动团队多人协作，每人一个子目录；
- 有的驱动编译成模块，有的直接集成进内核。

如果没有组织好，Makefile 会乱成一锅粥。
 这一章我们来看看，如何在大项目里 **层层组织 Makefile**。

------

## 9.1 顶层 + 子目录结构

假设我们有三个驱动：

- `led` —— 控制 LED 灯
- `key` —— 处理按键输入
- `lcd` —— 驱动 LCD 显示

目录结构：

```
drivers/
├── Makefile        ← 顶层 Makefile
├── led/
│   ├── Makefile
│   └── led.c
├── key/
│   ├── Makefile
│   └── key.c
└── lcd/
    ├── Makefile
    └── lcd.c
```

------

## 9.2 顶层 Makefile

在顶层 `drivers/Makefile` 里，我们只需要声明：

```makefile
obj-m += led/
obj-m += key/
obj-m += lcd/
```

这样，Kbuild 会自动进入这三个子目录，读取它们的 Makefile 并编译。

------

## 9.3 子目录 Makefile

每个子目录只关心自己的模块：

**led/Makefile**

```makefile
obj-m := led.o
```

**key/Makefile**

```makefile
obj-m := key.o
```

**lcd/Makefile**

```makefile
obj-m := lcd.o
```

这样编译后，你会在 `drivers/` 目录下同时得到：

```
led.ko
key.ko
lcd.ko
```

------

## 9.4 混合模式：一个子目录多个文件

如果 `lcd` 驱动比较复杂，由多个文件组成，可以在 `lcd/Makefile` 里写：

```makefile
obj-m := lcd.o
lcd-objs := lcd_main.o lcd_ctrl.o lcd_dma.o
```

最终还是只生成一个 `lcd.ko`。

------

## 9.5 扩展：源码树内的管理

如果你要把整个 `drivers/` 目录挂到内核源码树里，那就在 `drivers/Kconfig` 里加：

```Kconfig
source "drivers/led/Kconfig"
source "drivers/key/Kconfig"
source "drivers/lcd/Kconfig"
```

在 `drivers/Makefile` 里写：

```makefile
obj-$(CONFIG_LED) += led/
obj-$(CONFIG_KEY) += key/
obj-$(CONFIG_LCD) += lcd/
```

这样就能在 `make menuconfig` 里选择性地打开/关闭这些驱动。

------

## 9.6 批量清理与编译

当驱动数量多时，可以在顶层 Makefile 里写一个批量清理：

```makefile
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

这样执行 `make clean`，会自动清理所有子目录的生成物。

同理，执行 `make` 就会一次性编译所有驱动。

------

## 9.7 小结

- 用 **顶层 + 子目录** 的方式组织驱动目录，清晰又可扩展。
- 顶层 Makefile 负责声明子目录，子目录 Makefile 各自独立维护。
- 复杂驱动可以用 `xxx-objs` 把多个 `.c` 文件合并成一个 `.ko`。
- 集成到内核源码树时，可以配合 `Kconfig` 管理开关。

这样，你就能轻松管理几十个驱动，而不会乱掉。

------

👉 下一章（第 10 章），我可以写 **“实战锦囊：Makefile 与驱动开发中的最佳实践”**，总结一些经验法则，比如：

- 如何保持 Makefile 简洁而不失灵活；
- 什么时候适合外部模块，什么时候适合集成内核；
- 如何避免版本不匹配、符号缺失等常见坑。

要不要我继续写第 10 章？

好👌，那我们进入 **最后一章：最佳实践总结**。这一章我会把前面所有知识点提炼成“锦囊”，像一本说明书的附录，便于你在实际工作中快速参考。

------

# 第 10 章 实战锦囊：最佳实践

写驱动的 Makefile，看似只是几行代码，但细节决定成败。
 这一章，我们来总结一些 **经验法则**，帮助你在实际开发中少踩坑、多省力。

------

## 10.1 保持 Makefile 简洁

**原则**：能写一行，不写两行。

示例（最小外部模块）：

```makefile
obj-m := mydriver.o
KDIR  := /home/lizhaojun/linux/nxp/kernel/linux-imx-6.1
PWD   := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- modules
```

> 千万别像应用程序 Makefile 那样罗列一堆 `gcc` 参数 —— 在驱动世界里，这都是 Kbuild 的事。

------

## 10.2 参数要可配置

不要把 `ARCH` 和 `CROSS_COMPILE` 写死，支持命令行覆盖：

```makefile
ARCH ?= arm
CROSS_COMPILE ?= arm-none-linux-gnueabihf-
```

这样：

- 默认编 ARM 模块；

- 如果要在 PC 上调试：

  ```bash
  make ARCH=x86 CROSS_COMPILE=
  ```

------

## 10.3 外部模块 vs 内核集成

- **外部模块 (`obj-m`)**
  - 适合开发调试阶段；
  - 修改后编译快；
  - 用 `insmod` 即可加载。
- **源码树集成 (`obj-$(CONFIG_xxx)`)**
  - 适合产品化阶段；
  - 配合 `Kconfig`，能在 `menuconfig` 里管理；
  - 可选择 `y/m/n`，灵活集成到内核。

**经验法则**：

- 早期开发 → 外部模块
- 项目定型 → 内核集成

------

## 10.4 版本匹配是第一要务

最常见的坑：

- “Unknown symbol”
- “invalid module format”
- “vermagic mismatch”

**口诀**：

> **KDIR 一定要与目标板运行的内核 100% 匹配**。

匹配包括：

- 内核版本号
- `.config` 配置
- 编译器版本

否则就算模块编译出来，`insmod` 也会失败。

------

## 10.5 善用调试手段

- 在 Makefile 加：

  ```makefile
  ccflags-y := -Wall -Wextra -DDEBUG
  ```

- 在代码里写：

  ```c
  #ifdef DEBUG
  printk(KERN_INFO "Debug: %s:%d\n", __func__, __LINE__);
  #endif
  ```

- 用 `make V=1` 查看完整编译命令，排查编译参数问题。

- 用 `modinfo mydriver.ko` 查看 vermagic、依赖信息。

------

## 10.6 多驱动项目的组织

- 顶层 Makefile 声明子目录：

  ```makefile
  obj-m += led/ key/ lcd/
  ```

- 子目录各自维护自己的 Makefile：

  ```makefile
  obj-m := led.o
  ```

- 如果要合并多个文件成一个模块：

  ```makefile
  mydriver-objs := main.o foo.o bar.o
  ```

这样项目结构清晰，便于多人协作。

------

## 10.7 一键调试

在 Makefile 里加：

```makefile
load:
	sudo insmod mydriver.ko || dmesg | tail -n 10

unload:
	sudo rmmod mydriver || dmesg | tail -n 10

reload: all unload load
```

> 每次调试时只需 `make reload`，即可完成重新编译 + 卸载旧模块 + 加载新模块。

------

## 10.8 何时使用 `EXPORT_SYMBOL`

- 如果你写的驱动只在自己模块内用，不需要导出符号。

- 如果要让其他模块调用，就必须用：

  ```c
  EXPORT_SYMBOL(my_func);
  ```

- 如果你希望只允许 GPL 驱动使用，就用：

  ```c
  EXPORT_SYMBOL_GPL(my_func);
  ```

------

## 10.9 最终口诀

1. **简单** —— Makefile 只声明，不写编译细节。
2. **匹配** —— KDIR 必须与目标板内核完全一致。
3. **灵活** —— 外部调试，内部集成。
4. **调试** —— 善用 `DEBUG` 宏、`make V=1`、`modinfo`。
5. **组织** —— 顶层 + 子目录，避免混乱。

------

## 本书总结

从第 1 章到第 10 章，我们走过了：

- 最小外部模块
- 多文件/多模块写法
- 内核源码树集成
- 常见问题排查
- Makefile 调优技巧
- Hello 驱动 & 字符设备驱动案例
- 大项目组织方法
- 最佳实践锦囊

到此为止，你已经掌握了 **Linux 驱动 Makefile 的完整知识体系**。

------

