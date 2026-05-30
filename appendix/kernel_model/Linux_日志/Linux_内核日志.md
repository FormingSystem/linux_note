# 第 1 章 一条日志是怎样“出现”的？

> 读完本章，你会清楚一条内核日志从函数调用到屏幕/日志文件的**完整旅程**，知道“为什么我打了日志却看不见”。

## 1.1 两个目的地：Ring Buffer 与 Console

- **Ring Buffer（内核环形缓冲区）**
   每条日志都会先写入这里。`dmesg`、`journalctl -k` 读的都是它（或其持久化）。**无论控制台等级如何，Ring Buffer 都会收**。
- **Console（控制台）**
   指屏幕/串口等“实时显示”终端。**只有“等级≥门槛”的日志**才会被同时输出到 Console。

### Console 等级门槛怎么调？

```bash
# 查看四元组：current default minimum boot
cat /proc/sys/kernel/printk

# 临时允许 INFO(6) 与 DEBUG(7) 也上控制台
dmesg -n 7
# 或直接写入四元组（常见形如 6 6 1 7）
echo 7 7 1 7 | sudo tee /proc/sys/kernel/printk
```

## 1.2 简化的调用链

```
你的代码：printk()/pr_info()/dev_err()
        ↓
vprintk() / log_store()：拼接级别、时间戳、CPU/线程等元信息
        ↓
Ring Buffer：内核统一收集
        ↓
Console 驱动（串口tty/显卡framebuffer/虚拟终端）：等级达标才显示
```

> 现代内核为避免死锁/长时间持锁打印，还会引入后台线程/延迟打印等机制（如 `printk_deferred()`），但**对使用者的 API 不变**：你只需正确选择 `printk/pr_* / dev_*`，其余交给内核。

## 1.3 本书写法约定

- **示例能跑**：全部按外部模块（out-of-tree）构建。
- **头文件速查**：每节都会给到“我该 include 谁”。
- **命令可复制**：`make && insmod && dmesg -w` 即可观察现象。

------

# 第 2 章 printk`（深入但好用）

> 这一节你会真正理解 `printk` 的“级别前缀”、常用格式化扩展，以及它与高层封装的关系。

## 2.1 `printk()` 的级别前缀

`printk()` 自身不带级别；用字符串宏做前缀（在 `<linux/printk.h>`）：

| 宏             | 数值 | 语义       | 习惯用法     |
| -------------- | ---- | ---------- | ------------ |
| `KERN_EMERG`   | 0    | 崩溃级     | panic 前打印 |
| `KERN_ALERT`   | 1    | 立刻处理   | 关键硬件异常 |
| `KERN_CRIT`    | 2    | 严重错误   | 数据损坏     |
| `KERN_ERR`     | 3    | 错误       | 失败分支     |
| `KERN_WARNING` | 4    | 警告       | 可继续但异常 |
| `KERN_NOTICE`  | 5    | 重要但正常 | 启动里程碑   |
| `KERN_INFO`    | 6    | 一般信息   | 初始化信息   |
| `KERN_DEBUG`   | 7    | 调试       | 调试噪声     |

**示例（阶段 2-1）**

```c
// loglab.c
#include <linux/module.h>
#include <linux/printk.h>

static int __init loglab_init(void)
{
    printk(KERN_INFO  "printk: hello info\n");
    printk(KERN_ERR   "printk: error=%d\n", -22);
    printk(KERN_DEBUG "printk: debug (maybe hidden)\n");
    return 0;
}
static void __exit loglab_exit(void)
{
    printk(KERN_INFO "printk: bye\n");
}
module_init(loglab_init);
module_exit(loglab_exit);
MODULE_LICENSE("GPL");
```

> 看不到 DEBUG？请把控制台等级提到 7：`dmesg -n 7`。即便看不见，它也在 Ring Buffer 里。

## 2.2 printk 的“内核格式化”小技巧（实用精选）

- **指针**：`%pK` 受 `kptr_restrict` 影响；`%px` 不遮掩（调试用）。
- **错误码**：`%pe` 会把 `-ENOENT` 转成字符串 `"(-2)"` 且含义。
- **UUID**：`%pUb/%pUl` 输出 16 字节 UUID（大/小端）。
- **网络地址**：`%pI4`（IPv4）、`%pI6`（IPv6）、`%pIS`（带端口）。
- **位掩码**：`%*pb` 按位打印 bitmap。
- **时长**：`%pM`（MAC），`%pr`（资源区间）等。

