[TOC] 

内容摘抄至GPT。

# 成员介绍

```c
struct file_operations {
	struct module *owner;
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
	ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
	int (*iopoll)(struct kiocb *kiocb, struct io_comp_batch *,
			unsigned int flags);
	int (*iterate) (struct file *, struct dir_context *);
	int (*iterate_shared) (struct file *, struct dir_context *);
	__poll_t (*poll) (struct file *, struct poll_table_struct *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	unsigned long mmap_supported_flags;
	int (*open) (struct inode *, struct file *);
	int (*flush) (struct file *, fl_owner_t id);
	int (*release) (struct inode *, struct file *);
	int (*fsync) (struct file *, loff_t, loff_t, int datasync);
	int (*fasync) (int, struct file *, int);
	int (*lock) (struct file *, int, struct file_lock *);
	ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
	unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
	int (*check_flags)(int);
	int (*flock) (struct file *, int, struct file_lock *);
	ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
	ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
	int (*setlease)(struct file *, long, struct file_lock **, void **);
	long (*fallocate)(struct file *file, int mode, loff_t offset,
			  loff_t len);
	void (*show_fdinfo)(struct seq_file *m, struct file *f);
#ifndef CONFIG_MMU
	unsigned (*mmap_capabilities)(struct file *);
#endif
	ssize_t (*copy_file_range)(struct file *, loff_t, struct file *,
			loff_t, size_t, unsigned int);
	loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in,
				   struct file *file_out, loff_t pos_out,
				   loff_t len, unsigned int remap_flags);
	int (*fadvise)(struct file *, loff_t, loff_t, int);
	int (*uring_cmd)(struct io_uring_cmd *ioucmd, unsigned int issue_flags);
	int (*uring_cmd_iopoll)(struct io_uring_cmd *, struct io_comp_batch *,
				unsigned int poll_flags);
} __randomize_layout;
```

## **基础元信息**

### `struct module *owner;`

- **作用**：指向实现这套回调的内核模块（一般写 `owner = THIS_MODULE;`）。
- **意义**：内核在执行回调前会对该模块做引用计数，避免卸载中的竞态。
- **注意**：内建（built-in）驱动可置空，但模块形式强烈建议设为 `THIS_MODULE`。

### `int (*open)(struct inode *, struct file *);`

- **时机**：`open(2)` 或首次 `fget`() 时调用。
- **用途**：初始化 `file->private_data`、检查权限/状态、增加设备使用计数等。
- **返回**：0 成功，负错误码失败（如 `-EBUSY`、`-ENODEV`）。

### `int (*release)(struct inode *, struct file *);`

- **时机**：最后一个文件引用关闭时（`close(2)` / `fput`）。
- **用途**：清理 `private_data`、释放资源、降低使用计数。
- **误区**：不是每次 `close(2)` 都会立即调用，需等到最后一个引用释放。

### `int (*flush)(struct file *, fl_owner_t id);`

- **语义**：与 `release` 不同，`flush` 对于同一 `struct file` 可能被多次调用，常用于网络/驱动层同步数据、终止 pending I/O。
- **大多场景可不实现**。

------

## 读写/迭代 I/O

### `ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);`

### `ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);`

- **传统 read/write** 路径（基于单缓冲）。
- **偏移量**：通过 `*ppos`（`loff_t *`）读写并更新；若是非寻址设备（管道/字符设备），通常忽略或使用内部指针。
- **与 iter 关系**：如果实现了 `read_iter`/`write_iter`，VFS 优先选择 iter 版本；否则回退到 `read`/`write`。

### `ssize_t (*read_iter)(struct kiocb *, struct iov_iter *);`

### `ssize_t (*write_iter)(struct kiocb *, struct iov_iter *);`

- **现代零拷贝/聚集 I/O** 接口，支持 `iov_iter`（用户 iovec、内核缓冲、pipe、xarray 等多种后端）。
- **优势**：与 AIO/io_uring/`splice` 等更好集成；推荐**优先实现 iter 版本**。
- **返回**：实际处理的字节数或错误码。

### `int (*iopoll)(struct kiocb *, struct io_comp_batch *, unsigned int flags);`

- **用途**：配合 **io_uring** 的 polled I/O（旋转轮询完成队列），适用于支持轮询完成的设备（如部分 NVMe）。
- **语义**：检查并收割已完成的 I/O，填充 `io_comp_batch`，返回剩余/进度。

------

## 目录遍历

### `int (*iterate)(struct file *, struct dir_context *);`

### `int (*iterate_shared)(struct file *, struct dir_context *);`

- **用途**：实现 `readdir(3)` 语义，向 `dir_context` 回调目录项。
- **差别**：`iterate_shared` 允许并发更友好（读共享锁），是更现代的选择；老式 `iterate` 通常在独占锁下运行。
- **文件系统**：多数真正的文件系统实现 `iterate_shared`。

------

## 定位/锁

### `loff_t (*llseek)(struct file *, loff_t, int);`

- **用途**：实现 `lseek(2)`/`llseek(2)` 逻辑。
- **常用缺省**：
  - `no_llseek`：不支持 seek；
  - `generic_file_llseek` / `default_llseek`：普通文件的标准实现。
- **注意**：必须处理 `SEEK_SET/SEEK_CUR/SEEK_END`；检查越界（返回 `-EINVAL`）。

### `int (*lock)(struct file *, int, struct file_lock *);`

### `int (*flock)(struct file *, int, struct file_lock *);`

- **`lock`**：POSIX 记录锁（`fcntl(F_SETLK)` 等），支持区域锁。
- **`flock`**：BSD 风格整文件锁（`flock(2)`）。
- **文件系统**：由具体 FS 协议/后端实现；字符/块设备一般不实现。

------

## 同步/异步、刷盘

### `int (*fsync)(struct file *, loff_t, loff_t, int datasync);`

- **语义**：把文件指定范围的脏数据/元数据落盘。`datasync!=0` 时可仅保证数据持久性。
- **返回**：0 成功；出错返回负值（如存储介质错误）。
- **注意**：确保 barrier/flush 的正确性（文件系统层通常已实现好）。

### `int (*fasync)(int, struct file *, int);`

- **用途**：支持 `F_SETFL(O_ASYNC)` 和 `SIGIO` 异步通知（tty/网络设备常用）。
- **典型**：字符设备在有数据就绪时向拥有者进程发 `SIGIO`。

------

## 事件与复用

### `__poll_t (*poll)(struct file *, struct poll_table_struct *);`

- **用途**：支持 `poll(2)/select(2)/epoll(7)`。
- **返回**：`POLLIN/POLLOUT/POLLERR/...` 位图。
- **典型写法**：在 `poll_wait(filp, &wq, wait);` 把当前等待者挂到等待队列，条件满足时唤醒；然后返回可读/可写状态位。

------

## 控制/杂项

### `long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);`

- **设备控制命令**：用户空间 `ioctl(fd, cmd, arg)`.
- **unlocked**：不再使用 BKL（大内核锁）；需要你自己做并发保护。
- **约定**：命令号用 `_IO/_IOR/_IOW/_IOWR` 定义；检查 `arg` 指针合法并 `copy_{to,from}_user()`。

### `long (*compat_ioctl)(struct file *, unsigned int, unsigned long);`

- **32 位兼容层**：在 64 位内核上跑 32 位用户程序时，对结构体布局/指针大小不同的 `ioctl` 做转换。
- **仅在需要兼容时实现**。

### `int (*check_flags)(int);`

- **用途**：过滤/调整 `fcntl(F_SETFL)` 设置（例如拒绝不支持的标志）。
- **返回**：0 或负错误码。

### `int (*fadvise)(struct file *, loff_t, loff_t, int);`

- **`posix_fadvise`** 实现钩子，提供访问模式 hint（`POSIX_FADV_*`），便于缓存策略优化。

------

## 映射与地址空间

### `int (*mmap)(struct file *, struct vm_area_struct *);`

- **用途**：实现 `mmap(2)`，把文件/设备映射到用户虚拟地址。
- **常见**：驱动里映射设备寄存器/显存/环形缓冲；文件系统里由 `filemap` 层接管。
- **安全**：根据 `vma->vm_flags` 判断可写/可执行/共享，设置 `vm_ops` 并管理页错误处理。

### `unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);`

- **用途**：为 `mmap` 选地址起点（地址选择策略）。大多数情况下用内核通用实现，特殊硬件/对齐需求才自定义。

### `unsigned long mmap_supported_flags;`

- **用途**：声明该文件支持的 `mmap` 标志位（如 `MAP_SYNC` 等），便于内核快速拒绝不支持的标志。

### `#ifndef CONFIG_MMU`

### `unsigned (*mmap_capabilities)(struct file *);`

- **无 MMU 系统**：声明映射能力（如可执行/可写），嵌入式极简系统才用得到。

------

## 零拷贝/数据通道

### `ssize_t (*sendpage)(struct file *, struct page *, int, size_t, loff_t *, int);`

- **用途**：把页直接“发送”到文件/套接字/设备（历史接口，很多子系统已转向 `splice`/`iter`）。
- **逐渐边缘化**：新代码优先用 `splice`/`iter`。

### `ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);`

### `ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);`

- **用途**：`splice(2)` 的文件端实现，支持**零拷贝**在文件与管道之间搬运数据。
- **优势**：减少用户态缓冲往返拷贝，特别适合大数据传输。

### `ssize_t (*copy_file_range)(struct file *, loff_t, struct file *, loff_t, size_t, unsigned int);`

- **用途**：`copy_file_range(2)`，由内核在文件之间直接拷贝（可能由底层存储 offload）。
- **优势**：可避免把数据搬到用户态再写回，提高效率与一致性。

### `loff_t (*remap_file_range)(struct file *in, loff_t pos_in, struct file *out, loff_t pos_out, loff_t len, unsigned int flags);`

- **用途**：`remap_file_range(2)`，在同一文件或不同文件间“重映射/克隆”区间（快照/写时复制）。
- **示例**：btrfs/xfs 等支持高效“克隆复制”。

------

## 文件租约/预分配/FD 信息

### `int (*setlease)(struct file *, long, struct file_lock **, void **);`

- **用途**：实现 `fcntl(F_SETLEASE)` 文件租约（通知机制，网络文件系统常用）。

### `long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);`

- **用途**：`fallocate(2)` 预留空间、打洞（`FALLOC_FL_PUNCH_HOLE`）、零填等。
- **文件系统**：由 FS 决定如何高效实现。

### `void (*show_fdinfo)(struct seq_file *m, struct file *f);`

- **用途**：向 `/proc/<pid>/fdinfo/<fd>` 输出自定义的 FD 调试信息（通过 seq_file 安全输出）。

------

## io_uring 专用

### `int (*uring_cmd)(struct io_uring_cmd *ioucmd, unsigned int issue_flags);`

### `int (*uring_cmd_iopoll)(struct io_uring_cmd *, struct io_comp_batch *, unsigned int poll_flags);`

- **用途**：面向 `io_uring` 的“直通命令”通道（比如 NVMe passthrough、驱动自定义命令），能更低成本地把用户态命令送达设备。
- **iopoll**：与 polled I/O 配合实现忙轮询完成。

------

### 其他

### `int (*check_flags)(int);`（已上）

### `int (*mmap)`, `mmap_supported_flags`（已上）

------

## 常见实现建议（经验卡片）

- **owner**：模块里务必设 `THIS_MODULE`。
- **读写**：新驱动优先实现 `read_iter/write_iter`；传统 `read/write` 由辅助封装或留空。
- **poll**：记得 `poll_wait()` 注册等待队列，并根据设备状态返回 `POLLIN/OUT` 等位。
- **ioctl**：严格校验用户指针，使用 `copy_{to,from}_user()`；32 位兼容需求时实现 `compat_ioctl`。
- **mmap**：检查 `vma->vm_flags`，正确设置 `vm_ops`，并处理页错误路径；非缓存/强制缓存策略须设置 `pgprot`。
- **fsync/fallocate/copy_file_range/remap_file_range**：文件系统类驱动按 FS 语义实现；字符设备一般不需要。
- **/proc/sysfs 输出**：`seq_printf()`（proc/debugfs）和 `sysfs_emit()`（sysfs）是**首选**，不要手写缓冲长度逻辑。
- **锁语义**：这些回调通常可睡眠（除特殊硬中断上下文调用），自行保证并发安全（mutex/spinlock/atomic）。
- **错误码**：遵循 POSIX/内核约定，返回 `-EFAULT/-EINVAL/-EBUSY/-ENOSPC` 等明确错误。

------



# 第 1 章：从“文件”到 `file_operations`

### 1.1 用户态的直觉：一切皆文件

在 Linux/UNIX 世界里，我们常说“一切皆文件”。这不是一句口号，而是 VFS（Virtual File System，虚拟文件系统）给出的抽象：

- 你打开 `/etc/passwd` —— 得到的是一个普通文件；
- 你打开 `/dev/ttyS0` —— 得到的是一个串口设备；
- 你打开 `/proc/cpuinfo` —— 得到的是一个内核动态生成的伪文件。

在用户态，你不会关心这些对象的差别：**都是 `open/read/write/ioctl/close`**。而内核必须知道“这个文件描述符到底指向什么”，然后调用对应的实现。

------

### 1.2 VFS 的穿针引线

Linux 内核把用户态的系统调用统一收口到 VFS 层。VFS 并不关心对象是文件、目录还是设备，而是依赖每个对象的**操作函数表**来完成具体工作。

这个函数表，就是：

```c
struct file_operations {
	struct module *owner;
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
	ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
	int (*iopoll)(struct kiocb *kiocb, struct io_comp_batch *,
			unsigned int flags);
	int (*iterate) (struct file *, struct dir_context *);
	int (*iterate_shared) (struct file *, struct dir_context *);
	__poll_t (*poll) (struct file *, struct poll_table_struct *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	unsigned long mmap_supported_flags;
	int (*open) (struct inode *, struct file *);
	int (*flush) (struct file *, fl_owner_t id);
	int (*release) (struct inode *, struct file *);
	int (*fsync) (struct file *, loff_t, loff_t, int datasync);
	int (*fasync) (int, struct file *, int);
	int (*lock) (struct file *, int, struct file_lock *);
	ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
	unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
	int (*check_flags)(int);
	int (*flock) (struct file *, int, struct file_lock *);
	ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
	ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
	int (*setlease)(struct file *, long, struct file_lock **, void **);
	long (*fallocate)(struct file *file, int mode, loff_t offset,
			  loff_t len);
	void (*show_fdinfo)(struct seq_file *m, struct file *f);
#ifndef CONFIG_MMU
	unsigned (*mmap_capabilities)(struct file *);
#endif
	ssize_t (*copy_file_range)(struct file *, loff_t, struct file *,
			loff_t, size_t, unsigned int);
	loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in,
				   struct file *file_out, loff_t pos_out,
				   loff_t len, unsigned int remap_flags);
	int (*fadvise)(struct file *, loff_t, loff_t, int);
	int (*uring_cmd)(struct io_uring_cmd *ioucmd, unsigned int issue_flags);
	int (*uring_cmd_iopoll)(struct io_uring_cmd *, struct io_comp_batch *,
				unsigned int poll_flags);
} __randomize_layout;
```

当你调用 `open(2)` 打开某个 inode 时，VFS 会建立一个 `struct file` 对象，里面有个成员 `f_op` 指向这张函数表。之后的 `read/write/ioctl/poll` 等调用，都会通过 `f_op` 找到对应回调。

------

### 1.3 一个简单的类比

你可以把 `file_operations` 理解为 **C++ 虚函数表**：

- `file` 就像一个对象实例；
- `file->f_op` 就像虚表指针；
- `read/write/mmap/ioctl` 就像虚函数。

调用路径大致是这样：

```c
ssize_t vfs_read(...) {
    struct file *f = fget(fd);
    return f->f_op->read(f, buf, count, &pos);
}
```

------

### 1.4 为什么我们需要关心它？

如果你要写：

- 一个字符设备驱动（/dev 下的设备节点），
- 一个内核模块向用户态暴露接口，
- 一个自定义文件系统（FUSE 内核端、procfs、sysfs、debugfs…），

那你都必须提供一份 `struct file_operations`，告诉 VFS：**当用户对这个文件执行操作时，你希望怎么处理。**

------

### 1.5 最小化的例子

来看一个“hello world”风格的字符设备驱动，只实现 `open` 和 `release`：

```c
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>

static int hello_open(struct inode *inode, struct file *file)
{
    pr_info("hello: device opened\n");
    return 0;
}

static int hello_release(struct inode *inode, struct file *file)
{
    pr_info("hello: device released\n");
    return 0;
}

static const struct file_operations hello_fops = {
    .owner   = THIS_MODULE,   /* 必须：防止模块卸载时函数表悬空 */
    .open    = hello_open,
    .release = hello_release,
};

static struct miscdevice hello_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "hello",
    .fops  = &hello_fops,
};

static int __init hello_init(void)
{
    return misc_register(&hello_dev);
}

static void __exit hello_exit(void)
{
    misc_deregister(&hello_dev);
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
```

加载模块后你会得到 `/dev/hello`，打开/关闭时会在 `dmesg` 看到日志。这就是 `file_operations` 的最小触达。

------

### 1.6 小结

- `file_operations` 是 VFS 和具体对象之间的桥梁；
- 并不是所有回调都要实现，只实现你需要的；
- 最小可用的驱动，只需 `.owner/.open/.release`；
- 从这个起点，你可以逐步加入 `read/write/ioctl/poll/mmap` 等操作。

------

 那么我们继续展开，进入 **第 2 章：最常用的成员该怎么写**。这一章是学习 `file_operations` 的“必修课”，因为大多数驱动/伪文件接口都只会用到这些基础成员。

---

# 第 2 章：最常用的成员详解

在本章，我们依次讲解以下常见成员：

- `owner`
- `open`
- `release`
- `llseek`
- `read` / `write`
- `poll`
- `unlocked_ioctl`

通过一个“带缓冲区的字符设备”示例，让你在真实代码里看到它们是如何协同工作的。

------

## 2.1 `.owner` —— 模块的自我保护

### 概念

- 类型：`struct module *owner`

- 作用：防止模块被卸载时，函数表还在使用。

- 常见写法：模块驱动里一律写成：

  ```c
  .owner = THIS_MODULE,
  ```

### 为什么重要？

假如你忘了设置 `.owner`，那么用户在操作设备过程中卸载了模块，VFS 就可能跳到一个已释放的函数地址，引发 **内核崩溃**。

------

## 2.2 `.open` —— 打开设备的入口

### 概念

- 类型：`int (*open)(struct inode *, struct file *)`
- 作用：当用户调用 `open("/dev/xxx")` 时，VFS 会调用这里。
- 常见用途：
  - 初始化 `file->private_data`（存放设备实例或上下文）；
  - 检查资源是否可用（例如设备是否正忙）；
  - 增加设备的使用计数。

### 示例

```c
static int demo_open(struct inode *inode, struct file *filp)
{
    struct demo_state *st = &g_demo;   /* 简单情况下用全局状态 */
    filp->private_data = st;           /* 保存到 private_data，后续 read/write 能直接取 */
    pr_info("demo: device opened\n");
    return 0;
}
```

------

## 2.3 `.release` —— 关闭设备的清理

### 概念

- 类型：`int (*release)(struct inode *, struct file *)`
- 作用：当用户关闭最后一个 `fd` 时调用，用于清理资源。

### 示例

```c
static int demo_release(struct inode *inode, struct file *filp)
{
    pr_info("demo: device released\n");
    return 0;
}
```

------

## 2.4 `.llseek` —— 文件偏移的管理

### 概念

- 类型：`loff_t (*llseek)(struct file *, loff_t, int)`
- 作用：实现 `lseek(2)`，修改 `file->f_pos`。
- 内核提供了默认实现：
  - `no_llseek`：不支持定位；
  - `default_llseek`：常规实现，支持 SEEK_SET/SEEK_CUR/SEEK_END。

### 示例

```c
static loff_t demo_llseek(struct file *filp, loff_t off, int whence)
{
    struct demo_state *st = filp->private_data;
    loff_t newpos;

    switch (whence) {
    case SEEK_SET: newpos = off; break;
    case SEEK_CUR: newpos = filp->f_pos + off; break;
    case SEEK_END: newpos = st->len + off; break;
    default: return -EINVAL;
    }

    if (newpos < 0 || newpos > st->len)
        return -EINVAL;

    filp->f_pos = newpos;
    return newpos;
}
```

------

## 2.5 `.read` / `.write` —— 数据交换的核心

### 概念

- `read`：`ssize_t (*read)(struct file *, char __user *, size_t, loff_t *)`
- `write`：`ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *)`
- 作用：实现用户的 `read(2)`、`write(2)` 调用。
- 注意：
  - 必须使用 `copy_to_user` / `copy_from_user` 与用户空间交换；
  - 返回值是**实际传输的字节数**，错误时返回负数。

### 示例

```c
static ssize_t demo_read(struct file *filp, char __user *ubuf,
                         size_t len, loff_t *ppos)
{
    struct demo_state *st = filp->private_data;
    size_t avail = st->len - *ppos;
    size_t to_copy = min(len, avail);

    if (to_copy == 0)
        return 0; /* EOF */

    if (copy_to_user(ubuf, st->buf + *ppos, to_copy))
        return -EFAULT;

    *ppos += to_copy;
    return to_copy;
}

static ssize_t demo_write(struct file *filp, const char __user *ubuf,
                          size_t len, loff_t *ppos)
{
    struct demo_state *st = filp->private_data;
    size_t space = BUF_SIZE - *ppos;
    size_t to_copy = min(len, space);

    if (to_copy == 0)
        return -ENOSPC;

    if (copy_from_user(st->buf + *ppos, ubuf, to_copy))
        return -EFAULT;

    *ppos += to_copy;
    st->len = max(st->len, (size_t)*ppos);
    return to_copy;
}
```

------

## 2.6 `.poll` —— 事件通知

### 概念

- 类型：`__poll_t (*poll)(struct file *, struct poll_table_struct *)`
- 作用：支持 `select/poll/epoll` 系统调用，告诉用户当前文件是否可读/可写/出错。
- 常用写法：
  - `poll_wait(filp, &queue, wait)` 注册等待队列；
  - 根据条件返回 `POLLIN/POLLOUT/POLLERR`。

### 示例

```c
static __poll_t demo_poll(struct file *filp, struct poll_table_struct *wait)
{
    struct demo_state *st = filp->private_data;
    __poll_t mask = 0;

    poll_wait(filp, &st->wq, wait); /* 注册等待 */

    if (st->len > 0)      /* 有数据可读 */
        mask |= POLLIN | POLLRDNORM;
    if (st->len < BUF_SIZE) /* 有空间可写 */
        mask |= POLLOUT | POLLWRNORM;

    return mask;
}
```

------

## 2.7 `.unlocked_ioctl` —— 控制接口

### 概念

- 类型：`long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long)`
- 作用：处理用户的 `ioctl(2)` 控制命令。
- 定义命令号时用 `_IO/_IOR/_IOW/_IOWR` 宏。
- 需要 `copy_{to,from}_user` 与用户交换数据。

### 示例

```c
#define DEMO_IOC_MAGIC  'd'
#define DEMO_CLEAR  _IO(DEMO_IOC_MAGIC, 0)

static long demo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct demo_state *st = filp->private_data;

    switch (cmd) {
    case DEMO_CLEAR:
        memset(st->buf, 0, BUF_SIZE);
        st->len = 0;
        filp->f_pos = 0;
        return 0;
    default:
        return -ENOTTY; /* 不支持的命令 */
    }
}
```

------

## 2.8 完整的 `file_operations`

```c
static const struct file_operations demo_fops = {
    .owner          = THIS_MODULE,
    .open           = demo_open,
    .release        = demo_release,
    .llseek         = demo_llseek,
    .read           = demo_read,
    .write          = demo_write,
    .poll           = demo_poll,
    .unlocked_ioctl = demo_ioctl,
};
```

到这里，你已经有了一份“基础款” `file_operations`，它能让 `/dev/demo` 设备支持：

- 打开/关闭
- 读写
- `lseek` 定位
- `poll/epoll` 事件通知
- `ioctl` 控制命令

这就是大多数字符设备驱动的常用模式。

------

## 2.9 小结

- `.owner` 保证模块安全；
- `.open` / `.release` 管理资源；
- `.llseek` 提供定位能力；
- `.read` / `.write` 完成数据交换；
- `.poll` 提供事件通知，支持 epoll 等；
- `.unlocked_ioctl` 提供扩展控制。

**这一套，几乎覆盖了 80% 驱动的需求。**

------

很好，那我们把“常用成员”真正落地跑一遍。第 3 章给你一个**可直接编译加载**的字符设备驱动：支持 `open/llseek/read/write/poll/ioctl/release`，并附**用户态测试程序**与**Makefile**。你可以把它当模板，后续按需改造成环形缓冲、DMA、`read_iter` 等。

---

# 第 3 章：完整示例——一个可用的字符设备驱动

## 3.1 目标与设计

我们实现一个 `/dev/fops_demo` 字符设备，具备：

- **读写**：用户写入的数据进入内核缓冲，读出来的一致；
- **定位**：`lseek` 管理读写偏移；
- **事件通知**：支持 `poll/epoll` 等待读；
- **控制面**：`ioctl` 支持清空缓冲、设置长度；
- **线程安全**：`mutex` 保护共享状态；
- **模块安全**：`.owner = THIS_MODULE`。

关键点：

- 使用 **内核 helper**：`simple_read_from_buffer()` / `simple_write_to_buffer()`，避免手写易错的边界与 `copy_to_user`。
- 使用 **等待队列** + **状态位** 实现 `poll`。
- 用 **miscdevice** 注册，省事可靠。

------

## 3.2 内核模块代码（可粘贴）

创建目录 `fops_demo/`，保存为 `fops_demo.c`：