> 这些扩展让 `printk` 更像“内核专属 printf”。写驱动时善用它能减少解析压力。

**头文件**

- `#include <linux/printk.h>`（`printk` 与所有级别宏）
- 一些 `%p` 扩展可能依赖特定子系统头，但常用无需额外 include。

------

# 第 3 章 `pr_*`（规范与前缀）

> `pr_info/pr_err/...` 是**官方推荐**的高层打印方式：统一级别、易读、可自动加模块前缀。

## 3.1 `pr_*` 是如何封装的

在 `<linux/printk.h>` 里，`pr_info(x)` 展开为：

```
printk(KERN_INFO pr_fmt(x))
```

其中 `pr_fmt(fmt)` 可自定义“前缀策略”。

## 3.2 给日志自动带上“模块名：”

在每个 `.c` 顶部加：

```c
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
```

- **`KBUILD_MODNAME`** 由 Kbuild 在编译时以 `-D` 注入，取值等于你的**模块目标名**（如 `obj-m += loglab.o` → `"loglab"`）。
- 多文件模块（`mymod-objs := a.o b.o`）时，**所有 .c 的 `KBUILD_MODNAME` 都是 `"mymod"`**；`KBUILD_BASENAME` 则是各自源文件名（`"a"` / `"b"`）。

**示例（阶段 3-1）**

```c
#include <linux/module.h>
#include <linux/printk.h>
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int __init loglab_init(void)
{
    pr_info("init, MOD=%s\n", KBUILD_MODNAME); // loglab: init, MOD=loglab
    pr_err("oops\n");                          // loglab: oops
    return 0;
}
static void __exit loglab_exit(void) { pr_info("exit\n"); }
module_init(loglab_init);
module_exit(loglab_exit);
MODULE_LICENSE("GPL");
```

> 想按“源文件”而非“模块”区分？把 `pr_fmt(fmt)` 改成 `KBUILD_BASENAME ": " fmt`。
>  想稳定可控？自定义常量：`#define DRV_NAME "mydrv"`，再 `#define pr_fmt(fmt) DRV_NAME ": " fmt`。

## 3.3 `pr_*` 系列与注意点（精要）

| 宏            | 等级       | 用途                    |
| ------------- | ---------- | ----------------------- |
| `pr_emerg()`  | EMERG(0)   | 崩溃级                  |
| `pr_alert()`  | ALERT(1)   | 立刻处理                |
| `pr_crit()`   | CRIT(2)    | 严重错误                |
| `pr_err()`    | ERR(3)     | 错误                    |
| `pr_warn()`   | WARNING(4) | 警告（注意别用老别名）  |
| `pr_notice()` | NOTICE(5)  | 正常但重要              |
| `pr_info()`   | INFO(6)    | 一般信息                |
| `pr_debug()`  | DEBUG(7)   | 调试（第 5 章详解开关） |
| `pr_cont()`   | 延续行     | 不加级别、不换行，慎用  |

**避免陷阱**：长串拼接用一条日志打印，尽量不要 `pr_cont()` 连续输出，避免与其他 CPU 的日志交叉。

------

# 第 4 章 把日志“绑到设备上”：`dev_*`（驱动首选）

> 对驱动来说，最重要的问题不是“谁（模块）说的”，而是“**哪块设备**出了问题”。`dev_info/err/warn/dbg()` 会自动带上设备标识（总线/地址/设备名），**排障效率飞升**。

## 4.1 `dev_*` 的形式与前缀

原型（`<linux/device.h>`）：

```c
dev_info(struct device *dev, const char *fmt, ...);
dev_warn(struct device *dev, const char *fmt, ...);
dev_err (struct device *dev, const char *fmt, ...);
dev_dbg (struct device *dev, const char *fmt, ...); // 调试
```

它们内部相当于 `dev_printk(level, dev, pr_fmt(fmt), ...)`，前缀会包含设备信息（如 `platform foo.0:`、`pci 0000:00:1f.2:`、自建 `class` 的 `/dev` 名称等）。

## 4.2 我从哪里拿到 `struct device *`？

常见驱动类型与“拿 dev 指针”的方法：

| 驱动类型           | 入口/对象                      | 拿到 `dev` 的方式                |
| ------------------ | ------------------------------ | -------------------------------- |
| platform 驱动      | `struct platform_device *pdev` | `&pdev->dev`                     |
| I²C 驱动           | `struct i2c_client *client`    | `&client->dev`                   |
| SPI 驱动           | `struct spi_device *spi`       | `&spi->dev`                      |
| PCI 驱动           | `struct pci_dev *pdev`         | `&pdev->dev`                     |
| net_device（网卡） | `struct net_device *ndev`      | **用 `netdev_\*` 专用宏**        |
| 字符设备（自建）   | `class_create/device_create`   | `device_create` 的返回值就是 dev |

> **网络驱动建议用**：`netdev_info/err/warn/dbg(ndev, ...)`，会带上接口名（如 `eth0`），更贴切。

## 4.3 例子：不引入总线，造个最小设备来打印

**示例（阶段 4-1）**

```c
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/device.h>
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static struct class  *loglab_cls;
static struct device *loglab_dev;

static int __init loglab_init(void)
{
    pr_info("init\n");

    loglab_cls = class_create(THIS_MODULE, "loglab");
    if (IS_ERR(loglab_cls)) return PTR_ERR(loglab_cls);

    // 仅作为 dev_* 日志的承载点
    loglab_dev = device_create(loglab_cls, NULL, MKDEV(0,0), NULL, "loglab0");
    if (IS_ERR(loglab_dev)) {
        class_destroy(loglab_cls);
        return PTR_ERR(loglab_dev);
    }

    dev_info(loglab_dev, "device online\n");
    dev_warn(loglab_dev, "temperature high (demo)\n");
    dev_err (loglab_dev, "allocation failed (demo)\n");
    return 0;
}

static void __exit loglab_exit(void)
{
    device_destroy(loglab_cls, MKDEV(0,0));
    class_destroy(loglab_cls);
    pr_info("exit\n");
}
module_init(loglab_init);
module_exit(loglab_exit);
MODULE_LICENSE("GPL");
```

**你会看到**（示例前缀，依平台而异）：

```
loglab: init
loglab 0:0: device online
loglab 0:0: temperature high (demo)
loglab 0:0: allocation failed (demo)
loglab: exit
```

## 4.4 最佳实践（设备日志的“手感”）

- **能用 `dev_\*` 就别用 `pr_\*`**：定位更准。
- `probe/remove/open/close/错误分支`：输出带上下文的 `dev_err/ warn/info`。
- 高频路径慎重打印（下一章会讲限流/一次性）。
- 遇到 `-EPROBE_DEFER` 这类常见重试场景，可用 `dev_err_probe()`（自动处理延迟探测、避免刷屏）。

**头文件**

- `#include <linux/device.h>`（`dev_*` 与 `class/device_create`）
- `#include <linux/printk.h>`（`pr_*`）

------

**本批次到这里**：我们已经把“日志去哪里”“如何写得规范”“如何与设备绑定”完整铺垫好。
 下一批我会顺着章节继续展开 **第 5～10 章**：`pr_debug/dev_dbg` 的编译期与动态调试、限流与一次性打印、十六进制转储、看不见日志的三连排查、头文件速查与“一次到位的最小工程”。如果你希望我在第 5 章里加入 **Dynamic Debug 控制语法的速查卡**，我就一并放进去，读起来更顺手。

没问题。下面我**继续按我们既定的章节顺序**，把 **第 5～10 章**详细展开。写法仍保持“出版社书籍”的节奏：先铺垫心智，再给可运行例子，最后给到排查与小结。内核版本以 **Linux 6.1** 为准。

------

# 第 5 章 调试输出要“可开可关”：`pr_debug` / `dev_dbg`（含 Dynamic Debug 速查卡）

> 目标：让调试日志**默认安静**，需要时**随时打开**，而且**不用重编译、不用重载模块**。

## 5.1 两种开启方式的全貌

### 5.1.1 编译期开关（没有 Dynamic Debug 时）

- 当 **未启用 CONFIG_DYNAMIC_DEBUG** 时，`pr_debug/dev_dbg` 的行为取决于 `DEBUG`：

  ```c
  #ifdef DEBUG
  # define pr_debug(fmt, ...) printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
  #else
  # define pr_debug(fmt, ...) no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
  #endif
  ```

- 也就是说：**你在源码或编译参数里 `#define DEBUG`**，`pr_debug/dev_dbg` 才会真正打印；否则只是做类型检查（不输出）。

> 外部模块最简单：在 `.c` 顶部 `#define DEBUG`。
>  内核树工程也可 `ccflags-y += -DDEBUG`（此处了解即可）。