```c
// fops_demo.c  — Linux 6.1 可用
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/mutex.h>

#define DRV_NAME   "fops_demo"
#define BUF_SIZE   4096

/* ioctl 命令 */
#define DEMO_IOC_MAGIC 'D'
#define DEMO_CLEAR   _IO(DEMO_IOC_MAGIC, 0)          /* 清空缓冲区 */
#define DEMO_SETLEN  _IOW(DEMO_IOC_MAGIC, 1, int)    /* 设置有效长度（演示） */

struct demo_state {
	char   buf[BUF_SIZE];
	size_t datalen;          /* 有效数据长度 */
	struct mutex lock;       /* 保护 buf/datalen/f_pos 相关并发 */
	wait_queue_head_t wq;    /* poll 等待队列 */
	bool   readable;         /* 是否有数据可读 */
};

static struct demo_state g;  /* 单实例：所有 fd 共享一份状态 */

/* 打开/关闭 */
static int demo_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &g;
	return 0;
}

static int demo_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 定位：允许在 [0, datalen] 范围内 seek */
static loff_t demo_llseek(struct file *filp, loff_t off, int whence)
{
	struct demo_state *st = filp->private_data;
	loff_t newpos;

	mutex_lock(&st->lock);
	switch (whence) {
	case SEEK_SET: newpos = off; break;
	case SEEK_CUR: newpos = filp->f_pos + off; break;
	case SEEK_END: newpos = st->datalen + off; break;
	default:
		mutex_unlock(&st->lock);
		return -EINVAL;
	}
	if (newpos < 0 || newpos > st->datalen) {
		mutex_unlock(&st->lock);
		return -EINVAL;
	}
	filp->f_pos = newpos;
	mutex_unlock(&st->lock);
	return newpos;
}

/* 读：无数据时阻塞，非阻塞返回 -EAGAIN */
static ssize_t demo_read(struct file *filp, char __user *ubuf,
                         size_t len, loff_t *ppos)
{
	struct demo_state *st = filp->private_data;
	ssize_t ret;

	if (len == 0)
		return 0;

	/* 无数据可读时：阻塞或非阻塞 */
	if (!READ_ONCE(st->readable)) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(st->wq, READ_ONCE(st->readable)))
			return -ERESTARTSYS;
	}

	mutex_lock(&st->lock);
	ret = simple_read_from_buffer(ubuf, len, ppos, st->buf, st->datalen);
	/* 演示：读到尾部就清掉 readable（一次性读取模型） */
	if (*ppos >= st->datalen)
		st->readable = false;
	mutex_unlock(&st->lock);

	return ret;
}

/* 写：把用户数据写到 buf 尾部，扩大 datalen，并唤醒读者 */
static ssize_t demo_write(struct file *filp, const char __user *ubuf,
                          size_t len, loff_t *ppos)
{
	struct demo_state *st = filp->private_data;
	ssize_t ret;

	if (len == 0)
		return 0;

	mutex_lock(&st->lock);
	ret = simple_write_to_buffer(st->buf, BUF_SIZE, ppos, ubuf, len);
	if (ret > 0) {
		if (*ppos > st->datalen)
			st->datalen = *ppos;
		if (!st->readable) {
			st->readable = true;
			wake_up_interruptible(&st->wq);  /* 通知 poll/阻塞读 */
		}
	}
	mutex_unlock(&st->lock);

	return ret;
}

/* poll：注册等待队列，返回当前状态位 */
static __poll_t demo_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct demo_state *st = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &st->wq, wait); /* 将当前进程挂到等待队列上 */

	/* 有数据 → 可读；还有空间 → 可写 */
	if (READ_ONCE(st->readable))
		mask |= POLLIN | POLLRDNORM;
	if (READ_ONCE(st->datalen) < BUF_SIZE)
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}

/* ioctl：清空缓冲 / 设置长度（演示用） */
static long demo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct demo_state *st = filp->private_data;

	switch (cmd) {
	case DEMO_CLEAR:
		mutex_lock(&st->lock);
		memset(st->buf, 0, sizeof(st->buf));
		st->datalen  = 0;
		filp->f_pos  = 0;
		st->readable = false;
		mutex_unlock(&st->lock);
		return 0;
	case DEMO_SETLEN: {
		int newlen;
		if (copy_from_user(&newlen, (void __user *)arg, sizeof(newlen)))
			return -EFAULT;
		if (newlen < 0 || newlen > BUF_SIZE)
			return -EINVAL;
		mutex_lock(&st->lock);
		st->datalen  = newlen;
		if (st->datalen) {
			st->readable = true;
			wake_up_interruptible(&st->wq);
		} else {
			st->readable = false;
		}
		mutex_unlock(&st->lock);
		return 0;
	}
	default:
		return -ENOTTY; /* 不支持的命令 */
	}
}

static const struct file_operations demo_fops = {
	.owner          = THIS_MODULE,
	.open           = demo_open,
	.release        = demo_release,
	.llseek         = demo_llseek,
	.read           = demo_read,
	.write          = demo_write,
	.poll           = demo_poll,
	.unlocked_ioctl = demo_ioctl,
};

static struct miscdevice demo_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = DRV_NAME,
	.fops  = &demo_fops,
	.mode  = 0666, /* 演示方便：所有人可读写 */
};

static int __init demo_init(void)
{
	mutex_init(&g.lock);
	init_waitqueue_head(&g.wq);
	g.datalen  = 0;
	g.readable = false;
	return misc_register(&demo_misc);
}

static void __exit demo_exit(void)
{
	misc_deregister(&demo_misc);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("your-name");
MODULE_DESCRIPTION("fops_demo: reference file_operations device");
module_init(demo_init);
module_exit(demo_exit);
```

------

## 3.3 Makefile（内核外部模块）

同目录新建 `Makefile`。如你在 PC 上本机测试：

```makefile
obj-m := fops_demo.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

如果你要在 i.MX6ULL 交叉编译，把 `KDIR/ARCH/CROSS_COMPILE` 换成你的环境（前面几章已详解，这里从略）。

------

## 3.4 编译与加载

```bash
make
sudo insmod fops_demo.ko
ls -l /dev/fops_demo
dmesg | tail
```

看到 `/dev/fops_demo` 即成功。

------

## 3.5 快速交互（Shell）

```bash
# 写入
echo -n "hello fops" | sudo tee /dev/fops_demo >/dev/null

# 读出（读完一次即清 readable，下一次会阻塞/返回 EAGAIN）
sudo dd if=/dev/fops_demo bs=4 count=3 status=none | hexdump -C

# ioctl 清空（示例：用小工具，下一节提供；或直接 rmmod/insmod）
```

------

## 3.6 用户态测试程序（C）

提供两个小程序：一个阻塞读 + poll 等待；一个执行 ioctl。

### 3.6.1 阻塞读 + poll

保存为 `poll_read.c`：

```c
// gcc -O2 -Wall -o poll_read poll_read.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <errno.h>

int main(void) {
    const char *dev = "/dev/fops_demo";
    int fd = open(dev, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    printf("Waiting for data on %s ...\n", dev);
    int r = poll(&pfd, 1, 10000);   // 10s timeout
    if (r < 0) { perror("poll"); close(fd); return 1; }
    if (r == 0) { printf("timeout\n"); close(fd); return 0; }
    if (pfd.revents & POLLIN) {
        char buf[128];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) { perror("read"); close(fd); return 1; }
        printf("read %zd bytes: \"", n);
        fwrite(buf, 1, n, stdout);
        printf("\"\n");
    }
    close(fd);
    return 0;
}
```

两个终端测试：

1. 终端 A：`sudo ./poll_read` → 等待读
2. 终端 B：`echo "from B" | sudo tee /dev/fops_demo`
3. A 会被唤醒并打印数据。

### 3.6.2 `ioctl` 小工具

保存为 `demo_ioctl.c`：

```c
// gcc -O2 -Wall -o demo_ioctl demo_ioctl.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define DEMO_IOC_MAGIC 'D'
#define DEMO_CLEAR   _IO(DEMO_IOC_MAGIC, 0)
#define DEMO_SETLEN  _IOW(DEMO_IOC_MAGIC, 1, int)

int main(int argc, char **argv) {
    const char *dev = "/dev/fops_demo";
    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    if (argc == 1) {
        if (ioctl(fd, DEMO_CLEAR) < 0) perror("DEMO_CLEAR");
        else puts("cleared");
    } else {
        int n = atoi(argv[1]);
        if (ioctl(fd, DEMO_SETLEN, &n) < 0) perror("DEMO_SETLEN");
        else printf("setlen=%d\n", n);
    }
    close(fd);
    return 0;
}
```

使用：

```bash
# 清空
sudo ./demo_ioctl
# 预设长度（将 readable 置真，便于 poll 测试）
sudo ./demo_ioctl 12
```

------

## 3.7 代码走读与要点回顾

- **状态结构 `demo_state`**：集中管理缓冲、长度、等待队列与锁；所有 fd 共享（单实例设备）。若你需要“每个 fd 一个上下文”，可在 `open` 时分配，将指针放入 `file->private_data`，在 `release` 里释放。
- **`read` 阻塞/非阻塞**：根据 `O_NONBLOCK` 决定直接返回 `-EAGAIN` 还是 `wait_event_interruptible()` 等待。
- **`poll` 三步曲**：`poll_wait()` 注册 → 返回状态位 → 数据就绪时 `wake_up_interruptible()`。
- **`llseek` 边界**：限制在 `[0, datalen]`，避免越界；真实场景下，你可能允许 seek 到尚未写入的数据区，这就需要自定义策略。
- **`ioctl` 规范**：使用 `_IO/_IOR/_IOW/_IOWR` 定义命令；检查用户指针并用 `copy_{to,from}_user()` 交互；不支持的命令返回 `-ENOTTY`。
- **并发保护**：`mutex` 保护共享状态；`READ_ONCE()`/`WRITE_ONCE()` 用于读取简单状态位避免竞态可见性问题（本例对 `readable` 使用 `READ_ONCE` 读取，修改在锁内完成）。

------

## 3.8 常见变形与扩展

- **环形缓冲**：将线性 `buf + f_pos` 改为 `head/tail` 环形队列，`poll` 在队列非空/未满时返回对应位。
- **`read_iter/write_iter`**：将 `read/write` 换成 iter 版本（第 4 章详解），更适配零拷贝与 `io_uring`。
- **`mmap`**：将缓冲映射到用户态（第 5 章）；注意缓存一致性与页权限。
- **多实例设备**：不是一个 `/dev`，而是一组（`minor` 号数组或 class+device 动态创建设备节点）。
- **proc/debugfs**：如只需只读调试信息，考虑用 `seq_file` 与 `debugfs`（更省代码）。

------

## 3.9 小结

本章给出了一份**可运行的 `file_operations` 样板**：安全、清晰、容易扩展。掌握这个模板，你能快速把任何“字节流式”的设备/伪文件原型跑起来，再按需要逐步升级为环形缓冲、iter I/O、mmap 等。

------

太好了，我们正式进入 **第 4 章：读写的现代道路——`read_iter` / `write_iter`**。这一章会把第 3 章的驱动“进化”为 iter 版，实现思路、代码、边界语义、与 AIO/io_uring/零拷贝的关系都会讲清楚。读完这一章，你会知道为什么 **6.x 时代优先实现 `\*_iter`** 是更通用、更具前瞻性的选择。

---

# 第 4 章 读写的现代道路——`read_iter` / `write_iter`

## 4.1 为什么要用 `*_iter`？

传统的 `.read/.write` 每次只处理一段用户缓冲（单个 `void *buf + size`），而现代内核 I/O 越来越多地面对：

- **分散/聚集 I/O**（scatter-gather）：用户传入多个 iovec；
- **与管道/内核缓冲直连**：避免来回 `copy_{to,from}_user`；
- **AIO / io_uring**：异步提交，内核端希望“直接面向迭代器”处理数据；
- **splice/零拷贝**：更容易与“页”为单位的路径结合。

`read_iter(struct kiocb *, struct iov_iter *)` / `write_iter(...)` 的设计，正是为此而来：**把“数据源/目的地”统一抽象成 `struct iov_iter`**。你不必关心它来自用户内存、管道、还是内核缓冲，统一用 `copy_to_iter/copy_from_iter` 等 helper 操作即可。

> 经验法则：**新驱动优先实现 `read_iter`/`write_iter`；旧的 `read`/`write` 可以直接省略**（VFS 会优先调用 iter 版）。

------

## 4.2 必会 API 速览

- `size_t iov_iter_count(const struct iov_iter *iter)`
   剩余可传输的字节数（像 `readable bytes`）。
- `size_t copy_to_iter(const void *kbuf, size_t bytes, struct iov_iter *iter)`
   从内核缓冲拷贝到 iter（用户/管道/内核…），返回实际拷贝字节数。
- `size_t copy_from_iter(void *kbuf, size_t bytes, struct iov_iter *iter)`
   从 iter 拷贝到内核缓冲，返回实际拷贝字节数。
- `bool iov_iter_is_user(const struct iov_iter *iter)` / `iov_iter_is_kvec(...)` / `iov_iter_is_pipe(...)`
   判定迭代器的后端类型（偶尔需要分支优化）。
- `struct file *filp = iocb->ki_filp;`
   通过 `kiocb` 取到 `file`，其他用法与 `.read/.write` 类似。

------

## 4.3 把第 3 章的驱动改造成 iter 版本

我们保留第 3 章的设计：同样的 `demo_state`，同样的阻塞/非阻塞策略、等待队列与 `poll`。只是把 `.read/.write` 改成 `.read_iter/.write_iter`。

**修改点**：

1. `file_operations` 中：去掉 `.read/.write`，加上 `.read_iter/.write_iter`。
2. 内部实现改用 `copy_to_iter`/`copy_from_iter` + `iov_iter_count`。
3. 其余逻辑（等待/唤醒/锁）不变。

### 4.3.1 核心实现

```c
/* 读 iter：无数据阻塞，非阻塞返回 -EAGAIN；一次尽量拷贝更多 */
static ssize_t demo_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
    struct file *filp         = iocb->ki_filp;
    struct demo_state *st     = filp->private_data;
    ssize_t done = 0;

    /* 快速路径：无数据可读，按阻塞/非阻塞决定 */
    if (!READ_ONCE(st->readable)) {
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        if (wait_event_interruptible(st->wq, READ_ONCE(st->readable)))
            return -ERESTARTSYS;
    }

    mutex_lock(&st->lock);
    /* 还能读多少（受限于 datalen 与当前 f_pos） */
    {
        size_t avail = (st->datalen > filp->f_pos) ? (st->datalen - filp->f_pos) : 0;
        size_t want  = min_t(size_t, iov_iter_count(to), avail);

        if (want) {
            done = copy_to_iter(st->buf + filp->f_pos, want, to);
            filp->f_pos += done;
            if (filp->f_pos >= st->datalen)
                st->readable = false; /* 演示：读空后清除可读 */
        }
    }
    mutex_unlock(&st->lock);

    /* 注意：如果 want>0 但 done=0，多半是迭代器端短写，回 -EFAULT 或 -EAGAIN 需结合场景判断。
       此处我们按“没有实际进展”返回 -EAGAIN，促使上层走重试路径。 */
    if (done == 0)
        return (filp->f_flags & O_NONBLOCK) ? -EAGAIN : 0; /* 阻塞场景 0=EOF 语义也可接受 */

    return done;
}