### 5.1.2 运行期开关（**Dynamic Debug**，强烈推荐）

- 当 **启用 CONFIG_DYNAMIC_DEBUG**（或至少 `CONFIG_DYNAMIC_DEBUG_CORE`）时，`pr_debug/dev_dbg` 自动接入动态调试框架，你可以在**运行时**通过 **debugfs/proc** 接口精确开启：

  ```bash
  # 先挂载 debugfs（若尚未）
  sudo mount -t debugfs none /sys/kernel/debug
  
  # 打开本模块所有 pr_debug/dev_dbg
  echo 'module loglab +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
  
  # 关闭
  echo 'module loglab -p' | sudo tee /sys/kernel/debug/dynamic_debug/control
  ```

> 好处：**无需重新编译/加载**，还能按**模块/文件/函数/行号/格式串**做精细过滤。

------

## 5.2 Dynamic Debug 速查卡（Linux 6.1）

**控制文件**（二选一，通常用 debugfs）

- `/sys/kernel/debug/dynamic_debug/control`
- `/proc/dynamic_debug/control`

**基本语法**（一条规则一行）：

```
# 语法
<selector> <flags>

# selector（可叠加多种条件）
module <mod>         # 模块名（不含 .ko）
file <path>          # 源文件路径（相对内核源码根，或子串匹配）
func <name>          # 函数名
line <lineno>        # 行号（可配合 file）
format "<substr>"    # 匹配格式串中的子串

# flags（对匹配到的调试点进行操作）
+p   开启打印
-p   关闭打印
+f   打印函数名
+F   打印文件与行号
+t   打印时间戳
+T   打印线程（task）信息
```

**常见用法示例**：

```bash
# 1) 打开某模块全部调试
echo 'module loglab +p' | sudo tee /sys/kernel/debug/dynamic_debug/control

# 2) 指定文件
echo 'file drivers/foo/bar.c +p' | sudo tee /sys/kernel/debug/dynamic_debug/control

# 3) 指定函数
echo 'func probe +p' | sudo tee /sys/kernel/debug/dynamic_debug/control

# 4) 指定行号（需配合 file）
echo 'file drivers/foo/bar.c line 120 +p' | sudo tee ...

# 5) 按格式串关键字
echo 'format "rx path" +p' | sudo tee ...

# 6) 叠加更多细节（带函数名与行号）
echo 'module loglab +pfF' | sudo tee ...

# 7) 一键关闭
echo 'module loglab -p' | sudo tee ...
```

**列出当前规则/状态**

```bash
# 全部调试点（很多，配合 grep）
cat /sys/kernel/debug/dynamic_debug/control | grep loglab
```

**内核命令行（可选）**

- 也可以通过 **内核启动参数**一次性启用，比如：
   `dyndbg="module loglab +p"`
   （不同发行版传参方式略异，了解即可）

------

## 5.3 最小范例：`pr_debug` / `dev_dbg` 联动

```c
// loglab_dbg.c（节选）
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/device.h>
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static struct class *cls;
static struct device *dev0;

static int __init demo_init(void)
{
    cls = class_create(THIS_MODULE, "loglab");
    if (IS_ERR(cls)) return PTR_ERR(cls);
    dev0 = device_create(cls, NULL, MKDEV(0,0), NULL, "loglab0");
    if (IS_ERR(dev0)) return PTR_ERR(dev0);

    pr_debug("hello from pr_debug\n");       // 默认隐藏
    dev_dbg(dev0, "hello from dev_dbg\n");   // 默认隐藏
    return 0;
}
static void __exit demo_exit(void)
{
    device_destroy(cls, MKDEV(0,0));
    class_destroy(cls);
}
module_init(demo_init);
module_exit(demo_exit);
MODULE_LICENSE("GPL");
```

**开启方式**：

```bash
# 运行期打开（推荐）
sudo mount -t debugfs none /sys/kernel/debug
echo 'module loglab_dbg +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
dmesg | tail
```

> 若系统未启用 Dynamic Debug，你也可以在源码顶部 `#define DEBUG`，但**运行期灵活度远不如动态调试**。

------

# 第 6 章 控制“分寸”：限流与“一次性打印”

> 高频路径里直接日志，会把 Ring Buffer 淹没，定位也变困难。本章教你“节制地说话”。

## 6.1 “只说一次”：`*_once`