/* 写 iter：尽量把 iter 数据收进内核缓冲，超过容量则回 -ENOSPC */
static ssize_t demo_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
    struct file *filp         = iocb->ki_filp;
    struct demo_state *st     = filp->private_data;
    ssize_t done;

    mutex_lock(&st->lock);
    {
        size_t space = (filp->f_pos < BUF_SIZE) ? (BUF_SIZE - filp->f_pos) : 0;
        size_t want  = min_t(size_t, iov_iter_count(from), space);

        if (want == 0) {
            mutex_unlock(&st->lock);
            return -ENOSPC;
        }

        done = copy_from_iter(st->buf + filp->f_pos, want, from);
        if (done > 0) {
            filp->f_pos  += done;
            if (st->datalen < filp->f_pos)
                st->datalen = filp->f_pos;
            if (!st->readable) {
                st->readable = true;
                wake_up_interruptible(&st->wq); /* 通知读端可读 */
            }
        }
    }
    mutex_unlock(&st->lock);

    return done ? done : -EFAULT; /* iter 端未提供可读数据 */
}
```

### 4.3.2 更新 `file_operations`

```c
static const struct file_operations demo_fops = {
    .owner      = THIS_MODULE,
    .open       = demo_open,
    .release    = demo_release,
    .llseek     = demo_llseek,
    .read_iter  = demo_read_iter,   /* 改这里 */
    .write_iter = demo_write_iter,  /* 改这里 */
    .poll       = demo_poll,
    .unlocked_ioctl = demo_ioctl,
};
```

**测试方法**：和第 3 章完全一致（`echo`、`cat`、`poll_read`、`demo_ioctl`）。iter 版本对用户态是“透明的”，但对 **AIO / io_uring / 大块 iovec** 更友好。

------

## 4.4 语义与返回值：别踩这些坑

- **返回值 = 实际处理的字节数**；出错返回负值。
  - 允许 **短读/短写**：比如缓冲区不够，或迭代器端供给不足。
  - 非阻塞场景没进展时返回 `-EAGAIN` 是常见策略；阻塞场景“无数据可读”返回 `0`（EOF）或阻塞等待再读，取决于你的语义设计。
- **并发与可见性**：
  - 共享标志位可用 `READ_ONCE/WRITE_ONCE` 辅助“可见性”；多字段的一致性用 `mutex` 保护。
  - `poll_wait()` + `wake_up_interruptible()` 成对出现，**唤醒**要发生在**状态变更之后**。
- **`iov_iter_count(to/from)`** 可能为 0（比如用户传入空 iovec），要提前处理。
- **用户空间错误**：`copy_to_iter/copy_from_iter` 失败，返回 0；你应回 `-EFAULT` 或 `-EAGAIN`（视场景决策），别静默吞掉。

------

## 4.5 与 AIO、io_uring 的关系

- `read_iter/write_iter` 与 **AIO**/ **io_uring** 是天然契合的：
  - 这些框架在内核里最终都能以 `iov_iter` 形式“把数据端摆到你面前”。
  - 你的驱动不必关心用户是通过 `read(2)` 还是 `io_uring`；统一处理 iter 即可。
- **忙轮询（iopoll）**：如果你的硬件/队列支持轮询完成，还可以实现 `->iopoll()` / `->uring_cmd_iopoll()`，进一步降低延迟（这属于高阶主题，后续章节再展开）。

------

## 4.6 与 splice/零拷贝的关系

- `splice` 路径也能用 `iov_iter` 抽象（如 pipe-backed iter）。
- 如果你要做 **文件<->管道** 的零拷贝数据泵，考虑实现 `->splice_read/->splice_write`；
   但许多现代场景仅靠 `*_iter` + 合理的 copy helper 已足够；真正追求极致时再引入 `splice` 专用回调。

------

## 4.7 进一步优化与常见变体

- **环形缓冲**：把线性 `f_pos/datalen` 换成 `head/tail`，读写各自前进；`copy_to_iter/copy_from_iter` 可分两段处理“回绕”。
- **大块路径**：当 `iov_iter_is_kvec()` 或“后端就是页”时，可用页级 API（如 `iov_iter_get_pages*` 家族）做 DMA 映射/直接提交（需结合你的 6.1 版本提供的具体 helper），减少中间拷贝。
- **分层设计**：把“拉/推一批字节”的逻辑封装成 `pull_bytes()` / `push_bytes()`，上层 `read_iter/write_iter` 只处理等待/并发/状态更新，底层处理具体搬运细节。

------

## 4.8 小结

- `read_iter/write_iter` 是 6.x 时代的 **首选读写回调**：与分散/聚集、AIO/io_uring、管道/页更好地融合。
- 核心用法很简单：`iov_iter_count` 决定目标字节数，`copy_to_iter/copy_from_iter` 完成搬运。
- 阻塞/非阻塞、等待/唤醒、并发保护的“心法”与 `.read/.write` 完全相通。
- 把第 3 章的样板替换为 iter 版，你的驱动就具备“现代 I/O”的地基了。

------

好 👍 那我们进入 **第 5 章：`mmap` 的正确打开方式**。这一章会教你如何在驱动里支持 `mmap`，把内核缓冲区或者硬件内存区域直接映射到用户空间。重点放在 **正确性、安全性、常见坑**，并通过一个简化示例让你能跑通。

---

# 第 5 章：`mmap` 的正确打开方式

## 5.1 为什么要支持 `mmap`

- **性能需求**：避免 `copy_to_user/copy_from_user` 的双拷贝；
- **硬件特性**：很多设备有 DMA 缓冲或寄存器区域，用户态需要直接访问；
- **共享内存**：驱动和用户态需要共享一块缓冲区，减少上下文切换。

> **例子**：显卡帧缓冲、摄像头视频缓冲、环形队列，都依赖 `mmap`。

------

## 5.2 `mmap` 的回调原型

在 `file_operations` 里：

```c
int (*mmap)(struct file *filp, struct vm_area_struct *vma);
```

- **`filp`**：文件对象；
- **`vma`**：描述映射区域，里面有 `vm_start`、`vm_end`、`vm_flags` 等信息。

你的任务：
 → 把 `vma` 和一块物理/页帧/内核内存建立映射关系。

------

## 5.3 常见的三种映射类型

1. **设备 I/O 内存**（寄存器、DMA buffer）：
    使用 `remap_pfn_range()`。
   - 前提：有物理地址（`pa >> PAGE_SHIFT`）。
   - 常见于 PCIe/SoC MMIO。
2. **内核缓冲 kmalloc/vmalloc 得来的页**：
   - `vmalloc`：用 `remap_vmalloc_range()`;
   - `kmalloc`（物理连续）：可通过 `virt_to_phys` → `remap_pfn_range`。
3. **专用页框**：
    使用 `vm_insert_page()`/`vm_insert_pages()`，逐页插入。

------

## 5.4 示例：共享内核缓冲区

假设我们分配了一块内核缓冲 `kmalloc`，用户态可通过 `mmap` 直接看到它。

### 5.4.1 驱动代码片段

```c
#define BUF_SIZE PAGE_SIZE * 2  /* 8KB */

struct demo_state {
    char *kbuf;
    size_t size;
};

static struct demo_state g;

static int demo_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long pfn;
    unsigned long vsize = vma->vm_end - vma->vm_start;

    if (vsize > g.size)
        return -EINVAL;

    /* 将缓存页映射到用户空间 */
    pfn = virt_to_phys(g.kbuf) >> PAGE_SHIFT;
    if (remap_pfn_range(vma, vma->vm_start, pfn,
                        vsize, vma->vm_page_prot))
        return -EAGAIN;

    pr_info("demo: mmap %lu bytes\n", vsize);
    return 0;
}

static const struct file_operations demo_fops = {
    .owner   = THIS_MODULE,
    .mmap    = demo_mmap,
    /* 其余省略… */
};

static int __init demo_init(void)
{
    g.kbuf = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!g.kbuf) return -ENOMEM;
    g.size = BUF_SIZE;
    /* 注册 miscdevice … */
    return 0;
}

static void __exit demo_exit(void)
{
    kfree(g.kbuf);
    /* 注销设备 … */
}
```

------

## 5.5 用户态测试程序

保存为 `mmap_test.c`：

```c
// gcc -O2 -Wall -o mmap_test mmap_test.c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 8192

int main(void) {
    int fd = open("/dev/fops_demo", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    char *addr = mmap(NULL, BUF_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) { perror("mmap"); return 1; }

    printf("Mapped at %p\n", addr);

    strcpy(addr, "hello from user");
    printf("write ok, now sleeping...\n");
    sleep(2);

    printf("kernel buf = '%s'\n", addr);

    munmap(addr, BUF_SIZE);
    close(fd);
    return 0;
}
```

运行：

```bash
sudo ./mmap_test
```

你会发现内核缓冲和用户态指针 **共享同一块数据**。

------

## 5.6 注意事项与常见坑

1. **页对齐**：`mmap` 映射必须是页粒度（`PAGE_SIZE`），否则失败。
   - 用户传入的 `len` 会被内核页对齐。
   - 你的缓冲区最好也对齐到页边界。
2. **缓存一致性**：
   - 对 DMA 区域要考虑 CPU cache 与设备 cache 的一致性（需要 `dma_alloc_coherent()`，而不是裸 kmalloc）。
   - 否则可能写了数据却设备没看到。
3. **访问权限**：
   - 检查 `vma->vm_flags & VM_WRITE/VM_READ` 决定是否允许写/读；
   - 不要随意映射内核地址，避免越权。
4. **同步问题**：
   - 内核和用户同时读写时，需要额外的锁或协议。
   - 不能假设“用户写入内核立刻看到”，要根据 cache 和编译器优化正确使用 `volatile` 或内存屏障。
5. **安全性**：
   - 切记不要把任意物理内存映射出去。
   - 只映射自己分配/管理的区域。

------

## 5.7 扩展思路

- **vm_ops + 页错误处理**：
   如果缓冲很大，可以按需分配页，在 `fault` 回调里 `vm_insert_page()`，延迟映射。
- **设备寄存器 mmap**：
   比如 `ioremap` 到内核，然后再用 `remap_pfn_range` 提供给用户。
- **结合 `poll`**：
   用户态可同时 `mmap` 数据区 + `poll` 事件通知，常见于视频采集驱动。
- **环形缓冲**：
   常见在高速采集，mmap + ring buffer 让用户态像“读日志”一样消费数据。

------

## 5.8 小结

- `mmap` 提供了高性能的共享内存机制，是很多设备驱动的核心能力。
- 核心函数：`remap_pfn_range()` / `remap_vmalloc_range()` / `vm_insert_page()`。
- 注意页对齐、权限、缓存一致性和同步问题。
- 用户态通过 `mmap(2)` 直接访问数据，大幅减少拷贝和系统调用开销。

------

好，我们继续进入 **第 6 章：目录型接口——`iterate` 与文件系统驱动**。这一章我们换个视角，从“字符设备的字节流”转向“目录项的遍历”，讲清楚 `iterate/iterate_shared` 是怎么被 VFS 调用的、它们的典型应用场景，以及怎么写一个最小化的“虚拟文件系统”示例。

------

# 第 6 章：目录型接口——`iterate` 与文件系统驱动

## 6.1 背景与意义

前几章的 `read/write/mmap` 等操作，主要面向 **字节流文件**（如 `/dev/fops_demo`）。但在 Linux 里，**目录本身也是文件**，只不过它的 `file_operations` 不一样。

当用户执行：

- `ls /myfs`
- `readdir()` 系统调用
- `getdents()` 等函数

VFS 就会调用目录文件的 `.iterate` 或 `.iterate_shared` 回调，要求驱动或文件系统把该目录下的条目一个个“塞给 VFS”。

### 两个版本：

- `.iterate`：旧接口，只能在不加锁的场景下用；
- `.iterate_shared`：较新的推荐接口，支持并发读目录时共享锁（RCU 安全）。

------

## 6.2 接口原型

```c
int (*iterate) (struct file *file, struct dir_context *ctx);
int (*iterate_shared) (struct file *file, struct dir_context *ctx);
```

- **file**：当前打开的目录文件；
- **ctx**：目录上下文，包含当前偏移和回调函数。

关键在于 `dir_emit()` 系列函数，它们用于把一个目录项“交给” VFS。

### 常用函数

- `dir_emit(ctx, name, namelen, ino, type)`
  - 把一个目录项写入用户态缓冲；
- `dir_emit_dots(file, ctx)`
  - 自动写入 `"."` 和 `".."`；
- 返回 `false` 表示用户缓冲已满，应该停止遍历。

------

## 6.3 示例：最小的内存虚拟目录

我们写一个伪文件系统驱动，它在 `/proc` 下创建一个目录 `/proc/demo_dir`，里面有两个“虚拟文件”：`foo` 和 `bar`。用户 `ls /proc/demo_dir` 就能看到它们。

### 6.3.1 核心代码

```c
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define DEMO_DIR_NAME "demo_dir"

/* iterate 回调：告诉内核目录下有哪些项 */
static int demo_iterate(struct file *file, struct dir_context *ctx)
{
    /* 必须先输出 . 和 .. */
    if (!dir_emit_dots(file, ctx))
        return 0;

    /* 然后输出我们自定义的条目 */
    if (!dir_emit(ctx, "foo", 3, 1, DT_REG))
        return 0;
    if (!dir_emit(ctx, "bar", 3, 2, DT_REG))
        return 0;

    return 0;
}

static const struct file_operations demo_dir_fops = {
    .owner = THIS_MODULE,
    .iterate_shared = demo_iterate,  /* 新内核推荐 */
};

/* 初始化时，在 /proc 下注册一个目录 */
static int __init demo_init(void)
{
    struct proc_dir_entry *dir;

    dir = proc_create(DEMO_DIR_NAME, 0555, NULL, &demo_dir_fops);
    if (!dir)
        return -ENOMEM;

    pr_info("demo_dir created under /proc\n");
    return 0;
}

static void __exit demo_exit(void)
{
    remove_proc_entry(DEMO_DIR_NAME, NULL);
    pr_info("demo_dir removed\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("your-name");
MODULE_DESCRIPTION("demo iterate/iterate_shared example");
module_init(demo_init);
module_exit(demo_exit);
```

------

## 6.4 编译与运行

1. 编译内核模块并加载：

   ```bash
   make
   sudo insmod demo_dir.ko
   ```

2. 查看目录：

   ```bash
   ls /proc/demo_dir
   ```

   你会看到：

   ```
   foo  bar
   ```

3. 卸载：

   ```bash
   sudo rmmod demo_dir
   ```

------

## 6.5 `ctx->pos` 与偏移管理

`struct dir_context` 有一个 `loff_t pos` 成员，记录当前目录遍历的偏移。

- 你应该根据 `ctx->pos` 来判断是否已经输出过某些条目。
- 每次调用 `dir_emit()`，会自动推进 `ctx->pos`。
- 这样即使用户 `readdir()` 中途打断、下次继续，也能保持正确位置。

> 如果忽略 `pos`，用户可能会看到重复的目录项。

------

## 6.6 使用场景

- **procfs/debugfs/sysfs**：虚拟文件系统最常见的用途。
- **自定义文件系统**：实现自己的 `super_block` / `inode` / `file_operations`，再通过 `iterate_shared` 列出子文件。
- **容器/虚拟化**：有些内核模块会动态生成“目录结构”来描述资源（如 netns、cgroups）。

------

## 6.7 常见坑

1. **别忘记 `.` 和 `..`**：很多工具依赖它们，推荐直接用 `dir_emit_dots()`。
2. **权限**：目录必须有 `x` 权限才能遍历，否则 `ls` 看不到内容。
3. **多线程安全**：用 `.iterate_shared`，这样多个进程能同时 `ls` 一个目录。
4. **偏移管理**：一定要检查 `ctx->pos`，否则结果混乱。

------

## 6.8 小结

- `iterate/iterate_shared` 让你定义“目录型文件”的内容；
- 本质是：把目录项一个个 `dir_emit()` 给 VFS；
- 推荐使用 `.iterate_shared`，它支持并发更安全；
- 常见于 procfs/debugfs、自定义虚拟文件系统。

------

我们继续进入 **第 7 章：异步与高性能接口——`aio`, `uring_cmd`, `fadvise`**。这一章聚焦在 **Linux 6.x 内核的新一代 I/O 框架**，解释它们如何与 `file_operations` 结合，从而让驱动支持 **异步 I/O、高性能 I/O 提示、甚至用户态驱动接口**。

------

# 第 7 章：异步与高性能接口——`aio`, `uring_cmd`, `fadvise`

## 7.1 背景

传统 `read/write` 是同步阻塞调用：用户发出系统调用 → 内核执行完 → 返回结果。
 但现代 I/O 有几个趋势：

- **异步化**：应用不希望被阻塞，倾向用 `aio` / `io_uring`；
- **批处理/并行化**：一次提交多个请求，减少系统调用开销；
- **高性能优化**：用户态能给驱动“提示”访问模式，驱动据此优化缓存或预取。

内核的 `file_operations` 因此新增了几个关键成员：

- `->uring_cmd`
- `->uring_cmd_iopoll`
- `->fadvise`
- （旧的 `aio_read/aio_write` 已逐渐废弃，转向 `*_iter` + `io_uring`）

------

## 7.2 `uring_cmd` —— io_uring 的驱动扩展

### 接口原型

```c
int (*uring_cmd)(struct io_uring_cmd *ioucmd, unsigned int issue_flags);
```

- **ioucmd**：描述一次来自 io_uring 的命令；
- **issue_flags**：提交标志。

### 特点

- 不再局限于“读写字节流”，而是允许驱动定义自己的命令协议；
- 类似于异步版的 `ioctl`，但集成在 io_uring 的 **SQE/CQE 队列模型**里；
- 返回后，结果通过 CQE（完成队列）异步交给用户态。

### 使用场景

- **存储驱动**：NVMe 就大量使用 `uring_cmd`，因为 NVMe 命令集天然异步；
- **网络/专用设备**：需要低延迟提交/完成队列的硬件。

### 简化示例（伪代码）

```c
static int demo_uring_cmd(struct io_uring_cmd *ioucmd, unsigned int flags)
{
    struct file *filp = ioucmd->file;
    struct demo_state *st = filp->private_data;

    switch (ioucmd->cmd_op) {
    case DEMO_CMD_CLEAR:
        memset(st->buf, 0, BUF_SIZE);
        st->datalen = 0;
        /* 完成通知 */
        io_uring_cmd_done(ioucmd, 0, 0, 0);
        return 0;
    default:
        io_uring_cmd_done(ioucmd, -EOPNOTSUPP, 0, 0);
        return -EOPNOTSUPP;
    }
}
```

这里 `io_uring_cmd_done()` 类似 `complete()`, 它会生成 CQE 通知用户态。

------

## 7.3 `uring_cmd_iopoll` —— 轮询完成路径

### 接口原型

```c
int (*uring_cmd_iopoll)(struct io_uring_cmd *, struct io_comp_batch *,
                        unsigned int poll_flags);
```

- 用于高性能设备，允许应用通过 **忙轮询** 快速获取结果（降低中断延迟）。
- 常见于高速 NVMe / RDMA 驱动。
- 普通驱动很少用，但如果你写的是 **高性能块设备**，这是必修课。

------

## 7.4 `fadvise` —— 用户给内核的 I/O 提示

### 接口原型

```c
int (*fadvise)(struct file *, loff_t offset, loff_t len, int advice);
```

对应用户态 `posix_fadvise()` 系统调用。

- `advice` 提示类型：
  - `POSIX_FADV_NORMAL`：默认；
  - `POSIX_FADV_SEQUENTIAL`：用户打算顺序访问；
  - `POSIX_FADV_RANDOM`：用户随机访问，内核不要预读；
  - `POSIX_FADV_NOREUSE`：数据只访问一次；
  - `POSIX_FADV_WILLNEED`：用户希望预取；
  - `POSIX_FADV_DONTNEED`：用户不再需要缓存的数据。

### 作用

- 给驱动或文件系统一个机会来调整缓存、预取、回收策略；
- 在块设备 / 文件系统驱动里很有用（比如提前做 readahead 或丢掉 page cache）。

### 简单示例

```c
static int demo_fadvise(struct file *filp, loff_t off, loff_t len, int advice)
{
    pr_info("fadvise: offset=%lld, len=%lld, advice=%d\n", off, len, advice);
    /* 简化：什么都不做 */
    return 0;
}
```

------

## 7.5 与传统 AIO 的区别

- **老接口**：`aio_read/aio_write` 已被废弃，维护成本高，且和普通 read/write 语义重复。
- **新接口**：统一使用 `read_iter/write_iter` + io_uring，避免重复代码。
- **扩展命令**：`uring_cmd` 取代了很多“异步 ioctl”的需求。

------

## 7.6 实际驱动中怎么用

1. **如果是字符设备驱动**：
   - 通常不用实现 `uring_cmd`，`read_iter/write_iter` 就够了；
   - 如果你要提供**自定义异步命令接口**（例如 DMA 控制、视频采集参数），才会考虑 `uring_cmd`。
2. **如果是块设备/存储驱动**：
   - `uring_cmd`/`uring_cmd_iopoll` 基本必用；
   - `fadvise` 也常用，用于做读写调度优化。
3. **如果是文件系统驱动**：
   - `fadvise` 用于 readahead / 缓存管理；
   - `iterate_shared` 已在第 6 章讲过，和目录遍历配合。

------

## 7.7 小结

- **`uring_cmd`**：驱动在 io_uring 下暴露自定义命令接口，支持异步通知；
- **`uring_cmd_iopoll`**：提供忙轮询完成机制，适合超低延迟硬件；
- **`fadvise`**：用户空间给驱动/文件系统的“提示”，帮忙优化缓存与 I/O 策略；
- **整体趋势**：传统 AIO 被废弃，统一转向 `*_iter + io_uring`。

------

好 👍 那我们继续进入 **第 8 章：同步控制与文件锁——`lock`, `flock`, `setlease`**。这一章专注于 Linux 文件锁的驱动接口，解释它们与用户态 `flock(2)`、`fcntl(2)` 的关系，以及如何在 `file_operations` 里实现同步控制。

------

# 第 8 章：同步控制与文件锁——`lock`, `flock`, `setlease`

## 8.1 为什么要有文件锁

Linux 是一个多进程、多线程系统：

- 多个进程可能同时访问同一个文件或设备；
- 某些情况下必须确保“互斥”或“顺序”访问（例如数据库文件、串口设备）；
- 用户空间的 `flock(2)`、`fcntl(2)`、`lockf(3)` 等 API 都依赖内核驱动的 **文件锁接口**。

因此，`file_operations` 里提供了三个相关的回调：

```c
int (*lock) (struct file *, int, struct file_lock *);
int (*flock) (struct file *, int, struct file_lock *);
int (*setlease)(struct file *, long, struct file_lock **, void **);
```

------

## 8.2 `lock` —— POSIX 锁

### 概念

- 对应用户态的 `fcntl(fd, F_SETLK, ...)`；
- 支持 **记录锁**（record locking），即锁定文件的某个字节区间（不是整个文件）；
- 常用于数据库、日志文件，多个进程可并发访问不同区间。

### 回调原型

```c
int lock(struct file *filp, int cmd, struct file_lock *fl);
```

- `cmd`：锁操作（F_SETLK, F_GETLK, F_SETLKW）；
- `fl`：描述锁的类型、范围（start, end, type=读/写锁）。

### 驱动如何实现

大多数驱动直接调用 **内核提供的通用文件锁管理**：

```c
#include <linux/fs.h>
#include <linux/filelock.h>

static int demo_lock(struct file *filp, int cmd, struct file_lock *fl)
{
    return posix_lock_file(filp, fl, NULL);
}
```

这样驱动就支持了标准 POSIX 锁行为。

------

## 8.3 `flock` —— BSD 风格锁

### 概念

- 对应用户态的 `flock(fd, LOCK_EX)` / `LOCK_SH`；
- 作用在整个文件，而不是字节区间；
- 实现起来比 `lock` 简单。

### 回调原型

```c
int flock(struct file *filp, int cmd, struct file_lock *fl);
```

- `cmd`：LOCK_SH（共享锁）、LOCK_EX（排它锁）、LOCK_UN（解锁）。

### 驱动实现

通常直接调用内核的 helper：

```c
static int demo_flock(struct file *filp, int cmd, struct file_lock *fl)
{
    return locks_lock_file(filp, fl, cmd);
}
```

------

## 8.4 `setlease` —— 文件租约（Lease）

### 概念

- 对应用户态 `fcntl(fd, F_SETLEASE, F_RDLCK/F_WRLCK/F_UNLCK)`；
- 本质是一种 **文件缓存一致性机制**，常用于 NFS、分布式文件系统：
  - 应用可以“租赁”一个文件，在 lease 期间假设没人改它；
  - 如果有其他进程要访问，内核会通知租约持有者（通过信号），让它释放或降级。

### 回调原型

```c
int setlease(struct file *filp, long arg,
             struct file_lock **flp, void **priv);
```

- `arg`：租约类型（读、写、解除）；
- `flp`：返回租约锁对象；
- `priv`：驱动可存放自定义私有数据。

### 应用场景

- 本地文件系统：内核已有通用实现；
- 分布式文件系统（如 NFS、Ceph）：驱动/内核模块必须实现自己的租约管理，确保一致性。

------

## 8.5 例子：让字符设备支持 flock

在很多简单字符设备驱动中（如串口 `/dev/ttyS0`），我们需要保证只有一个进程能独占设备。可以利用 `flock` 接口做到这一点。

```c
static DEFINE_MUTEX(demo_lock_mutex);
static int device_in_use;

static int demo_open(struct inode *inode, struct file *filp)
{
    mutex_lock(&demo_lock_mutex);
    if (device_in_use) {
        mutex_unlock(&demo_lock_mutex);
        return -EBUSY;
    }
    device_in_use = 1;
    mutex_unlock(&demo_lock_mutex);
    return 0;
}

static int demo_release(struct inode *inode, struct file *filp)
{
    mutex_lock(&demo_lock_mutex);
    device_in_use = 0;
    mutex_unlock(&demo_lock_mutex);
    return 0;
}

/* flock 实现：依赖内核通用 helper */
static int demo_flock(struct file *filp, int cmd, struct file_lock *fl)
{
    return locks_lock_file_wait(filp, fl, cmd);
}

static const struct file_operations demo_fops = {
    .owner  = THIS_MODULE,
    .open   = demo_open,
    .release= demo_release,
    .flock  = demo_flock,
};
```

这样，用户态就能用：

```c
int fd = open("/dev/fops_demo", O_RDWR);
flock(fd, LOCK_EX);  // 独占设备
```

------

## 8.6 常见坑

1. **混淆 flock 与 lock**
   - flock 作用在整个文件；
   - lock/posix lock 可以作用在字节区间。
2. **锁与进程/文件描述符的关系**
   - flock 绑定在文件描述符上；
   - posix lock（fcntl）绑定在进程上。
3. **并发与死锁**
   - `F_SETLKW` 是阻塞锁操作，可能引发死锁，驱动要正确返回 `-EDEADLK`。
4. **租约复杂度**
   - setlease 很少自己实现，除非写分布式文件系统驱动。

------

## 8.7 小结

- `lock`：实现 POSIX 记录锁，精细到字节区间；
- `flock`：实现 BSD 风格整文件锁，常用于设备独占；
- `setlease`：实现文件租约，常见于分布式文件系统；
- 一般驱动只需实现 `flock`（设备独占）或用 `posix_lock_file` 提供基础支持。

------

好 👍 那我们进入 **第 9 章：零拷贝与 splice —— `sendpage`, `splice_read`, `splice_write`, `copy_file_range`**。这一章重点讲解 **内核零拷贝机制** 在 `file_operations` 中的体现，为什么要这样设计，以及驱动里如何实现。

------

# 第 9 章：零拷贝与 splice —— `sendpage`, `splice_read`, `splice_write`, `copy_file_range`

## 9.1 背景：为什么需要零拷贝

普通 I/O 流程：

1. 用户 `read()` → 数据从内核缓冲拷贝到用户缓冲。
2. 用户 `write()` → 再拷贝回内核缓冲，进入目标设备。

这样一来：

- 有 **两次拷贝**（内核→用户、用户→内核）；
- 有 **上下文切换**（用户态↔内核态）；
- 对大文件复制、网络传输极为低效。

零拷贝（Zero-Copy）的目标：**直接在内核态缓冲之间搬运数据，不进入用户态**。

------

## 9.2 相关接口

`file_operations` 里提供了几组零拷贝接口：

1. **`sendpage`**
   - 把页直接从一个文件发送到 socket（常见于网络文件/设备）；
2. **`splice_read` / `splice_write`**
   - 基于管道的零拷贝：文件 ↔ pipe ↔ 文件；
3. **`copy_file_range`**
   - 文件到文件的复制，用户态只发出一个命令，内核内部直接搬运；
4. **`remap_file_range`**
   - 更高级的文件复制/重映射，可能直接调整元数据（而不是数据块）。

------

## 9.3 `sendpage`

### 原型

```c
ssize_t (*sendpage)(struct file *file, struct page *page,
                    int offset, size_t size, loff_t *pos, int more);
```

- 把 `page` 的一部分内容直接“发”到文件/设备。
- 常见实现：网络设备 → socket。

### 使用场景

- 发送大文件到 TCP 连接：
  - 用户态调用 `sendfile()`，VFS 就会触发 `sendpage`。
- 避免中间用户态缓存。

------

## 9.4 `splice_read` / `splice_write`

### 原型

```c
ssize_t (*splice_read)(struct file *, loff_t *ppos,
                       struct pipe_inode_info *, size_t len, unsigned int flags);

ssize_t (*splice_write)(struct pipe_inode_info *,
                        struct file *, loff_t *ppos, size_t len, unsigned int flags);
```

- **splice_read**：从文件读数据，直接写入管道页缓冲；
- **splice_write**：从管道页缓冲直接写入文件。

### 使用场景

- `splice()` 系统调用：用户把文件描述符和 pipe 绑定，实现内核内数据搬运；
- 视频流、日志管道、大规模复制时，极大减少用户态参与。

------

## 9.5 `copy_file_range`

### 原型

```c
ssize_t (*copy_file_range)(struct file *file_in, loff_t pos_in,
                           struct file *file_out, loff_t pos_out,
                           size_t len, unsigned int flags);
```

- 用户态调用 `copy_file_range()` 系统调用时触发；
- 文件到文件的复制，避免用户缓冲区中转；
- 对支持的文件系统（ext4、xfs 等），甚至可以直接拷贝元数据，完全零拷贝。

### 示例（用户态）

```c
int fd1 = open("bigfile", O_RDONLY);
int fd2 = open("copy", O_WRONLY|O_CREAT, 0644);
copy_file_range(fd1, NULL, fd2, NULL, 1<<20, 0);  // 拷贝 1MB
```

------

## 9.6 示例：用 splice 把设备数据送到用户 socket

驱动里实现 `.splice_read`，模拟从内核缓冲搬数据到 pipe：

```c
static ssize_t demo_splice_read(struct file *filp, loff_t *ppos,
                                struct pipe_inode_info *pipe, size_t len,
                                unsigned int flags)
{
    struct demo_state *st = filp->private_data;
    ssize_t ret;

    mutex_lock(&st->lock);
    ret = splice_to_pipe(pipe, st->buf + *ppos,
                         min(len, st->datalen - *ppos));
    if (ret > 0)
        *ppos += ret;
    mutex_unlock(&st->lock);
    return ret;
}
```

这样，用户就能通过：

```bash
cat /dev/fops_demo | nc -l 1234
```

直接把设备缓冲送到 socket，避免用户态拷贝。

------

## 9.7 实际驱动中该怎么做？

1. **字符设备驱动**
   - 一般只实现 `read/write` 就够了；
   - 如果要支持高性能管道/网络交互，可以额外实现 `splice_read`。
2. **文件系统驱动**
   - 必须实现 `copy_file_range`，否则系统调用会回退到普通 `read+write`。
3. **存储驱动**
   - `sendpage`/`splice_write` 可以用来减少网络传输中的拷贝。

------

## 9.8 常见坑

- **页对齐**：零拷贝通常要求页对齐，否则会回退到普通拷贝。
- **内核回退机制**：
  - 如果你的驱动没实现 `splice` / `copy_file_range`，内核会退回到 `read+write`；
  - 但性能会差很多。
- **错误码处理**：必须正确返回 `-EFAULT/-EAGAIN/-ENOSPC`，否则上层逻辑会乱套。

------

## 9.9 小结

- **sendpage**：零拷贝到 socket；
- **splice_read/splice_write**：文件 ↔ pipe ↔ 文件的零拷贝；
- **copy_file_range**：文件到文件的高效复制；
- **remap_file_range**：甚至能做到“元数据级”拷贝。

零拷贝接口不是所有驱动都要实现，但一旦涉及 **大数据传输**（存储、网络、多媒体），就是必修课。

------

好 👍 那我们继续进入 **第 10 章：调试与信息展示——`show_fdinfo`, `seq_file`, `debugfs`**。这一章教你如何把驱动内部状态“优雅地”暴露出来，而不是靠 `printk` 打日志。

------

# 第 10 章：调试与信息展示——`show_fdinfo`, `seq_file`, `debugfs`

## 10.1 背景

驱动开发时常遇到需求：

- 用户想看到设备的内部状态（缓冲区大小、队列深度、统计信息）；
- 开发者调试时需要监控驱动内部变量；
- `printk` 打印在内核日志，难以长期使用，格式也不友好。

Linux 提供了几个机制：

1. **`show_fdinfo`**：为单个文件描述符输出额外信息；
2. **`seq_file`**：标准化的多行输出接口（常用于 `/proc`、`debugfs`）；
3. **`debugfs`**：专门用于调试的虚拟文件系统。

------

## 10.2 `show_fdinfo` —— 每个 fd 的信息

### 回调原型

```c
void (*show_fdinfo)(struct seq_file *m, struct file *f);
```

- 当用户访问 `/proc/<pid>/fdinfo/<fd>` 时调用；
- `seq_file *m` 是输出流，可以 `seq_printf()`；
- `struct file *f` 是对应的文件。

### 示例

```c
static void demo_show_fdinfo(struct seq_file *m, struct file *f)
{
    struct demo_state *st = f->private_data;
    seq_printf(m, "datalen: %zu\n", st->datalen);
    seq_printf(m, "readable: %d\n", st->readable);
}
```

注册到 `file_operations`：

```c
static const struct file_operations demo_fops = {
    .owner = THIS_MODULE,
    .show_fdinfo = demo_show_fdinfo,
    /* 其他成员省略… */
};
```

用户侧查看：

```bash
cat /proc/$(pidof cat)/fdinfo/3
```

就能看到驱动提供的状态信息。

------

## 10.3 `seq_file` —— 格式化输出的黄金标准

### 为什么需要

直接用 `read()` 输出字符串很容易出错（缓冲区不够、seek 混乱）。
 `seq_file` 提供一个统一模式：

- 逐行输出，内核自动管理分页；
- 避免重复代码；
- 常见于 `/proc/net/*`, `/proc/meminfo` 等。

### 使用流程

实现几个回调，挂到 `seq_operations`：

```c
static void *demo_seq_start(struct seq_file *m, loff_t *pos)
{
    if (*pos >= 1) return NULL;
    return (void *)1; /* 任意非 NULL 表示有效 */
}

static void *demo_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
    (*pos)++;
    return NULL;
}

static void demo_seq_stop(struct seq_file *m, void *v) { }

static int demo_seq_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Hello from seq_file, datalen=%zu\n", g.datalen);
    return 0;
}