- 模块级：`pr_info_once()` / `pr_warn_once()` / `pr_err_once()` …
- 设备级：`dev_info_once()` / `dev_warn_once()` / `dev_err_once()` …

**示例**

```c
pr_warn_once("first warning only once\n");
dev_info_once(dev0, "device info only once\n");
```

> 语义：**本次引导期间**只打印一次（每条调用点各记一次）。

## 6.2 “别刷屏”：`*_ratelimited`

- 模块级：`pr_info_ratelimited()` / `pr_warn_ratelimited()` …
- 设备级：`dev_err_ratelimited(dev, ...)` …

**示例**

```c
if (likely(noise)) {
    pr_info_ratelimited("too chatty, throttled\n");
    dev_err_ratelimited(dev0, "error bursts throttled\n");
}
```

**系统级限流调节**（全局令牌桶参数）：

```bash
# 允许多少条/多长时间内的打印
cat /proc/sys/kernel/printk_ratelimit
cat /proc/sys/kernel/printk_ratelimit_burst

# 调整（示例值，按需）
echo 5  | sudo tee /proc/sys/kernel/printk_ratelimit
echo 20 | sudo tee /proc/sys/kernel/printk_ratelimit_burst
```

> 内核里具体实现基于令牌桶，超出后会丢弃并统计略过次数（偶尔会见到 “__ratelimit” 提示）。

------

# 第 7 章 看“二进制世界”：十六进制转储

> 抓包、看寄存器、核对报文头最直接的办法就是**十六进制转储**。

## 7.1 两个接口

- `print_hex_dump(level, prefix_str, prefix_type, rowsize, groupsize, buf, len, ascii)`
- `print_hex_dump_bytes(prefix_str, prefix_type, buf, len)`（简化版，level 使用 `KERN_DEBUG`）

**参数要点**

- `level`：日志级别，如 `KERN_DEBUG`。
- `prefix_type`：前缀格式，常用 `DUMP_PREFIX_NONE / DUMP_PREFIX_OFFSET / DUMP_PREFIX_ADDRESS`。
- `rowsize`：每行多少字节（一般 16）。
- `groupsize`：按 1/2/4/8 字节分组（例如 2 表示 16 位一组）。
- `ascii`：是否在行尾显示 ASCII。

**示例**

```c
#include <linux/printk.h>

static const u8 pkt[] = {0xde,0xad,0xbe,0xef,0,1,2,3,4,5,6,7,8,9,0xaa,0xbb};

print_hex_dump(KERN_DEBUG, "rx: ", DUMP_PREFIX_OFFSET, 16, 1, pkt, sizeof(pkt), true);
/* 或更简单： */
print_hex_dump_bytes("rx: ", DUMP_PREFIX_OFFSET, pkt, sizeof(pkt));
```

> 这是 `DEBUG` 级别；看不到时请 `dmesg -n 7` 或用 Dynamic Debug 打开对应调试点。

------

# 第 8 章 “为什么看不见？”三连排查

1. **Console 门槛太高**

   - `dmesg -n 6`（允许 INFO）或 `dmesg -n 7`（允许 DEBUG）
   - 或直接：`echo 7 7 1 7 | sudo tee /proc/sys/kernel/printk`

2. **`pr_debug/dev_dbg` 还没启**

   - 编译期开：`#define DEBUG`；

   - 运行期开：Dynamic Debug

     ```bash
     sudo mount -t debugfs none /sys/kernel/debug
     echo 'module <你的模块名> +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
     ```

3. **设备级日志没出现**

   - 确认你的 `struct device *` 有效（`device_create` 成功，或真实 `probe` 场景里的 `&pdev->dev` 等）。
   - 若走字符设备路径，也可以保留 `dev` 指针在 `file->private_data` 里，统一用 `dev_*`。

> 额外提示：日志**一定进 Ring Buffer**，只是可能没到 Console。实时看 Ring Buffer：`dmesg -w`。

------

# 第 9 章 头文件速查表（够用即止）

| 功能/宏                                         | 头文件                                                       |
| ----------------------------------------------- | ------------------------------------------------------------ |
| `printk`, `pr_*`, `pr_fmt`, `KERN_*`            | `<linux/printk.h>`                                           |
| `pr_*_once`, `*_ratelimited`, `print_hex_dump*` | `<linux/printk.h>`                                           |
| `dev_*`（`dev_info/err/warn/dbg`）              | `<linux/device.h>`                                           |
| `netdev_*`（网络设备专用）                      | `<linux/netdevice.h>`                                        |
| Dynamic Debug 控制接口                          | **无需 include**；操作 `/sys/kernel/debug/dynamic_debug/control` |
| `KBUILD_MODNAME / KBUILD_BASENAME`              | 由 Kbuild 以 `-D` 注入（无需 include；`make V=1` 可见）      |