static const struct seq_operations demo_seq_ops = {
    .start = demo_seq_start,
    .next  = demo_seq_next,
    .stop  = demo_seq_stop,
    .show  = demo_seq_show,
};
```

再通过 `proc_create_seq()` 或者 `debugfs_create_file()` 挂载。

------

## 10.4 `debugfs` —— 驱动开发者的乐园

### 特点

- 专门用于调试，不会出现在生产系统默认挂载点；
- 用户可在 `/sys/kernel/debug/<yourdir>` 看到内容；
- 支持简单的读写接口：`debugfs_create_file`, `debugfs_create_u32`, `debugfs_create_blob` 等。

### 示例

```c
#include <linux/debugfs.h>

static struct dentry *demo_dbg_dir;

static int __init demo_init(void)
{
    demo_dbg_dir = debugfs_create_dir("fops_demo", NULL);
    debugfs_create_u32("datalen", 0444, demo_dbg_dir, &g.datalen);
    debugfs_create_u8("readable", 0444, demo_dbg_dir, (u8 *)&g.readable);
    return 0;
}

static void __exit demo_exit(void)
{
    debugfs_remove_recursive(demo_dbg_dir);
}
```

用户态：

```bash
cat /sys/kernel/debug/fops_demo/datalen
```

直接获取数值。

------

## 10.5 选择哪种方式？

- **`show_fdinfo`**
  - 精细到“某个 fd 的上下文”；
  - 适合描述文件实例相关的状态。
- **`seq_file`**
  - 用于 `/proc` 等标准接口，输出长文本；
  - 适合展示全局统计信息。
- **`debugfs`**
  - 专为调试准备，简单易用；
  - 适合快速验证、开发调试阶段。

------

## 10.6 示例：把 demo 驱动状态展示出来

综合前几章的 `demo_state`：

```c
static void demo_show_fdinfo(struct seq_file *m, struct file *f)
{
    struct demo_state *st = f->private_data;
    seq_printf(m, "datalen: %zu\n", st->datalen);
    seq_printf(m, "readable: %d\n", st->readable);
}
```

在模块初始化时，再加上 `debugfs`：

```c
static struct dentry *demo_dbg;

static int __init demo_init(void)
{
    demo_dbg = debugfs_create_dir("fops_demo", NULL);
    debugfs_create_u32("datalen", 0444, demo_dbg, &g.datalen);
    return 0;
}

static void __exit demo_exit(void)
{
    debugfs_remove_recursive(demo_dbg);
}
```

这样：

- 通过 `/proc/<pid>/fdinfo/<fd>` 可以看到单个文件的状态；
- 通过 `/sys/kernel/debug/fops_demo/` 可以实时观察全局变量。

------

## 10.7 小结

- `show_fdinfo`：精细化到 **文件描述符级别**，用于调试单个 fd；
- `seq_file`：内核推荐的“文本输出框架”，避免 `read()` 出错；
- `debugfs`：调试时最方便的虚拟文件系统。

> 这三者结合，可以让驱动既有 **用户可见的统计信息**，又有 **开发者的调试接口**。

------

好 👍 那我们继续进入 **第 11 章：高级内存管理接口——`get_unmapped_area`, `mmap_supported_flags`, `copy_file_range`, `remap_file_range`**。这一章主要讲解 **文件与虚拟内存的深度交互机制**，以及如何在 `file_operations` 层面影响进程的地址空间布局和文件复制行为。

------

# 第 11 章：高级内存管理接口

## 11.1 背景

前面第 5 章已经介绍了基本的 `mmap`。但在 Linux 内核里，文件还可以进一步参与 **虚拟内存分配策略** 和 **跨文件映射/复制优化**。
 对应的回调有：

- `get_unmapped_area`
- `mmap_supported_flags`
- `copy_file_range`
- `remap_file_range`

------

## 11.2 `get_unmapped_area` —— 控制地址空间选择

### 原型

```c
unsigned long (*get_unmapped_area)(struct file *file,
                                   unsigned long addr,
                                   unsigned long len,
                                   unsigned long pgoff,
                                   unsigned long flags);
```

### 作用

当用户调用 `mmap()` 时，内核需要找到一块合适的虚拟地址区间。

- 默认策略由内核通用的 `mm/mmap.c` 处理；
- 但某些设备或文件需要特殊映射要求，例如：
  - 显卡缓冲必须落在特定对齐区间；
  - 大页内存（hugepage）要求特殊地址；
  - 硬件限制要求地址低于 4G。

此时驱动可以提供自己的 `get_unmapped_area`。

### 示例

```c
static unsigned long demo_get_unmapped_area(struct file *file,
                                            unsigned long addr,
                                            unsigned long len,
                                            unsigned long pgoff,
                                            unsigned long flags)
{
    /* 要求地址必须是 2MB 对齐 */
    unsigned long align = 2 * 1024 * 1024;
    unsigned long area = current->mm->get_unmapped_area(file, addr, len, pgoff, flags);

    if (IS_ERR_VALUE(area))
        return area;

    return ALIGN(area, align);
}
```

------

## 11.3 `mmap_supported_flags` —— 允许哪些 mmap 标志

### 原型

```c
unsigned long (*mmap_supported_flags)(struct file *file);
```

### 作用

用户调用 `mmap()` 时可以传递 `MAP_*` 标志，比如 `MAP_SHARED`, `MAP_PRIVATE`, `MAP_LOCKED` 等。

- 默认情况下，内核会检查这些标志是否合法；
- 但驱动可用 `mmap_supported_flags` 指定“我支持哪些标志”。

### 示例

```c
static unsigned long demo_mmap_supported_flags(struct file *file)
{
    return MAP_SHARED | MAP_PRIVATE | MAP_LOCKED;
}
```

这样，当用户传入不支持的标志，VFS 会拒绝。

------

## 11.4 `copy_file_range` —— 文件间的高效复制

这一点在 **第 9 章**已经讲过基础，这里更深入。

### 原型

```c
ssize_t (*copy_file_range)(struct file *file_in, loff_t pos_in,
                           struct file *file_out, loff_t pos_out,
                           size_t len, unsigned int flags);
```

### 用途

- 避免用户态缓冲；
- 文件系统可以直接复制数据块；
- 存储驱动甚至能调用硬件 offload（比如 NVMe 的 “copy” 命令）。

### 实现思路

大多数自制驱动直接返回 `-EOPNOTSUPP`（不支持），让内核回退到通用路径。
 如果你写文件系统，则需要结合 **页缓存/page cache** 或 **元数据** 完成高效复制。

------

## 11.5 `remap_file_range` —— 文件数据的映射/重定向

### 原型

```c
loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in,
                           struct file *file_out, loff_t pos_out,
                           loff_t len, unsigned int remap_flags);
```

### 区别于 `copy_file_range`

- **copy_file_range**：实际拷贝数据；
- **remap_file_range**：可能直接修改元数据指向（COW 或 reflink），避免真正复制。

### 应用

- 高级文件系统（如 btrfs、xfs）支持 **reflink**：
  - 文件 A 的部分区间“映射”到文件 B；
  - 修改时触发 COW（写时复制）；
- 大文件的快速 clone（比如 `cp --reflink=always`）。

### 示例（文件系统里）

```c
static loff_t demo_remap_file_range(struct file *in, loff_t pos_in,
                                    struct file *out, loff_t pos_out,
                                    loff_t len, unsigned int flags)
{
    pr_info("remap from %lld to %lld, len=%lld\n", pos_in, pos_out, len);
    return -EOPNOTSUPP;  /* 简化：不支持，交给内核回退 */
}
```

------

## 11.6 实际驱动中的意义

- **字符设备驱动**：
  - 一般只用到 `get_unmapped_area`（比如显卡、FPGA 驱动需要特殊内存区域）。
  - `mmap_supported_flags` 可用来限制不合理的 mmap。
- **文件系统驱动**：
  - 需要实现 `copy_file_range` / `remap_file_range`，才能支持现代工具的高效复制。
- **存储驱动**：
  - 有可能通过 `remap_file_range` 做“硬件 offload copy”。

------

## 11.7 常见坑

1. **地址冲突**：`get_unmapped_area` 必须返回不与现有 VMA 冲突的区间，否则 `mmap` 失败。
2. **标志支持不完整**：如果忘记实现 `mmap_supported_flags`，用户可能传入不合理标志，导致不可预期结果。
3. **回退路径**：很多时候要正确返回 `-EOPNOTSUPP`，让内核用默认实现。
4. **安全性**：`remap_file_range` 不可随意允许跨文件映射，否则会破坏隔离性。

------

## 11.8 小结

- **get_unmapped_area**：驱动可决定 mmap 的地址布局；
- **mmap_supported_flags**：限制 mmap 标志，保证安全性；
- **copy_file_range**：高效复制文件内容；
- **remap_file_range**：支持文件区间的重映射/克隆，是现代文件系统的核心。

------

好 👍 那我们继续进入 **第 12 章：异步通知与数据一致性——`fasync`, `fsync`, `flush`**。这一章聚焦在“设备如何主动通知用户进程”以及“如何保证数据真正落盘/写入硬件”的机制。

------

# 第 12 章：异步通知与数据一致性——`fasync`, `fsync`, `flush`

## 12.1 背景

驱动开发中，我们经常遇到两个问题：

1. **异步通知**：
   - 用户不想一直 `poll` 或 `read`，而是希望当数据到来时收到信号；
   - 典型场景：串口、网络设备、输入设备。
2. **数据一致性**：
   - 用户调用 `fsync()` 或关闭文件时，要求数据已经写入设备（而不是只在内核缓存）；
   - 典型场景：存储设备、数据库。

`file_operations` 提供了相应接口：

- `fasync` —— 异步通知
- `fsync`  —— 强制刷新
- `flush`  —— 进程关闭文件时清理

------

## 12.2 `fasync` —— 异步通知

### 原型

```c
int (*fasync)(int fd, struct file *filp, int on);
```

### 用法

- 当用户调用 `fcntl(fd, F_SETFL, O_ASYNC)` 时，内核会调用驱动的 `fasync` 回调；
- 驱动要维护一个 `fasync_struct` 链表；
- 当有事件发生时，用 `kill_fasync()` 给用户进程发 `SIGIO`。

### 示例：字符设备的异步通知

```c
#include <linux/fs.h>
#include <linux/fasync.h>

static struct fasync_struct *demo_async_queue;

static int demo_fasync(int fd, struct file *filp, int on)
{
    return fasync_helper(fd, filp, on, &demo_async_queue);
}

static void demo_notify_event(void)
{
    /* 当有数据可读时调用，通知用户进程 */
    if (demo_async_queue)
        kill_fasync(&demo_async_queue, SIGIO, POLL_IN);
}

static const struct file_operations demo_fops = {
    .owner  = THIS_MODULE,
    .fasync = demo_fasync,
};
```

用户态：

```c
fcntl(fd, F_SETFL, O_ASYNC);        // 打开异步通知
fcntl(fd, F_SETOWN, getpid());      // 设置接收 SIGIO 的进程
```

当驱动调用 `demo_notify_event()` 时，用户进程会收到 `SIGIO`。

------

## 12.3 `fsync` —— 数据同步

### 原型

```c
int (*fsync)(struct file *filp, loff_t start, loff_t end, int datasync);
```

- `start/end`：要求刷新的文件区间；
- `datasync=1`：只保证数据（不强制 metadata）；
- `datasync=0`：数据 + 元数据都要刷新。

### 用法

- 字符设备通常返回 0（无意义）；
- 块设备/文件系统必须实现，把缓存写回硬件。

### 简化示例

```c
static int demo_fsync(struct file *filp, loff_t start, loff_t end, int datasync)
{
    pr_info("demo fsync: start=%lld end=%lld\n", start, end);
    /* 假设立即完成 */
    return 0;
}
```

------

## 12.4 `flush` —— 文件描述符关闭时的钩子

### 原型

```c
int (*flush)(struct file *filp, fl_owner_t id);
```

- 当进程关闭文件描述符时调用（即使 `release` 还没触发）；
- 主要用于“进程级别”的资源清理，比如终止 I/O 请求。

### 与 `release` 的区别

- `release`：当**最后一个** fd 被关闭时调用；
- `flush`：每次 fd 被关闭都会调用（即使还有别的进程打开）。

### 示例

```c
static int demo_flush(struct file *filp, fl_owner_t id)
{
    pr_info("demo flush called (fd closed)\n");
    return 0;
}
```

------

## 12.5 综合示例：带异步通知的输入设备

```c
static struct fasync_struct *demo_async_queue;
static DECLARE_WAIT_QUEUE_HEAD(demo_wq);
static int demo_data_ready;

static int demo_fasync(int fd, struct file *filp, int on)
{
    return fasync_helper(fd, filp, on, &demo_async_queue);
}

/* 数据到来时调用 */
static void demo_new_data(void)
{
    demo_data_ready = 1;
    wake_up_interruptible(&demo_wq);
    kill_fasync(&demo_async_queue, SIGIO, POLL_IN);
}
```

这样用户既可以：

- `poll/select/epoll` 等待事件；
- 也可以依赖 `SIGIO` 信号异步处理。

------

## 12.6 常见坑

1. **忘记 fasync_helper**
   - 必须在 `fasync` 回调里调用 `fasync_helper()`，否则链表不会维护。
2. **并发问题**
   - `kill_fasync()` 必须在合适的上下文调用（中断上下文允许）。
   - 要小心 race condition，可用 `spin_lock` 保护异步队列。
3. **fsync 返回值**
   - 如果不支持，返回 `-EIO` 或 `0`，不要乱返回其他错误。
4. **flush/release 混淆**
   - `flush` 是“每个 fd 关闭”都会调用；
   - `release` 是“最后一个 fd 关闭”时才调用。

------

## 12.7 小结

- `fasync`：支持异步通知（SIGIO），常用于输入/网络/串口类设备；
- `fsync`：保证数据落盘/写入硬件，块设备和文件系统驱动必需；
- `flush`：每次文件关闭时调用，区别于 `release`。

------

好 👍 那我们继续进入 **第 13 章：文件空间管理与范围操作——`fallocate`, `remap_file_range`, `copy_file_range` 的高级应用**。这一章是对前面内容的进阶，重点在于**文件空间的预分配、稀疏文件的管理，以及高效的文件复制与重映射**。

------

# 第 13 章：文件空间管理与范围操作

## 13.1 背景

在现代存储和文件系统中，除了简单的读写，还需要一些**范围级操作**：

- **预分配（Preallocation）**：避免写到一半才分配磁盘块，保证性能和一致性。
- **稀疏文件管理**：允许文件逻辑大小大于实际分配的物理空间。
- **文件克隆与数据共享**：支持快速复制（COW、reflink）。

Linux 为此在 `file_operations` 提供了如下回调：

- `fallocate`
- `copy_file_range`（第 9 章讲过，这里结合存储场景再深入）
- `remap_file_range`（第 11 章初步提过，这里结合实际应用讲）

------

## 13.2 `fallocate` —— 文件空间预分配

### 原型

```c
long (*fallocate)(struct file *file, int mode,
                  loff_t offset, loff_t len);