------

# 第 10 章 一次到位的最小工程（覆盖本书全部要点）

> **你只需要这两个文件**，就能边学边用：`pr_*`、`dev_*`、`pr_fmt`、`pr_debug/dev_dbg`（含 Dynamic Debug）、`*_once`、`*_ratelimited`、十六进制转储、Console 等级调节。

## 10.1 Makefile（外部模块）

```make
obj-m += loglab.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

## 10.2 loglab.c（整合版，可直接编译运行）

```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/device.h>

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static struct class  *loglab_cls;
static struct device *loglab_dev;

static int __init loglab_init(void)
{
    static const u8 demo_bin[] = {0xde,0xad,0xbe,0xef,0,1,2,3,4,5,6,7,0xaa,0xbb};

    pr_info("init, MOD=%s\n", KBUILD_MODNAME);
    pr_err("example error (info for contrast follows)\n");

    /* 设备级日志：用于真实驱动更合适 */
    loglab_cls = class_create(THIS_MODULE, "loglab");
    if (IS_ERR(loglab_cls)) {
        pr_err("class_create failed: %ld\n", PTR_ERR(loglab_cls));
        return PTR_ERR(loglab_cls);
    }
    loglab_dev = device_create(loglab_cls, NULL, MKDEV(0,0), NULL, "loglab0");
    if (IS_ERR(loglab_dev)) {
        pr_err("device_create failed: %ld\n", PTR_ERR(loglab_dev));
        class_destroy(loglab_cls);
        return PTR_ERR(loglab_dev);
    }

    dev_info(loglab_dev, "device online\n");
    dev_warn(loglab_dev, "warn demo\n");
    dev_err (loglab_dev, "err demo\n");

    /* 调试输出：默认静默，运行期可通过 Dynamic Debug 打开 */
    pr_debug("pr_debug (maybe hidden)\n");
    dev_dbg (loglab_dev, "dev_dbg (maybe hidden)\n");

    /* 一次性与限流 */
    pr_info_once("printed once\n");
    dev_warn_once(loglab_dev, "device warn once\n");
    pr_info_ratelimited("ratelimited info (may be throttled)\n");
    dev_err_ratelimited(loglab_dev, "ratelimited error (throttled)\n");

    /* 十六进制转储（DEBUG 级别） */
    print_hex_dump(KERN_DEBUG, "dump: ", DUMP_PREFIX_OFFSET,
                   16, 1, demo_bin, sizeof(demo_bin), true);

    return 0;
}

static void __exit loglab_exit(void)
{
    device_destroy(loglab_cls, MKDEV(0,0));
    class_destroy(loglab_cls);
    pr_info("exit\n");
}

module_init(loglab_init);
module_exit(loglab_exit);
MODULE_LICENSE("GPL");
```

**运行与体验**

```bash
make
sudo insmod loglab.ko
dmesg -w   # 开一个窗口实时看

# 打开动态调试（若内核启用了 CONFIG_DYNAMIC_DEBUG）
sudo mount -t debugfs none /sys/kernel/debug
echo 'module loglab +p' | sudo tee /sys/kernel/debug/dynamic_debug/control

# 如需看 DEBUG 级别到控制台
dmesg -n 7

sudo rmmod loglab
```

------

## 小结（第 5～10 章）

- **调试输出策略**：默认静默 → Dynamic Debug 精准开启；没有再用 `#define DEBUG`。
- **日志粒度选择**：模块级用 `pr_*`，设备路径用 `dev_*`（或网卡用 `netdev_*`）。
- **节制**：`*_once` / `*_ratelimited`；必要时调整 `/proc/sys/kernel/printk_ratelimit*`。
- **可见性**：Ring Buffer 一定收；Console 由 `dmesg -n` 门槛控制。
- **工具化**：十六进制转储 `print_hex_dump*` 让你“看见”二进制。

------

如果你需要，我可以把 **netdev_\*（网络设备专用日志）**、**早期打印（early/bootconsole）**、**trace_printk 与 ftrace 配合做高频诊断** 写成后续章节，用同样的风格继续往下铺。