```

### 功能

- 对应用户态 `fallocate(2)` 系统调用；
- 用来：
  - 提前为文件分配磁盘空间（避免碎片）；
  - 在某个范围打空洞（punch hole，释放物理块）；
  - 设定文件大小。

### 常见 `mode`

- `FALLOC_FL_KEEP_SIZE`：分配块，但不改变文件大小；
- `FALLOC_FL_PUNCH_HOLE`：打空洞，需要与 `FALLOC_FL_KEEP_SIZE` 一起使用；
- `FALLOC_FL_ZERO_RANGE`：将范围置零。

### 示例（文件系统驱动里）

```c
static long demo_fallocate(struct file *file, int mode,
                           loff_t offset, loff_t len)
{
    pr_info("fallocate: off=%lld len=%lld mode=0x%x\n", offset, len, mode);

    if (mode & FALLOC_FL_PUNCH_HOLE) {
        /* 实现 punch hole：释放物理块 */
        return -EOPNOTSUPP;  // 简化：暂不支持
    }

    if (!(mode & FALLOC_FL_KEEP_SIZE)) {
        i_size_write(file_inode(file), offset + len);
    }

    return 0;
}
```

------

## 13.3 `copy_file_range` —— 高效文件复制

在第 9 章我们介绍过零拷贝的 `copy_file_range`。这里补充实际文件系统中的优化：

- **普通文件系统回退**：
  - 如果未实现，内核回退到 `read+write`。
- **高级文件系统优化**：
  - ext4：直接拷贝页缓存 + 元数据更新；
  - xfs / btrfs：支持 `reflink`，不复制数据块，只复制元数据引用。

### 用户态示例

```c
int fd1 = open("file1", O_RDONLY);
int fd2 = open("file2", O_WRONLY|O_CREAT, 0644);
copy_file_range(fd1, NULL, fd2, NULL, 1<<20, 0);  // 复制 1MB
```

如果底层支持 reflink，这个操作几乎是瞬时的。

------

## 13.4 `remap_file_range` —— 文件区间重映射

### 原型

```c
loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in,
                           struct file *file_out, loff_t pos_out,
                           loff_t len, unsigned int remap_flags);
```

### 功能

- 对应用户态 `remap_file_range(2)` 系统调用；
- 允许在文件间 **共享区间（COW）**，而不是复制；
- 常见于 **btrfs/xfs 的 reflink clone**。

### 典型 `remap_flags`

- `REMAP_FILE_DEDUP`：数据去重（不同文件共享同一数据块）；
- `REMAP_FILE_CAN_SHORTEN`：允许截断目标文件。

### 示例（伪代码）

```c
static loff_t demo_remap_file_range(struct file *in, loff_t pos_in,
                                    struct file *out, loff_t pos_out,
                                    loff_t len, unsigned int flags)
{
    pr_info("remap: %lld bytes from %lld -> %lld\n", len, pos_in, pos_out);
    /* 简化：不支持，返回错误 */
    return -EOPNOTSUPP;
}
```

------

## 13.5 场景对比

| 操作               | 行为            | 是否真正复制数据      | 典型应用               |
| ------------------ | --------------- | --------------------- | ---------------------- |
| `write`            | 写入数据        | ✅                     | 普通文件写入           |
| `copy_file_range`  | 文件复制        | 可能复制，也可能优化  | `cp file1 file2`       |
| `remap_file_range` | 文件区间映射    | ❌（可能只复制元数据） | reflink、快照          |
| `fallocate`        | 空间预分配/打洞 | ❌（修改元数据）       | 数据库预分配、稀疏文件 |

------

## 13.6 常见应用案例

1. **数据库系统**
   - 使用 `fallocate` 预分配日志文件，避免运行时碎片。
2. **虚拟机镜像**
   - qcow2/vmdk 文件常用 `fallocate` 管理稀疏空间；
   - `copy_file_range`/`remap_file_range` 用于快照和克隆。
3. **容器存储**
   - overlayfs / btrfs：大量用 `remap_file_range` 做“写时复制”层。

------

## 13.7 小结

- **fallocate**：文件空间预分配或打洞；
- **copy_file_range**：文件复制的零拷贝优化；
- **remap_file_range**：文件区间重映射，现代文件系统实现高效快照/克隆的核心；
- 这三者共同构成了 **文件范围级别的操作接口**，让应用程序和存储子系统之间更高效地协作。

------

好 👍 那我们继续进入 **第 14 章：兼容性与 32 位支持——`compat_ioctl`, `mmap_capabilities`**。这一章讨论 Linux 内核如何在 **64 位内核 + 32 位用户态** 的环境下保持系统调用兼容，驱动在 `file_operations` 中要做哪些适配。

------

# 第 14 章：兼容性与 32 位支持

## 14.1 背景

在 x86_64、ARMv8 等 64 位平台上，Linux 内核支持运行 **32 位用户态程序**。
 这时会遇到几个问题：

- 32 位应用的结构体布局（指针大小、对齐方式）与 64 位不一样；
- `ioctl` 这样的系统调用直接传结构体指针，内核需要识别并转换；
- `mmap` 的能力在无 MMU 系统中也不一样。

为此，`file_operations` 提供了两个接口：

- `compat_ioctl`
- `mmap_capabilities`

------

## 14.2 `compat_ioctl` —— 32 位 ioctl 兼容层

### 原型

```c
long (*compat_ioctl)(struct file *file, unsigned int cmd, unsigned long arg);
```

### 场景

- 用户态是 32 位程序，调用 `ioctl(fd, cmd, arg)`；
- 内核是 64 位，此时 `arg` 指针需要解释成 32 位布局；
- 如果驱动实现了 `compat_ioctl`，内核会优先走它。

### 举例

假设有一个 ioctl 命令传递结构体：

```c
struct demo_data {
    __u32 size;
    __u64 ptr;   /* 用户态指针 */
};
```

在 32 位应用里，`__u64 ptr` 其实是 32 位宽度的指针，需要在 `compat_ioctl` 里转成 64 位指针。

### 示例实现

```c
#ifdef CONFIG_COMPAT
#include <linux/compat.h>

struct compat_demo_data {
    __u32 size;
    __u32 ptr32;   /* 32位指针 */
};

static long demo_compat_ioctl(struct file *filp,
                              unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case DEMO_CMD:
    {
        struct compat_demo_data u32data;
        struct demo_data kdata;

        if (copy_from_user(&u32data, compat_ptr(arg), sizeof(u32data)))
            return -EFAULT;

        kdata.size = u32data.size;
        kdata.ptr  = (u64)(uintptr_t)compat_ptr(u32data.ptr32);

        /* 调用真正的 ioctl 处理函数 */
        return demo_real_ioctl(filp, cmd, (unsigned long)&kdata);
    }
    default:
        return -ENOIOCTLCMD;
    }
}
#endif
```

然后在 `file_operations` 注册：

```c
static const struct file_operations demo_fops = {
    .unlocked_ioctl = demo_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = demo_compat_ioctl,
#endif
};
```

这样，驱动就能同时支持 **64 位 ioctl** 和 **32 位兼容 ioctl**。

------

## 14.3 `mmap_capabilities` —— 无 MMU 系统下的 mmap

### 原型

```c
#ifndef CONFIG_MMU
unsigned (*mmap_capabilities)(struct file *file);
#endif
```

### 背景

- 在没有 MMU 的嵌入式系统里，`mmap` 受限；
- 驱动需要告诉内核：自己支持哪些 mmap 能力。

### 常见返回值

- `0`：不支持；
- `PROT_READ | PROT_WRITE`：支持读写；
- `VM_MAYSHARE`：允许共享。

### 示例

```c
#ifndef CONFIG_MMU
static unsigned demo_mmap_capabilities(struct file *file)
{
    return PROT_READ | PROT_WRITE | VM_MAYSHARE;
}
#endif
```

------

## 14.4 实际驱动中的策略

- **PC/服务器驱动**：
  - 几乎只需要 `compat_ioctl`，因为很多用户空间库可能仍然是 32 位。
  - 常见场景：显卡驱动、输入设备驱动。
- **嵌入式/无 MMU**：
  - 需要实现 `mmap_capabilities`，明确支持哪些 `mmap`。
  - 常见于小型 MCU Linux（如 uClinux）。
- **如果不用**：
  - 如果没有 `compat_ioctl`，32 位应用可能 ioctl 失败；
  - 如果没有 `mmap_capabilities`，`mmap` 调用会直接被拒绝。

------

## 14.5 常见坑

1. **结构体布局不兼容**
   - 指针大小、对齐不同，必须在 `compat_ioctl` 里手动转换。
2. **忽略 CONFIG_COMPAT**
   - 记得用 `#ifdef CONFIG_COMPAT` 包裹，否则 64 位内核不编译时会报错。
3. **无 MMU mmap 混乱**
   - `mmap_capabilities` 必须返回合适的标志，否则用户空间会报 `EINVAL`。

------

## 14.6 小结

- **`compat_ioctl`**：解决 32 位用户态和 64 位内核之间的 ioctl 参数兼容；
- **`mmap_capabilities`**：解决无 MMU 平台下的 mmap 能力声明；
- 这两个接口是 **跨平台/兼容性** 的关键，写驱动时不要忽略。

------

好 👍 那我们进入最后的总结篇： **第 15 章：文件操作的整体设计模式与最佳实践**。这一章会把前面 1–14 章的知识串起来，形成一个完整的 **驱动设计参考**，同时给出不同类型驱动该如何选择、组合 `file_operations` 回调。

------

# 第 15 章：文件操作的整体设计模式与最佳实践

## 15.1 回顾：`file_operations` 的角色

- **桥梁**：连接 **VFS（虚拟文件系统层）** 与 **具体驱动实现**。
- **入口表**：定义了所有可能的文件操作，从 `open` 到 `mmap`，从 `ioctl` 到 `splice`。
- **多态性**：不同驱动实现各自需要的函数指针，未实现的置 `NULL`，内核会选择默认或报错。

它的设计类似于 **C++ 的虚函数表**，但用 C 语言函数指针来表达。

------

## 15.2 驱动类型与典型回调组合

### 1. 字符设备驱动（串口、I²C、GPIO）

- 必须：`open`, `read`, `write`, `release`
- 常用：`poll`, `fasync`（异步通知），`ioctl`（配置）
- 很少用：`mmap`, `splice`（一般不涉及内存映射或高效数据管道）

👉 模式：**简洁 + 响应式**

```c
static const struct file_operations uart_fops = {
    .owner   = THIS_MODULE,
    .open    = uart_open,
    .read    = uart_read,
    .write   = uart_write,
    .release = uart_release,
    .poll    = uart_poll,
    .fasync  = uart_fasync,
    .unlocked_ioctl = uart_ioctl,
};
```

------

### 2. 块设备驱动（硬盘、SD 卡）

- 必须：`open`, `release`, `ioctl`
- 高级：`fsync`（强制落盘），`fallocate`（预分配），`remap_file_range`（快照/克隆）
- 不常用：`read`/`write`（因为通常通过页缓存完成）

👉 模式：**重视数据一致性与范围操作**

------

### 3. 文件系统驱动（ext4, btrfs, nfs）

- 必须：几乎全部接口都可能用到；
- `iterate_shared`（目录遍历）、`mmap`, `get_unmapped_area`, `copy_file_range`, `remap_file_range` 等都必不可少；
- `show_fdinfo`, `seq_file` 等调试接口也常见。

👉 模式：**完整覆盖 + 性能优化**

------

### 4. 内存映射型设备（显卡、FPGA、DSP）

- 必须：`mmap`, `get_unmapped_area`, `mmap_supported_flags`
- 常用：`poll`, `fasync`（事件通知）
- 可选：`ioctl`（配置），`read`/`write`（控制寄存器）

👉 模式：**内存映射 + 异步通知**

------

## 15.3 设计思路与裁剪方法

1. **最小化原则**
   - 只实现真正需要的回调，其余置 `NULL`。
   - 减少维护成本，避免用户态“假接口”误导。
2. **分层思想**
   - `file_operations` 只做入口分发；
   - 具体逻辑封装在独立模块函数（如 `device_read()`, `device_write()`）。
   - 方便调试与复用。
3. **一致性原则**
   - 类似驱动应暴露一致接口，例如所有 UART 驱动都应支持 `ioctl(TIOCGSERIAL)`。
   - 方便用户空间应用和库的兼容。

------

## 15.4 开发与调试建议

- **调试接口**：善用 `debugfs` 和 `seq_file`，不要依赖 `printk`。
- **用户态模拟**：写简单的 `cat`, `echo`, `dd`, `ioctl` 测试程序。
- **逐步扩展**：先实现 `open/read/write/release`，确认工作后再加高级功能。
- **文档参照**：
  - 《Linux Device Drivers, 3rd Edition》（LDD3）
  - 内核文档 `Documentation/filesystems/` 与 `Documentation/core-api/`
  - 源码：`fs/*`、`drivers/*` 目录中的真实驱动

------

## 15.5 常见陷阱与最佳实践

1. **阻塞与非阻塞**
   - `read`/`write` 应正确处理 `O_NONBLOCK`。
2. **并发问题**
   - 用 `mutex`, `spinlock`, `wait_queue` 保证线程安全。
3. **错误码**
   - 合理返回 `-EINVAL`, `-EFAULT`, `-ENOTTY` 等标准错误码。
   - 避免自造错误码，用户态无法识别。
4. **长生命周期资源**
   - 在 `open` 分配，在 `release` 释放。
   - 避免内存泄漏。

------

## 15.6 总结

- `file_operations` 是 **Linux 驱动与 VFS 的契约**；
- 驱动根据自身定位裁剪接口，不必“全家桶”；
- 通过 `debugfs`, `seq_file`, `fasync` 等接口，可以让驱动更健壮、更易调试；
- 不同驱动类型有不同“推荐组合模式”，掌握后可以快速设计文件操作层。

------

📌 至此，我们完成了 **《深入理解 Linux 内核 file_operations》** 全部 **15 章** 的讲解。从基本概念到高级内存管理，从同步/异步 I/O 到调试与兼容性，形成了一本系统化的参考说明书。

