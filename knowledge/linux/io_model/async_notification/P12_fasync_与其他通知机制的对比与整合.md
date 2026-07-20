---
id: knowledge.linux.io_model.async_notification.异步通知简介.p12_fasync_与其他通知机制的对比与整合
title: "fasync 与其他通知机制的对比与整合"
kind: mechanism
status: evolving
domains:
  - linux
  - kernel
---

# 第12章\_fasync\_与其他通知机制的对比与整合

## 12.1\_与其他通知机制的对比与整合

（章节内容说明）

本章不再孤立地讨论 fasync，而是把它放回 Linux 整体 I/O 事件通知体系之中，对比并串联：

- 传统的 `select` / `poll` / `epoll`；
- input 子系统 / netlink 等专用事件模型；
- 以及 `io_uring` / AIO 等更“现代”的异步 I/O 机制。

目标是从 **机制定位** 和 **工程选型** 两个层面回答三个问题：

1. fasync 在整个通知机制族谱里到底处于什么位置？
2. 在现实项目中，什么时候用 fasync，什么时候只用 poll/epoll 就够了？
3. 如何把 fasync 和其他机制 **组合起来**，构建统一的事件循环，而不是做出“孤岛式”的驱动/应用设计？

本章的结构安排如下：

- **12.1**：从“适用范围与取舍”的角度，对比 fasync 与 `select` / `poll` / `epoll`；
- **12.2**：对比 fasync 与 input 子系统事件模型（`input_report_* + evdev`）；
- **12.3**：对比 fasync 与 netlink / kobject_uevent / sysfs 轮询等“系统级通知”；
- **12.4**：讨论 fasync 与 `io_uring` / AIO 等新型异步 I/O 机制之间的关系与边界；
- **12.5**：给出一个面向工程实践的“通知机制决策矩阵”，并说明几种推荐组合。

本批先完整展开 **12.1 小节**。

------

### 12.1.1\_fasync\_vs\_select/poll/epoll\_适用范围与取舍

#### (1)\_引入\_为什么要单独对比这三者

在第 2 章你已经系统看过：

- 阻塞 I/O 的行为和局限；
- `select` / `poll` / `epoll` 的事件等待模型；
- `file_operations.poll` 与 `wait_queue_head_t` 的基本用法。

在前几章，我们又从驱动角度把 fasync 拆开研究。

在实际工程里，最常见的犹豫是下面几种：

- “我已经在驱动里实现了 `.poll`，还需要 fasync 吗？”
- “用户态已经有一个 epoll 循环，再加 SIGIO 会不会很乱？”
- “有没有可以完全只用 epoll、彻底不用 fasync 的方案？”
- “fasync 看起来更‘主动’，是不是比 poll 更高级？”

要把这些问题回答清楚，需要从三个维度比较：

1. **事件感知路径**：内核怎样判断“可读/可写”，怎样通知用户；
2. **状态管理责任**：驱动负责什么、用户负责什么；
3. **扩展性与组合性**：当 fd 数量增多/场景复杂时（多进程、多线程、多设备），行为是否仍然可控。

本小节围绕这三个维度，把 fasync 与 `select` / `poll` / `epoll` 的 **作用边界** 和 **典型使用模式** 讲清楚，并给出一个“选型表”和一套最小对照示例。

------

#### (2)\_数据结构视角\_四种路径背后的核心结构

从内核实现角度看，这四种机制涉及到的关键数据结构可以粗略对应为：

| 机制   | 用户态接口                       | 核心内核结构（简化视角）                                  |
| ------ | -------------------------------- | --------------------------------------------------------- |
| select | `select()`                       | `fd_set` → 内核临时映射 → `poll_table`/waitqueue          |
| poll   | `poll()` / `ppoll()`             | `struct poll_list` / `poll_wqueues` + `wait_queue_head_t` |
| epoll  | `epoll_create` / `epoll_wait`    | `struct eventpoll` + epoll 树/红黑树/ready list 等        |
| fasync | `fcntl(F_SETOWN/F_SETFL)` + 信号 | `struct fasync_struct` 链表 + 进程 `f_owner`/信号队列     |

驱动侧看到的是：

- 对 `select/poll/epoll` 来说，**统一映射到 `.poll` 回调 + waitqueue**：

  ```c
  __poll_t demo_poll(struct file *filp, poll_table *wait)
  {
  	struct demo_async_dev *dev = filp->private_data;

  	poll_wait(filp, &dev->wait, wait);

  	if (demo_has_data(dev))
  		return EPOLLIN | EPOLLRDNORM;

  	return 0;
  }
  ```

  内核在实现 `select` / `poll` / `epoll` 时，只要调用 `f_op->poll`，并把内部的 `poll_wqueues` / `poll_table` 传下来，驱动根本不需要区分“上层到底是 select 还是 epoll”。

- 对 fasync 来说，驱动看到的是：

  - `.fasync` 回调 + `fasync_helper()` 管理 `struct fasync_struct` 链表；
  - 事件路径中调用 `kill_fasync()` 触发信号。

**重要结论：**

> 从驱动角度看：
>
> - `select` / `poll` / `epoll` → 统一对应 `.poll + waitqueue`；
> - fasync → 对应 `.fasync + fasync_struct` 链表 + SIGIO 语义。
>
> 它们在 **内核抽象层面是两套平行机制**，共享的是“同一个字符设备 fd 上的事件状态”。

------

#### (3)\_开发者视角\_驱动实现上的对比与组合方式

从驱动作者的视角，可以从下面问题出发对比：

1. **驱动要不要实现 `.poll`？**
   - 只要该设备有“可读/可写”这样的状态变化，**实现 `.poll` 基本属于“标配”**；
   - `.poll` 让 `select` / `poll` / `epoll` 都能工作，是最通用的接口；
   - 成本并不高：一个 `wait_queue_head_t` + 一个状态位/计数即可。
2. **驱动要不要实现 fasync？**
   - 典型适用场景：
     - **少数 fd + 事件稀疏，但希望“尽快提醒”的场景**（例如报警、GPIO 外部事件）；
     - 或者 **应用已经有 signal/signalfd 体系，想把设备事件并入 SIGIO 流程**；
   - 不适合 fasync 单独使用的场景：
     - 高频数据流（串口高速收发、音频连续采集）只用 SIGIO 通知会非常吵；
     - 大规模 fd 监控（上千 socket）中，纯 signal 模型可维护性较差。
3. **“只实现 `.poll` 不实现 fasync” vs “同时实现两者”**
   - **只实现 `.poll`**：
     - 优点：简单、通用，兼容所有基于 `select/poll/epoll` 的上层框架；
     - 缺点：应用如果只依赖阻塞 `read` 或 `epoll_wait`，可能在“低频但高重要性事件”上感知不够“主动”，但这本质上是应用事件循环设计问题。
   - **同时实现 `.poll` 和 fasync（推荐）**：
     - `.poll` 提供统一事件状态接口（`EPOLLIN` 等）；
     - fasync 提供 “主动推送式” 通知；
     - 驱动中可以确保“事件状态”只维护一份（例如 `event_count` + waitqueue + async_queue），避免两套状态机。

一个典型的驱动结构是：

- `demo_async_dev` 内部维护：
  - `wait_queue_head_t wait`；
  - `struct fasync_struct *async_queue`；
  - `event_count` 等状态；
- `.read` / `.poll` / `.fasync` / 中断处理函数都围绕这几项状态展开。

后续的示例代码会给出一个对照模板。

------

#### (4)\_用户/平台视角\_接口层对比和选型思路

从用户态 API 的角度，对比可以汇总为：

| 机制         | 典型调用方式                                  | 优点                                       | 局限与注意点                              |
| ------------ | --------------------------------------------- | ------------------------------------------ | ----------------------------------------- |
| select       | `select(nfds, &rfds, ...)`                    | 跨平台、古老 API，简单                     | fd 数量有限制；接口冗长，不适合大规模场景 |
| poll         | `poll(struct pollfd *fds, nfds, timeout)`     | 比 select 清晰；支持更大 fd 数量           | 每次调用复制 `pollfd` 数组，开销相对较大  |
| epoll        | `epoll_wait` / `epoll_ctl`                    | 大规模 fd 监控标准工具；O(活跃 fd 数量)    | 只在 Linux；接口相对复杂                  |
| fasync+SIGIO | `fcntl(F_SETOWN/F_SETFL)` + signal / signalfd | 驱动“主动推事件”，对少量 fd 的通知非常直接 | 需要处理信号路由/掩码问题，多线程下较复杂 |

组合使用时几种常见模式：

1. **只用 epoll**
   - 驱动实现 `.poll`，用户态建立 `epoll` 事件循环；
   - 适合“很多 fd + 统一事件循环”的服务型/守护进程场景。
2. **fasync + signalfd + epoll**
   - 驱动使用 fasync 把事件转为 SIGIO；
   - 用户态把 SIGIO 加入一个 `signalfd`；
   - `signalfd` 再加入 epoll；
   - 最终所有事件（socket、设备、SIGIO）统一在一个 epoll 循环中处理。
3. **SIGIO 直连 handler（无 epoll）**
   - 进程不需要 epoll，只要“事件一来马上打断当前逻辑”，就直接用 SIGIO handler；
   - 适合简单工具/测试程序；
   - 不适合复杂业务 + 多线程场景。

在后面的 12.4 和 13 章会再给完整组合案例，这里只先把对比框架搭起来。

------

#### (5)\_可视化\_从驱动到用户的四条路径关系图

用一个简单的示意图，把四种机制的“路径关系”画出来（驱动视角统一）：

```mermaid
flowchart LR
    "DEV"["DEV: demo_async_dev\n(事件状态: event_count 等)"]
    "WQ"["WQ: wait_queue_head_t\n(dev->wait)"]
    "FAS"["FAS: fasync_struct 链表\n(dev->async_queue)"]

    "FOPS"["file_operations\n(.read/.poll/.fasync)"]

    "SYS_SELECT"["内核: sys_select/sys_pselect"]
    "SYS_POLL"["内核: sys_poll/ppoll"]
    "SYS_EPOLL"["内核: epoll_wait / eventpoll"]
    "SYS_SIG"["内核: 信号子系统\n(send_sigio, pending 队列)"]

    "U_SELECT"["用户: select()"]
    "U_POLL"["用户: poll()"]
    "U_EPOLL"["用户: epoll_wait()"]
    "U_SIG"["用户: SIGIO handler / signalfd"]

    "DEV" --> "WQ"
    "DEV" --> "FAS"

    "FOPS" --> "WQ"
    "FOPS" --> "FAS"

    "SYS_SELECT" --> "FOPS"
    "SYS_POLL" --> "FOPS"
    "SYS_EPOLL" --> "FOPS"
    "SYS_SIG" --> "FAS"

    "U_SELECT" --> "SYS_SELECT"
    "U_POLL" --> "SYS_POLL"
    "U_EPOLL" --> "SYS_EPOLL"
    "U_SIG" --> "SYS_SIG"
```

阅读要点：

- `select` / `poll` / `epoll` 在内核最终都走到 **同一个 `.poll` 回调 + waitqueue**；
- fasync 的 `.fasync` / `kill_fasync` 通过 `dev->async_queue` 连接到信号子系统；
- 驱动侧只维护一份事件状态（`DEV`），上层走哪条路径，取决于用户态如何调用。

------

#### (6)\_示例代码\_同时支持.poll\_与\_fasync\_的最小驱动\_+\_用户态对照

下面给一个“最小可用”的驱动接口片段，只展示核心逻辑（省略字符设备注册等样板），重点在：

- 驱动中同时实现 `.poll` 和 `.fasync`；
- 事件路径（例如中断）中既唤醒 waitqueue，又调用 `kill_fasync()`；
- 用户态分别用 `epoll` 和 `SIGIO` 两种方式感知事件。

##### 1)\_驱动核心片段

```c
/* demo_async_core.c */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched/signal.h>
#include <linux/spinlock.h>

#define DEMO_DEV_NAME               "demo_async"

struct demo_async_dev {
	wait_queue_head_t	wait;		/* 供 poll/select/epoll 使用 */
	spinlock_t		lock;		/* 保护 event_count/async_queue */
	struct fasync_struct	*async_queue;	/* fasync 链表 */

	unsigned int		event_count;	/* 等待处理的事件个数 */
};

static struct demo_async_dev demo_dev;

static bool demo_has_data(struct demo_async_dev *dev)
{
	return dev->event_count > 0U;
}

static ssize_t demo_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct demo_async_dev *dev = filp->private_data;
	unsigned long flags;
	unsigned int events = 0U;
	int ret;

	if (count < sizeof(events))
		return -EINVAL;

	if (wait_event_interruptible(dev->wait, demo_has_data(dev)))
		return -ERESTARTSYS;

	spin_lock_irqsave(&dev->lock, flags);

	if (dev->event_count > 0U) {
		events = dev->event_count;
		dev->event_count = 0U;
	}

	spin_unlock_irqrestore(&dev->lock, flags);

	if (events == 0U)
		return 0;

	ret = copy_to_user(buf, &events, sizeof(events));
	if (ret)
		return -EFAULT;

	return sizeof(events);
}

static __poll_t demo_poll(struct file *filp, poll_table *wait)
{
	struct demo_async_dev *dev = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &dev->wait, wait);

	if (demo_has_data(dev))
		mask |= EPOLLIN | EPOLLRDNORM;

	return mask;
}

static int demo_fasync(int fd, struct file *filp, int on)
{
	struct demo_async_dev *dev = filp->private_data;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&dev->lock, flags);
	ret = fasync_helper(fd, filp, on, &dev->async_queue);
	spin_unlock_irqrestore(&dev->lock, flags);

	return ret;
}

static int demo_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &demo_dev;
	return 0;
}

static int demo_release(struct file *filp)
{
	/* 清除 fasync 状态 */
	demo_fasync(-1, filp, 0);
	return 0;
}

/* 事件发生时由中断或工作队列调用 */
static void demo_event_raise(struct demo_async_dev *dev)
{
	unsigned long flags;
	bool notify = false;

	spin_lock_irqsave(&dev->lock, flags);

	dev->event_count++;

	wake_up_interruptible(&dev->wait);

	if (dev->async_queue)
		notify = true;

	spin_unlock_irqrestore(&dev->lock, flags);

	if (notify)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
}

static const struct file_operations demo_fops = {
	.owner		= THIS_MODULE,
	.open		= demo_open,
	.release	= demo_release,
	.read		= demo_read,
	.poll		= demo_poll,
	.fasync		= demo_fasync,
};

static int __init demo_init(void)
{
	/* 注册字符设备流程略 */
	init_waitqueue_head(&demo_dev.wait);
	spin_lock_init(&demo_dev.lock);
	demo_dev.async_queue = NULL;
	demo_dev.event_count = 0U;

	return 0;
}

static void __exit demo_exit(void)
{
	/* 注销字符设备流程略 */
}

module_init(demo_init);
module_exit(demo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("demo: poll + fasync example");
```

> 注意：这里没有涉及 `request_irq` / `devm_request_irq`，所以暂不展开 devres 对比。
>  真实项目里如果事件来自中断，一般会用 `devm_request_irq()` 或 `request_irq()` 绑定 `demo_event_raise()` 的上半部/下半部。

##### 2)\_用户态\_epoll\_示例

```c
/* demo_epoll.c: 使用 epoll 读取 /dev/demo_async */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define DEMO_EPOLL_MAX_EVENTS       4

int main(void)
{
	const char *dev_path = "/dev/demo_async";
	int fd;
	int epfd;
	struct epoll_event ev, events[DEMO_EPOLL_MAX_EVENTS];
	unsigned int events_count;
	int n, i;

	fd = open(dev_path, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	epfd = epoll_create1(0);
	if (epfd < 0) {
		perror("epoll_create1");
		return EXIT_FAILURE;
	}

	ev.events = EPOLLIN;
	ev.data.fd = fd;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		perror("epoll_ctl ADD");
		return EXIT_FAILURE;
	}

	for (;;) {
		n = epoll_wait(epfd, events, DEMO_EPOLL_MAX_EVENTS, -1);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			perror("epoll_wait");
			break;
		}

		for (i = 0; i < n; i++) {
			if (events[i].data.fd == fd &&
			    (events[i].events & EPOLLIN)) {
				ssize_t r = read(fd, &events_count,
						 sizeof(events_count));
				if (r == sizeof(events_count)) {
					printf("epoll: events_count=%u\n",
					       events_count);
				}
			}
		}
	}

	close(epfd);
	close(fd);
	return EXIT_SUCCESS;
}
```

##### 3)\_用户态\_SIGIO\_示例(与\_epoll\_对比)

```c
/* demo_sigio_simple.c: 使用 SIGIO 接收同一设备的事件 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

static int g_demo_fd = -1;

static void demo_sigio_handler(int sig, siginfo_t *info, void *ucontext)
{
	unsigned int events_count = 0U;
	ssize_t r;

	(void)ucontext;

	if (sig != SIGIO)
		return;

	if (info && info->si_fd != g_demo_fd)
		return;

	r = read(g_demo_fd, &events_count, sizeof(events_count));
	if (r == sizeof(events_count)) {
		printf("SIGIO: events_count=%u\n", events_count);
	}
}

int main(void)
{
	const char *dev_path = "/dev/demo_async";
	struct sigaction sa;
	int flags;
	int ret;

	g_demo_fd = open(dev_path, O_RDONLY | O_NONBLOCK);
	if (g_demo_fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = demo_sigio_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGIO, &sa, NULL) < 0) {
		perror("sigaction");
		return EXIT_FAILURE;
	}

	ret = fcntl(g_demo_fd, F_SETOWN, getpid());
	if (ret < 0) {
		perror("fcntl(F_SETOWN)");
		return EXIT_FAILURE;
	}

	flags = fcntl(g_demo_fd, F_GETFL);
	if (flags < 0) {
		perror("fcntl(F_GETFL)");
		return EXIT_FAILURE;
	}

	ret = fcntl(g_demo_fd, F_SETFL, flags | O_ASYNC);
	if (ret < 0) {
		perror("fcntl(F_SETFL, O_ASYNC)");
		return EXIT_FAILURE;
	}

	printf("waiting SIGIO on %s, pid=%d, fd=%d\n",
	       dev_path, getpid(), g_demo_fd);

	for (;;) {
		pause();
	}

	close(g_demo_fd);
	return EXIT_SUCCESS;
}
```

通过这两个用户态程序，可以直观对比：

- 相同的驱动接口（`.read + .poll + .fasync`）；
- 使用 epoll vs 使用 SIGIO 时，应用结构/心智模型的差异。

------

#### (7)\_调试与验证\_如何确认四种机制在同一个驱动上行为一致

对同一设备，确认 select/poll/epoll/fasync 行为一致性时，可以按顺序检查：

1. **从 `.poll` 路径验证**
   - 使用 `poll` 或 `epoll` 的简单测试程序；
   - 确认事件发生时 `.poll` 只在有数据时返回 `EPOLLIN`；
   - 确保 `event_count` 在 `read()` 后重置，`poll` 不会“空转”。
2. **从 `.fasync` 路径验证**
   - 使用 SIGIO 测试程序；
   - 配合 `strace` 检查 `F_SETOWN / F_SETFL(O_ASYNC)` 调用；
   - 确认事件发生时 `kill_fasync` 被调用（用 ftrace）。
3. **交叉验证**
   - 同时运行 epoll 和 SIGIO 版本，触发事件；
   - 确认在一次事件中：
     - `.poll` 报告一次可读；
     - `SIGIO` 至少触发一次；
     - `read()` 看到的 `events_count` 与期望一致。
4. **利用前面章节的工具链**
   - `/proc/PID/status` / `/proc/PID/fdinfo` → 看 FASYNC 状态；
   - `strace` → 看 fcntl / signal 调用；
   - `ftrace` → 看 `.poll` / `.fasync` / `kill_fasync()` 的调用轨迹。

------

#### (8)\_小结\_在\_通用事件接口\_与\_主动信号通知\_之间做清晰分工

本小节围绕 fasync 与 `select` / `poll` / `epoll` 的关系，给出几个结论：

1. **从驱动角度：`.poll` 是“通用接口”，fasync 是“可选增强”**
   - `.poll` + waitqueue 是面向 `select/poll/epoll` 的统一桥梁；
   - fasync 则通过 `.fasync + kill_fasync` 把事件转为 SIGIO；
   - 二者应共享同一份事件状态（如 `event_count`），而不是维护两套独立逻辑。
2. **从用户态角度：epoll 是“事件复用核心”，fasync 适合作为“信号源”**
   - 对复杂服务型程序，推荐用 epoll 统一管理 socket/pipe/设备 fd；
   - fasync 适合用 SIGIO（再通过 signalfd）接入现有 epoll 循环；
   - 直接使用 SIGIO handler 适合简单 demo 和测试，不适合作为复杂系统的主入口。
3. **选型建议（粗略版）**
   - 只需要简单阻塞 I/O → `read` + `.poll` 就够；
   - 多 fd + 可扩展事件循环 → 实现 `.poll`，应用用 epoll；
   - 少数“高重要性设备”希望被“主动通知” → 同时实现 fasync；
   - 不建议仅用 fasync 而不实现 `.poll`，这会限制上层组合能力。

在后续 **12.2–12.5** 中，我们会继续把 fasync 放到更大的生态里：

- 对比 input 子系统事件模型（evdev）与 netlink / uevent / sysfs 轮询；
- 讨论 fasync 与 `io_uring`/AIO 的关系；
- 最终给出一张“通知机制决策矩阵”，方便你在工程上做系统性选型。



------

### 12.1.2\_fasync\_vs\_input\_子系统事件模型

#### (1)\_引入\_同样是\_键盘/按键事件\_为什么有两套方案

在 SoC/嵌入式系统里，最典型的“异步事件”就是按键、触摸、编码器等输入设备。
 对于同一个 GPIO 按键，你理论上有两条完全不同的实现路径：

1. 写一个“裸”字符设备驱动：
   - 自己维护 `event_count` / `wait_queue_head_t` / `.fasync`；
   - 用户态通过 `read` + `poll` + SIGIO 直接读整数事件。
2. 把它接入 **input 子系统**：
   - 驱动注册 `struct input_dev`，在中断中调用 `input_report_key()` 等；
   - 由通用 `evdev` 驱动导出 `/dev/input/eventX`；
   - 用户态通过 evdev 协议得到 `struct input_event` 流，依赖 `.poll` + 可选 fasync。

这两条路径都能完成“按键产生 → 用户收到事件”的功能，但它们在：

- 数据建模（“一个事件是什么”）；
- 状态语义（按下/松开/重复、坐标系、同步框架）；
- API 抽象程度（原始 vs 规范化）；

上差异很大。

**本节目标：**

- 先从数据结构层面，把 **fasync 字符设备** 和 **input/evdev** 模型的差异拆出来；
- 再从驱动/用户两个视角，说明何时应选用 input 子系统而不是裸 fasync；
- 最后给出一个“同一块硬件按键”用两种方式写驱动的对照示例，并列出决策要点。

------

#### (2)\_数据结构视角\_event\_count\_vs\_input\_event\_帧语义

##### 1)\_fasync\_字符设备的典型状态模型

在前几章，我们给的“最小 async 驱动”通常是这样建模的：

- 驱动内部：

  ```c
  struct demo_async_dev {
  	wait_queue_head_t	wait;
  	spinlock_t		lock;
  	struct fasync_struct	*async_queue;
  	unsigned int		event_count;  /* 有多少次事件待处理 */
  };
  ```

- 事件处理时：

  - 中断或工作队列中 `event_count++`；
  - `wake_up_interruptible(&dev->wait)`；
  - 若有 `async_queue` 则 `kill_fasync()`。

- 用户态读到的是某种“计数”或“简单结构”：

  ```c
  struct demo_event {
  	__u32	count_since_last_read;
  };
  ```

特点：

- **事件的类型/语义非常自由**：只要双方约定好结构即可；
- 但没有统一框架来表达：
  - “按下/松开”的状态机；
  - 相对/绝对轴（鼠标 X/Y、触摸坐标）；
  - 多个子设备/多路复用等。

##### 2)\_input\_子系统的事件模型\_struct\_input\_event

input 子系统（尤其是 evdev）把输入设备统一建模为：

- 事件类型（`EV_KEY`、`EV_REL`、`EV_ABS`、`EV_SW`、`EV_MSC`……）；
- 事件 code（`KEY_A`, `BTN_LEFT`, `ABS_X` 等）；
- 事件 value（按键：0/1/2；坐标：数值；相对位移：增量）。

核心数据结构就是：

```c
struct input_event {
	struct timeval time;
	__u16 type;   /* EV_KEY / EV_REL / EV_ABS / EV_SYN / ... */
	__u16 code;   /* KEY_* / BTN_* / ABS_* / ... */
	__s32 value;  /* 具体值 */
};
```

evdev 输出的是 **事件帧流**：

- 驱动调用 `input_report_key(dev, KEY_X, 1)` 等函数；
- 内核将这些转换为一系列 `input_event`，并在恰当时机插入 `EV_SYN`（同步事件）；
- 用户态 read `/dev/input/eventX` 得到一个个 `struct input_event`，具有统一语义：
  - 按键按下：`EV_KEY, KEY_F1, 1`；
  - 按键松开：`EV_KEY, KEY_F1, 0`；
  - 完成一帧：`EV_SYN, SYN_REPORT, 0`；
  - 丢帧/缓冲溢出：`EV_SYN, SYN_DROPPED, 1` 等。

##### 3)\_通知机制层面的共性与差异

从“通知机制”角度，input/evdev 也是：

- 内部维护一个事件环形队列；
- `.read` 从队列取 `input_event`；
- `.poll` 和 `.fasync` 都以“队列非空”为条件通知用户态。

也就是说，在**纯通知层面**，fasync vs input 的差异不大：

- 都可以支持 `poll/epoll`；
- 都可以支持 SIGIO/fasync。

真正的差异在于：

- **上层事件语义的规范化程度**（input 有统一的 event type/code/value 约定）；
- **事件状态机是否由通用子系统维护**（input 统一处理按键重复、组合键、映射等）。

------

#### (3)\_开发者视角\_写一个\_裸\_fasync\_驱动\_vs\_接入\_input\_的差异

从内核驱动作者角度，对同一块按键硬件，两种路径的开发行为可以这样对比。

##### 1)\_裸\_fasync\_字符驱动\_完全自定义

你需要自己负责：

1. **硬件接入层**
   - GPIO/中断的 request/配置；
   - debouncing（去抖）、防抖时间常量等。
2. **事件状态建模**
   - 是按 0/1 表示按下/松开，还是用计数？
   - 是否区分“短按/长按/连击”等高级语义？
   - 是否需要按键重复（repeat）机制？
3. **通知机制**
   - 实现 `.read`：约定用户态读到什么结构；
   - 实现 `.poll`：利用 waitqueue 表示“队列非空”；
   - 实现 `.fasync`：维护 `async_queue`，在事件发生时 `kill_fasync()`。

优点：

- 设计空间完全开放；
- 学习成本和依赖最小（只依赖 VFS/字符设备框架）。

缺点：

- 你要自己定义协议（数据格式 + 语义）；
- 很容易出现“某个项目特定的临时协议”，后续很难被其他程序通用使用；
- 功能一多就会开始“重复造 input 子系统的轮子”（例如处理组合键、重复键）。

##### 2)\_接入\_input\_子系统\_重用通用事件框架

采用 input 模型时，驱动侧通常做的事情是：

1. **分配并注册 `struct input_dev`**

   - 使用 `devm_input_allocate_device()` 和 `input_register_device()`；

   - 在 `input_dev` 上描述能力：

     ```c
     input_dev->name = "demo-key";
     __set_bit(EV_KEY, input_dev->evbit);
     __set_bit(KEY_ENTER, input_dev->keybit);
     ```

2. **在中断处理函数中调用 `input_report_key()` + `input_sync()`**

   - 不需要自己指定时间戳/帧结束标志，input 核心会代劳；
   - evdev 驱动会把事件放到统一缓冲队列里。

3. **不关心 `.read` / `.poll` / `.fasync` 的实现细节**

   - 这些由 evdev 统一实现；
   - 通知机制（waitqueue + fasync）也由通用代码承担。

优点：

- 事件语义统一，可被现有工具直接使用（`evtest`、X.org/Wayland、libinput 等）；
- 多路输入源（键盘、鼠标、触摸屏等）可以统一被上层库处理；
- 驱动只需要专注于“从硬件转换为 input_event”。

缺点：

- 需要理解 input 子系统和 evdev 的概念；
- 适用范围偏“输入设备”（按键/指针/触摸等），对某些非人机输入场景可能不适合。

------

#### (4)\_用户/平台视角\_evdev\_的统一接口\_vs\_自定义协议

从用户态开发者的角度，差异更加明显。

##### 1)\_使用\_evdev\_的典型模式

用户只需要：

- 打开 `/dev/input/eventX`；
- 使用 `read()` 得到 `struct input_event` 数组；
- 或使用 `poll/epoll` 等等待可读事件。

特点：

- 事件结构是标准的 `input_event`，不需要额外文档说明；
- 系统提供的库（`libevdev`、`libinput` 等）可以直接解析、处理映射；
- 系统桌面/窗口系统可以通过 evdev 接入这些设备。

##### 2)\_使用自定义\_fasync\_字符设备的典型模式

用户必须：

- 熟悉特定项目定义的结构体（例如 `struct demo_event`）；
- 理解计数/状态含义（例如 1=按下，2=长按）等；
- 若希望参与系统输入（如当作键盘使用），还需要再写一层适配。

这种方式更适合：

- 只服务于单一专用应用（私有协议）；
- 或用于调试/实验阶段的快速验证。

因此，从平台/发行版角度，如果你希望一个设备：

- 被桌面环境/通用输入框架识别；
- 能够参与键盘布局映射、组合键处理；

**优先应考虑 input 子系统，而不是裸 fasync 设备**。

------

#### (5)\_可视化\_同一块\_GPIO\_按键的两种\_上行路径

用一个对比图展示“同一块硬件按键”的两种栈结构。

```mermaid
flowchart LR
    "HW"["GPIO/按键硬件"]

    subgraph "路径 A: 裸 fasync 字符设备"
        "DRV_A"["驱动A: demo_async_key\n(字符设备 + fasync)"]
        "CDEV"["/dev/demo_async_key"]
        "APP_A"["用户应用A\n(自定义协议 + SIGIO)"]
    end

    subgraph "路径 B: input/evdev"
        "DRV_B"["驱动B: demo_input_key\n(struct input_dev)"]
        "EVDEV"["通用驱动: evdev\n(/dev/input/eventX)"]
        "APP_B1"["用户应用B1\n(evdev 专用工具/库)"]
        "APP_B2"["系统组件B2\n(libinput/桌面系统)"]
    end

    "HW" --> "DRV_A"
    "DRV_A" --> "CDEV"
    "CDEV" --> "APP_A"

    "HW" --> "DRV_B"
    "DRV_B" --> "EVDEV"
    "EVDEV" --> "APP_B1"
    "EVDEV" --> "APP_B2"
```

阅读要点：

- 路径 A 中，驱动直接向上暴露字符设备，通知机制靠 fasync；
- 路径 B 中，驱动把自己接入 input 框架，由 evdev 向上暴露统一事件接口；
- 这两条路径可以共存，但**在同一个产品中通常只选一条**。

------

#### (6)\_示例代码\_同一\_GPIO\_按键的\_fasync\_版\_vs\_input\_版(缩略)

为了对比清晰，这里给出两个**缩略示例**（省略 probe/资源申请/错误处理的大量细节），重点看“通知模型”。

##### 1)\_示例一\_fasync\_字符设备按键驱动(概要)

```c
/* 示例: demo_key_fasync.c (概要框架) */

struct demo_key_dev {
	wait_queue_head_t	wait;
	spinlock_t		lock;
	struct fasync_struct	*async_queue;

	unsigned int		event_count;
	int			irq;
	int			gpio;
};

static irqreturn_t demo_key_isr(int irq, void *dev_id)
{
	struct demo_key_dev *dev = dev_id;
	unsigned long flags;
	bool notify = false;

	spin_lock_irqsave(&dev->lock, flags);

	dev->event_count++;

	wake_up_interruptible(&dev->wait);

	if (dev->async_queue)
		notify = true;

	spin_unlock_irqrestore(&dev->lock, flags);

	if (notify)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);

	return IRQ_HANDLED;
}

static ssize_t demo_key_read(struct file *filp, char __user *buf,
			     size_t count, loff_t *ppos)
{
	struct demo_key_dev *dev = filp->private_data;
	unsigned long flags;
	unsigned int events = 0;

	if (count < sizeof(events))
		return -EINVAL;

	if (wait_event_interruptible(dev->wait, dev->event_count > 0U))
		return -ERESTARTSYS;

	spin_lock_irqsave(&dev->lock, flags);
	events = dev->event_count;
	dev->event_count = 0U;
	spin_unlock_irqrestore(&dev->lock, flags);

	if (copy_to_user(buf, &events, sizeof(events)))
		return -EFAULT;

	return sizeof(events);
}

static __poll_t demo_key_poll(struct file *filp, poll_table *wait)
{
	struct demo_key_dev *dev = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &dev->wait, wait);

	if (dev->event_count > 0U)
		mask |= EPOLLIN | EPOLLRDNORM;

	return mask;
}

static int demo_key_fasync(int fd, struct file *filp, int on)
{
	struct demo_key_dev *dev = filp->private_data;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&dev->lock, flags);
	ret = fasync_helper(fd, filp, on, &dev->async_queue);
	spin_unlock_irqrestore(&dev->lock, flags);

	return ret;
}
```

用户态读到的只是 “某个时间段内按键触发次数”。

##### 2)\_示例二\_input/evdev\_按键驱动(概要)

```c
/* 示例: demo_key_input.c (概要框架) */

#include <linux/input.h>

struct demo_key_input_dev {
	struct input_dev	*idev;
	int			irq;
	int			gpio;
};

static irqreturn_t demo_key_input_isr(int irq, void *dev_id)
{
	struct demo_key_input_dev *dk = dev_id;
	int value;

	/* 从 GPIO 读当前电平, 0 = 释放, 1 = 按下 */
	value = gpio_get_value(dk->gpio);

	/* 报告按键状态 */
	input_report_key(dk->idev, KEY_ENTER, value);

	/* 一帧同步 */
	input_sync(dk->idev);

	return IRQ_HANDLED;
}

static int demo_key_input_probe(struct platform_device *pdev)
{
	struct demo_key_input_dev *dk;
	struct input_dev *idev;
	int error;

	dk = devm_kzalloc(&pdev->dev, sizeof(*dk), GFP_KERNEL);
	if (!dk)
		return -ENOMEM;

	idev = devm_input_allocate_device(&pdev->dev);
	if (!idev)
		return -ENOMEM;

	dk->idev = idev;

	idev->name = "demo-input-key";
	idev->phys = "demo/input0";

	__set_bit(EV_KEY, idev->evbit);
	__set_bit(KEY_ENTER, idev->keybit);

	error = input_register_device(idev);
	if (error)
		return error;

	/* 此后由 evdev 导出 /dev/input/eventX, .poll/.fasync 由通用层实现 */

	return 0;
}
```

特点：

- 驱动只关心“按键电平 → `input_report_key`”；
- `.read` / `.poll` / `.fasync` 全部由 `drivers/input/evdev.c` 实现；
- 用户态读取的是标准 `struct input_event` 序列。

------

#### (7)\_调试与验证\_fasync\_字符驱动\_vs\_input\_驱动的排错差异

调试时的工具链也略有不同：

1. **对于 fasync 字符驱动：**

   - `/dev/demo_async_key` 上用：
     - `hexdump` / 自定义小工具读结构；
     - `strace` 检查 fcntl/FASYNC；
     - `/proc/PID/fdinfo` 排查 O_ASYNC，`/proc/PID/status` 排查 SIGIO。

2. **对于 input/evdev 驱动：**

   - 使用标准工具：

     ```sh
     evtest /dev/input/eventX
     ```

   - 可以查看：

     - 按键类型、code、value 的实时变化；
     - `EV_SYN` 帧；
     - 如果发生缓冲溢出，会看到 `SYN_DROPPED`。

3. **从内核侧 trace**

   - fasync 字符驱动：trace `.fasync` / `kill_fasync` / 中断；
   - input 驱动：trace `input_report_key` / `evdev_events` 等。

4. **一致性验证**

   - 使用同一块硬件，分别加载两种驱动，实现对比测试：
     - 按同样的按键序列，观察用户态看到的数据是否值/语义一致；
     - 对比在高频按键或长按场景下的行为。

------

#### (8)\_小结\_input\_子系统是\_规范化输入设备\_的首选\_fasync\_更适合通用自定义

本小节的关键结论可以整理为：

1. **在“通知机制”这一层，fasync 与 input/evdev 没有本质差异**
   - input/evdev 内部同样使用 waitqueue 和可选 fasync；
   - 用户态也可以通过 `poll/epoll` 或 SIGIO 获取事件；
   - 差异不在“怎么通知”，而在“通知什么”。
2. **在“事件模型”这一层，差异非常明显**
   - 裸 fasync 字符设备：
     - 事件结构和语义完全自定义，灵活但不通用；
     - 适合纯自研应用、协议完全由你掌控的场景。
   - input 子系统：
     - 使用标准化的 `input_event` 模型；
     - 有丰富的 EV_* / KEY_* / ABS_* / SYN_* 语义；
     - 能被现有生态直接消费（桌面系统、libinput 等）。
3. **从驱动作者角度的建议**
   - 如果硬件逻辑就是“人机输入”（键盘、按钮、指点设备、旋钮、遥控等），
      **优先选择 input 子系统**，再考虑是否需要额外的裸 fasync 设备；
   - 只有在以下情况，才倾向于直接写 fasync 字符驱动：
     - 事件语义高度特化，不能自然映射到任何 EV_* 类型；
     - 不打算接入系统通用输入框架，而只是给专用服务使用；
     - 或者你在做教学/实验性质示例，重点在 fasync 本身。
4. **从用户/平台角度的建议**
   - 用户态若希望使用系统通用库/工具处理输入，应绑定 input/evdev；
   - 用户态若只想做简单测试或使用专有协议，可以直接对接 fasync 字符设备；
   - 在统一事件循环（epoll）中，两者都能与 socket/pipe 等资源共存。



------

### 12.1.3\_fasync\_vs\_netlink/kobject\_uevent/sysfs\_轮询

#### (1)\_引入\_设备\_数据事件\_vs\_配置/拓扑变化事件

在内核里，“通知机制”并不只服务于“设备产生了新数据”。
 还有一大类通知，是在描述：

- 设备/子系统的 **存在与拓扑变化**（插拔、新挂载、接口 up/down）；
- 某些 **配置状态的变化**（模式切换、电源状态改变、策略调整）；
- 内核内部的 **控制信息广播**（路由更新、链路事件等）。

这类信息，通常不会通过 fasync/SIGIO 来传递，而是使用：

- **netlink**：socket 风格的内核↔用户控制消息通道（`NETLINK_ROUTE`、`NETLINK_KOBJECT_UEVENT` 等）；
- **kobject_uevent**：基于 kobject 的热插拔/状态变化事件，通常由 `udevd` 等守护进程接收；
- **sysfs 轮询/触发**：通过 sysfs 文件承载状态/控制节点，用户态通过读/写/`poll` 轮询。

本小节的核心是把这类机制和 fasync 作一个**功能维度上的分工**：

> **fasync 字符设备 = “某个 fd 上的数据准备就绪通知”**
>  **netlink / uevent / sysfs = “系统级控制/拓扑/状态变化通知”**

我们从数据结构、开发者视角、用户/平台视角分别展开，然后给出典型示例与调试建议。

------

#### (2)\_数据结构视角\_per-fd\_事件\_vs\_系统级广播

##### 1)\_fasync\_挂在具体\_struct\_file\_上的通知链表

前面章节已经详细解析过：

- 每个打开的文件（`struct file`）可以通过 `.fasync` 链接到一个 `struct fasync_struct` 链表；
- `kill_fasync()` 按事件类型遍历链表，对相关 `fasync_struct` 对应的 `file` 所属进程发送 SIGIO；
- 这是一个典型的“**面向单个设备 fd 的通知链**”。

性质：

- 通知目标：**特定进程/进程组**（通过 F_SETOWN/F_SETOWN_EX）；
- 通知语义：**“这个 fd 有事件，需要你去读/写”**；
- 通知来源：具体驱动（字符设备/某些文件系统实现）。

##### 2)\_netlink\_基于\_struct\_sock\_的多播/单播消息通道

netlink 本质上是：

- 一类特殊的 socket 协议族：`PF_NETLINK`；
- 内核端维护 `struct sock` / `struct netlink_sock`；
- 用户态通过 `socket(AF_NETLINK, SOCK_RAW, NETLINK_xxx)` 打开；
- 通信模型支持：
  - 单播（内核↔特定 PID）；
  - 多播（一个消息发给多个订阅组）。

典型用途：

- `NETLINK_ROUTE`：路由/链路事件（接口 up/down、IP 地址变化等）；
- `NETLINK_KOBJECT_UEVENT`：kobject 层的 uevent（热插拔、属性变化）。

数据结构上，netlink 关注的是：

- **“消息”**（`struct nlmsghdr` + 自定义 payload）；
- **“多播组/订阅者集合”**；

而不是某个具体字符设备的 `struct file`。

##### 3)\_kobject\_uevent\_围绕\_struct\_kobject\_的属性变化广播

kobject 是内核对象模型的基础：

- 每个暴露到 sysfs 的对象，背后都有一个 `struct kobject`；
- `kobject_uevent()` 将某个事件（`KOBJ_ADD/REMOVE/CHANGE` 等）转换为 uevent 字符串（`ACTION=add` 等），通过 `NETLINK_KOBJECT_UEVENT` 发送给用户态；
- 通常由 `udevd` 监听并执行规则（创建节点、加载模块、运行脚本）。

本质上：

- **不关心某个 fd 是否可读/可写**；
- 关注的是“某个设备/对象在系统拓扑/属性层面的变化”。

##### 4)\_sysfs\_轮询\_基于属性文件的\_慢速状态感知

sysfs 文件背后是 `struct attribute` / `struct kobj_attribute`：

- `show()` 回调负责将内核状态以文本形式导出；
- `store()` 回调接受用户写入的配置；
- 某些 sysfs 文件还实现了 `.poll`，允许用户进程对“状态变化”使用 `poll/epoll` 进行轮询。

这里的重点是：

- sysfs 多用于**低频配置/状态**，而不是高频事件流；
- 如果把高频事件通过 sysfs 暴露再配合轮询，会产生严重性能问题。

------

#### (3)\_开发者视角\_哪些事用\_fasync\_哪些事用\_netlink/uevent/sysfs

从驱动/子系统作者的决策角度，可以将需求分成两类：

1. **数据面（data-plane）事件**
   - 某个设备 fd 上的 **数据缓存/队列** 发生了变化（有新数据、可发送空间等）；
   - 典型例子：
     - 串口收到数据；
     - 采集卡缓冲区填满一块；
     - GPIO 按键按下/松开（作为“事件流”看待）。
   - 对应机制：
     - `.read` / `.write` / `.poll` / `.fasync`；
     - 用户态通常有一个持续运行的事件循环。
2. **控制面（control-plane）事件**
   - 设备/对象的 **存在、拓扑或配置状态** 发生变化：
     - 设备插入/拔出；
     - 电源状态变化（上电/断电）；
     - 模式切换（正常/降速/只读等）；
     - 链路 up/down、路由新增/删除等。
   - 对应机制：
     - netlink（面向网络栈/路由/子系统控制）；
     - kobject_uevent（热插拔/属性变化，配合 udev 规则）；
     - sysfs 中的属性文件 + 轮询/写入。

**经验规则：**

- **“需要被某一个持有 fd 的进程尽快消费的数据” → fasync + poll**；
- **“需要广播给系统中所有关心该事件的守护进程/管理进程，用于做策略/拓扑更新” → netlink/uevent/sysfs**。

例子：

- 你的 NFC 读卡器产生“某卡号 + 时间戳”的读卡事件 → 更适合做成字符设备 + fasync；
- 你的 PCIe 控制器检测到一个新的硬件模块插入 → 更适合通过 `kobject_uevent(KOBJ_ADD)` 广播给 `udevd`，由其加载对应驱动/创建设备节点。

------

#### (4)\_用户/平台视角\_服务型进程\_vs\_系统守护进程

从用户态视角看，这几类通知机制面向的是不同角色的程序。

##### 1)\_fasync/poll\_面向\_业务应用/服务进程

特点：

- 进程用 `open("/dev/xxx")` 获得一个 fd，生命周期与业务紧密绑定；
- 业务进程负责消费具体数据（按键事件、采样数据、报文）并作逻辑处理；
- 通知对象是“这个 fd 的 owner”，语义是“你有数据要处理”。

典型角色：

- 输入事件处理服务；
- 多媒体/采集处理服务；
- 各类自定义设备的用户态驱动/守护进程。

##### 2)\_netlink/uevent\_面向\_系统管理/策略守护进程

特点：

- 守护进程不一定直接持有某个设备 fd；
- 关心的是系统整体状态：
  - 网卡 up/down → 调整路由/防火墙；
  - 块设备插入 → 自动挂载、做文件系统检查；
  - 电池电量/电源模式切换 → 调整电源策略。
- 通常只有少数系统级进程订阅这些事件（如 `udevd`、`NetworkManager`、`systemd-logind` 等）。

操作方式：

- netlink：监听特定协议族与多播组；
- uevent：通过 `/sys/class/net/...` 等配合 `udevd` 规则；
- sysfs：定期读/尝试 `poll` 某属性文件。

------

#### (5)\_可视化\_数据面\_vs\_控制面通知路径对比

下面用一个示意图，把“同一块物理网络设备”的两类通知路径画出来。

```mermaid
flowchart LR
    "HW"["HW: 网卡硬件"]

    subgraph "数据面: 数据收发通知"
        "DRV_DATA"["DRV_DATA: 驱动数据路径\n(NAPI / 驱动 RX/TX)"]
        "CHAR"["/dev/my_nic_data\n(假设有一个字符接口)"]
        "APP_DATA"["APP_DATA: 用户态数据消费者\n(业务进程)"]
    end

    subgraph "控制面: 链路/状态变化通知"
        "DRV_CTRL"["DRV_CTRL: 驱动控制路径\n(link up/down, carrier)"]
        "NETLINK"["NETLINK_ROUTE\n(内核 netlink socket)"]
        "UEVENT"["kobject_uevent\n(设备 ADD/REMOVE/CHANGE)"]
        "SYSFS"["/sys/class/net/eth0/*\n(状态/属性文件)"]
        "APP_CTRL1"["APP_CTRL1: NetworkManager"]
        "APP_CTRL2"["APP_CTRL2: udevd/systemd-udevd"]
    end

    "HW" --> "DRV_DATA"
    "DRV_DATA" --> "CHAR"
    "CHAR" --> "APP_DATA"

    "HW" --> "DRV_CTRL"
    "DRV_CTRL" --> "NETLINK"
    "DRV_CTRL" --> "UEVENT"
    "DRV_CTRL" --> "SYSFS"

    "NETLINK" --> "APP_CTRL1"
    "UEVENT" --> "APP_CTRL2"
    "SYSFS" --> "APP_CTRL1"
```

要点：

- 同一个硬件既可以产生“数据面事件”（报文收发），也会产生“控制面事件”（链路状态变化）；
- 数据面：更适合通过 fd + poll/fasync 被具体业务进程监听；
- 控制面：更适合通过 netlink/uevent/sysfs 被系统级守护进程处理。

------

#### (6)\_示例代码\_简单\_netlink/uevent/sysfs\_片段与\_fasync\_对照

本节只给“框架级”示例，避免展开完整子系统（那会非常长）。重点是感受“接口形态”。

##### 1)\_netlink\_内核端简单示例(自定义\_family)

> 注意：这里只给出极简版本，真实项目中建议复用现有子系统的 netlink 接口（如 `rtnetlink`）。

```c
/* 示例: demo_nl.c (极简框架) */

#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <net/sock.h>

#define DEMO_NETLINK_FAMILY      31
#define DEMO_NETLINK_GROUP       1

static struct sock *demo_nl_sock;

static void demo_nl_send_event(const char *msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	size_t len = strlen(msg) + 1;

	skb = nlmsg_new(len, GFP_KERNEL);
	if (!skb)
		return;

	nlh = nlmsg_put(skb, 0, 0, 0, len, 0);
	if (!nlh) {
		kfree_skb(skb);
		return;
	}

	memcpy(nlmsg_data(nlh), msg, len);

	/* 向多播组发送, 用户可以通过 netlink socket 订阅 DEMO_NETLINK_GROUP */
	netlink_broadcast(demo_nl_sock, skb, 0, DEMO_NETLINK_GROUP, GFP_KERNEL);
}

static int __init demo_nl_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.groups	= 1,
	};

	demo_nl_sock = netlink_kernel_create(&init_net,
					     DEMO_NETLINK_FAMILY,
					     &cfg);
	if (!demo_nl_sock)
		return -ENOMEM;

	/* 之后在某个状态变化处调用 demo_nl_send_event("state=xxx") */
	return 0;
}

static void __exit demo_nl_exit(void)
{
	if (demo_nl_sock)
		netlink_kernel_release(demo_nl_sock);
}

module_init(demo_nl_init);
module_exit(demo_nl_exit);
```

与 fasync 对比：

- fasync 面向的是“谁打开了 `/dev/demo_xxx`”；
- netlink 面向的是“谁订阅了 DEMO_NETLINK_FAMILY/DEMO_NETLINK_GROUP”；
- 两者可共存：数据走字符设备/fasync，状态变化走 netlink。

##### 2)\_kobject\_uevent\_简单触发示例(属性改变时通知)

```c
/* 示例: demo_uevent.c (片段) */

#include <linux/kobject.h>
#include <linux/kdev_t.h>

static struct kobject *demo_kobj;

static ssize_t demo_attr_store(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	/* 省略解析 buf, 更新状态的逻辑 */

	/* 状态变化后, 向用户空间发送 CHANGE 事件 */
	kfree_const(attr->attr.name);
	kobject_uevent(kobj, KOBJ_CHANGE);

	return count;
}

static struct kobj_attribute demo_attr =
	__ATTR(demo_mode, 0664, NULL, demo_attr_store);

static int __init demo_uevent_init(void)
{
	int ret;

	demo_kobj = kobject_create_and_add("demo_uevent", kernel_kobj);
	if (!demo_kobj)
		return -ENOMEM;

	ret = sysfs_create_file(demo_kobj, &demo_attr.attr);
	if (ret)
		kobject_put(demo_kobj);

	return ret;
}

static void __exit demo_uevent_exit(void)
{
	if (demo_kobj) {
		sysfs_remove_file(demo_kobj, &demo_attr.attr);
		kobject_put(demo_kobj);
	}
}

module_init(demo_uevent_init);
module_exit(demo_uevent_exit);
```

用户态可以通过 `udevd` 规则，匹配 `KERNEL=="demo_uevent", ACTION=="change"` 触发脚本。这是**系统级配置变更通知**，与 fasync 的“数据可读”语义截然不同。

##### 3)\_sysfs\_+\_poll\_示意(属性轮询)

部分 sysfs 属性可以实现 `.poll`，用户态可以：

```c
/* 示例: demo_sysfs_poll.c (用户态片段) */

int main(void)
{
	const char *path = "/sys/devices/virtual/demo_uevent/demo_mode";
	int fd = open(path, O_RDONLY);
	struct pollfd pfd;
	char buf[64];
	int ret;

	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	pfd.fd = fd;
	pfd.events = POLLPRI;	/* 某些驱动会在状态变化时触发 POLLPRI */

	for (;;) {
		ret = poll(&pfd, 1, -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("poll");
			break;
		}

		if (pfd.revents & POLLPRI) {
			lseek(fd, 0, SEEK_SET);
			memset(buf, 0, sizeof(buf));
			read(fd, buf, sizeof(buf) - 1);
			printf("demo_mode changed: %s", buf);
		}
	}

	close(fd);
	return EXIT_SUCCESS;
}
```

这种轮询适合低频状态变化，不适合高频数据事件。

------

#### (7)\_调试与验证\_混用\_fasync\_与系统级通知时的边界检查

当一个子系统同时使用 fasync 和 netlink/uevent/sysfs 时，调试需要注意：

1. **语义不要重复**
   - 同一个“按键按下事件”，不应既通过字符设备 fasync 通知，又通过 netlink 广播给系统守护进程，否则会出现重复处理；
   - 一般做法是：
     - “人机输入” → 走 input/evdev；
     - “设备拓扑/模式变化” → 走 uevent/netlink。
2. **通知频率控制**
   - netlink/uevent 通常是“低频控制通知”，不适合高频（每秒数千次）事件；
   - fasync 也要遵守前面 11.5/11.7 讲的限流/聚合策略；
   - sysfs + poll 更不适合高频使用。
3. **工具链区分**
   - 检查 fasync 行为：用 `strace` + `/proc` + `ftrace`；
   - 检查 netlink/uevent 行为：
     - `udevadm monitor`；
     - `tcpdump -i any netlink`（需要配置）；
     - 直接写小程序使用 `AF_NETLINK` 监听相关 family/group；
   - 检查 sysfs：`cat` / `inotifywait` / `poll` 小工具。
4. **边界条件验证**
   - 拔插设备、切换模式、网络抖动等情况下，观察：
     - 是否重复通知；
     - 是否存在遗漏事件；
     - 是否会导致守护进程/业务进程异常负载。

------

#### (8)\_小结\_fasync\_是\_设备事件级\_机制\_netlink/uevent/sysfs\_是\_系统级\_机制

本小节把 fasync 与 netlink/kobject_uevent/sysfs 轮询放在一张图上对比，结论可以整理为：

1. **关注对象不同**
   - fasync：
     - 关注 **某个 fd 的数据状态**（可读/可写）；
     - 对象是特定打开该 fd 的进程；
     - 通知粒度通常是“这段时间内，该设备有数据，请处理”。
   - netlink/uevent/sysfs：
     - 关注 **系统/子系统层面的拓扑、状态、配置变化**；
     - 对象是一组订阅者或系统守护进程；
     - 通知粒度是“系统状态发生一次变化”。
2. **适用场景不同**
   - 数据面：设备数据流、测量数据、事件流 → 适合字符设备 + `.poll` + fasync；
   - 控制面：设备插拔、模式改变、路由更新、电源策略变化 → 适合 netlink/uevent/sysfs。
3. **组合方式**
   - 同一个设备/子系统通常会**同时**使用两类机制：
     - 用字符设备/子系统专用接口传递数据事件；
     - 用 uevent/netlink/sysfs 通告控制/拓扑变化。
   - 设计关键是：划清“哪些是数据事件，哪些是控制事件”。
4. **实践建议**
   - 如果你正在写“高频数据流”的驱动（传感器、采集卡、串口等），
      → 优先考虑 `.read` / `.poll` / fasync，让业务进程通过 fd 管理事件；
   - 如果你要暴露“这块设备插上/拔下/模式改变”，
      → 优先考虑 `kobject_uevent`、netlink 或 sysfs 属性，而不是用 fasync 做广播。



------

### 12.1.4\_与\_io\_uring\_/\_AIO\_等新型异步\_I/O\_机制的关系

#### (1)\_引入\_fasync\_是\_老接口\_io\_uring\_是\_新框架\_它们怎么共存

在 Linux 生态里，如果按时间线来看异步 I/O 机制，大致可以分三代：

1. **信号驱动 I/O / fasync**
   - 早期以 `SIGIO`、`SIGPOLL` 等为主，驱动通过 fasync + `kill_fasync()` 触发；
   - 适配的是“少量 fd + 事件较稀疏 + 业务逻辑偏事件驱动”的场景。
2. **POSIX AIO（`aio_\*` 系列）**
   - 提供异步 `read`/`write` 语义，但实现复杂、语义不统一；
   - 很多实现实际上在用户态借助线程池模拟，内核原生支持也有限。
3. **io_uring（近几年内核重点方向）**
   - 用共享 ring buffer（SQ/CQ）+ 批量提交/回收的机制，减少系统调用开销；
   - 在一个统一框架下支持多种操作（读写、超时、poll、接受连接等）。

而 fasync 处于第一代：它解决的是“驱动如何主动告诉用户进程有事件”，本质仍然是 **以文件描述符为中心的事件通知**。
 io_uring 则试图给出一个覆盖“文件 I/O + poll + 部分驱动特定操作”的统一异步框架。

本节要回答的问题是：

- fasync 和 io_uring/AIO 的 **分工与重叠** 在哪里？
- 对驱动作者来说，要不要“特意支持 io_uring”？需要改哪些接口？
- 从用户态看，是“改用 io_uring 替代 fasync”，还是“用 io_uring 封装 poll，再配合现有驱动接口”？

------

#### (2)\_数据结构视角\_fasync\_链表\_vs\_AIO/io\_uring\_上下文

先用结构对比的方式，把三者的核心抽象列出来（只取必要部分）。

##### 1)\_fasync\_fasync\_struct\_+\_进程信号队列

- 驱动所见：

  ```c
  struct fasync_struct {
  	int			magic;
  	int			fa_fd;
  	struct fasync_struct	*fa_next;
  	struct file		*fa_file;
  	struct rcu_head	fa_rcu;
  };
  ```

- 内核通过 `fasync_helper()` 把 `struct file` 链接到 `fasync_struct` 链表；

- 设备事件发生时，驱动调用 `kill_fasync()`，内核遍历链表，对目标进程/进程组设置 pending 信号（`SIGIO` 等）；

- **通知载体是信号队列**，数据本身仍通过 `.read` / `.ioctl` 等接口获取。

##### 2)\_传统\_AIO\_aio\_context\_+\_kiocb\_/\_iocb

POSIX AIO 在 Linux 下的内核内部结构（简化视角）：

- 每个 AIO 上下文（`aio_context_t`）对应一个 `struct kioctx`；
- 每个 AIO 请求对应 `struct kiocb` / `struct iocb`；
- 内核在后台完成 I/O 后，将结果放入完成队列，用户通过 `io_getevents()` 等接口获取。

特点：

- 对常规文件/块设备支持较多，和直接 `read`/`write` 接口有关；
- 对一般字符设备/自定义驱动，是否支持 AIO 取决于驱动是否实现了 `->aio_read` / `->aio_write` 等钩子（现代内核里这些接口已经逐渐淡出，更多依赖 `->read_iter`/`->write_iter` 以及上层框架）。

总体而言，**POSIX AIO 在社区热度已经明显下降**，更多场景被 io_uring 替代。

##### 3)\_io\_uring\_SQ/CQ\_环\_+\_io\_uring\_ctx

io_uring 的核心抽象是“共享 ring + 批量提交/回收”：

- **用户态与内核共享两个环形队列：**
  - SQ（Submission Queue）：提交请求；
  - CQ（Completion Queue）：完成事件。
- 内核端有 `struct io_ring_ctx`，维护：
  - SQ/CQ 元数据（head/tail 索引等）；
  - 注册的 fd、固定缓冲区等资源；
  - 请求相关的 internal state。

请求类型可以是：

- “发起 I/O”：如 `IORING_OP_READ` / `IORING_OP_WRITE` / `IORING_OP_SEND` / `IORING_OP_RECV`；
- “等待事件”：如 `IORING_OP_POLL_ADD`，本质上相当于“异步 poll”；
- “专用命令”：`IORING_OP_URING_CMD`，供特定驱动提供自定义命令接口（如 NVMe 的 `io_uring_cmd`）。

从数据结构角度看：

- fasync：**围绕 `struct file` 构建“信号链表”**；
- AIO：**围绕 `aio_context` 构建“请求队列 + 完成队列”**；
- io_uring：**围绕 io_uring ctx 构建“通用操作队列 + 完成队列”**，并高度可扩展。

------

#### (3)\_开发者视角\_驱动要不要\_特意支持\_io\_uring/AIO

对绝大多数自定义字符设备驱动而言，可以把问题拆成两层：

1. **基础层：实现 `.read/.write/.poll`，保证在同步 I/O + poll/epoll 下行为正确。**
2. **进阶层：是否增加 io_uring 专用的 `->uring_cmd`、`->read_iter` 等接口以获得更高性能。**

##### 1)\_最通用的做法\_只实现.read/.write/.poll\_让\_io\_uring\_视你为\_普通\_fd

io_uring 支持一个操作类型：`IORING_OP_POLL_ADD`，其语义是：

- 对一个 fd 添加 poll 监视条件（如 `EPOLLIN`）；
- 当 fd 变得可读/可写时，在 CQ 中生成一次完成事件。

这意味着：

- 只要驱动正确实现 `.poll`（参见 6/7 章）并维护好 `wait_queue_head_t` 和状态，
- 用户态就可以在 io_uring 中使用 `IORING_OP_POLL_ADD` 监视这个字符设备，
- 而 **驱动不需要感知“io_uring 的存在”**——内核会把 `->poll` 调用集成进 io_uring 的内部逻辑。

简化结论：

> “想不想支持 io_uring”这件事，对绝大多数驱动作者而言，首先等价于：
>  **“你的 `.poll` 实现是否正确、健壮、可扩展”。**
>  只要 `.poll` 做好了，io_uring 就可以通过 `POLL_ADD` + `READ/WRITE` 操作与你协作。

同样地，POSIX AIO 很多时候也是基于“普通文件操作 + 内核公共层管控”。

##### 2)\_专用优化\_实现\_->uring\_cmd\_或配合块层框架

对某些高性能设备（NVMe、SCM、专用加速卡等），内核已经引入：

- `struct file_operations` 中的 `uring_cmd` 回调；
- 或与块层/网络子系统深度集成的 io_uring path。

这类接口的特点是：

- 驱动需要显式提供 `->uring_cmd`，把 io_uring 的请求直接映射为设备命令；
- 甚至可以绕过传统的 `read/write`/`ioctl` 路径，做更细粒度的异步控制。

对于本书讨论的这类“典型字符设备 + GPIO 中断 + 异步通知”场景：

- 一般 **不需要** 实现 `->uring_cmd` 这类高级接口；
- 只要保证 `.read` / `.poll` / `.fasync` 行为一致，io_uring 就可以通过普通 fd 接口工作；
- 你可以把“专用 io_uring 支持”看做未来设备驱动优化的高级选项，而不是必要条件。

------

#### (4)\_用户/平台视角\_fasync\_的进程模型\_vs\_io\_uring\_的队列模型

从用户态编程模型看，fasync 与 io_uring 差异非常明显。

##### 1)\_fasync\_以进程为中心的\_信号唤醒模型

- 配置过程：
  - `fcntl(F_SETOWN)` 设置 owner；
  - `fcntl(F_SETFL, O_ASYNC)` 打开 fasync；
  - 选择 signal handler 或 signalfd + epoll。
- 优点：
  - 少量 fd、少量事件时，代码简单、直观；
  - 驱动只要 `kill_fasync()`，内核负责信号队列与唤醒。
- 局限：
  - 信号是 per-task 的，处理多线程/多进程共享 fd 时需要谨慎处理所有权；
  - 高并发/高吞吐场景下，频繁信号递交开销较大；
  - 很难利用 batched I/O、固定缓冲区等高级优化。

##### 2)\_io\_uring\_以一次\_io\_uring\_ctx\_为中心的\_操作队列模型

- 使用过程大致是：
  - 创建 io_uring 实例（`io_uring_queue_init`）；
  - 在 SQ 中提交一批操作（READ/WRITE/POLL/ACCEPT 等）；
  - `io_uring_enter()` 通知内核处理；
  - 在 CQ 中批量拉取完成事件。
- 优点：
  - 系统调用次数可大幅减少（多次操作一次提交）；
  - 与线程/信号解耦，适合大量 fd、大量并发请求；
  - 能统一管理各种 I/O（文件、socket、定时器、poll 事件）。
- 局限：
  - 编程模型和简单阻塞 I/O / fasync 相比复杂度明显更高；
  - 对很多简单设备驱动场景来说，io_uring 的收益未必能抵消引入复杂度的成本。

##### 3)\_组合方式\_用\_io\_uring\_作为\_事件循环内核\_fasync\_作为\_某些设备的触发信号源

一种比较自然的组合是：

- 驱动实现 `.poll` + `.fasync`；
- 用户态主事件循环使用 io_uring：
  - 对大部分 I/O 使用 `READ/WRITE` 操作；
  - 对设备通知使用 `IORING_OP_POLL_ADD` 或者 `signalfd + IORING_OP_READ`；
- 对历史原因存在的 fasync 驱动，你可以保留 `SIGIO` 语义，再通过 signalfd 把信号变成 fd，从而纳入 io_uring 事件循环。

------

#### (5)\_可视化\_应用如何在同一个框架里用\_io\_uring\_+\_fasync\_+\_poll

下面用一个示意图说明用户态“统一事件循环”的可能结构：

```mermaid
flowchart LR
    "DEV"["DEV: demo_async_dev\n(.read/.poll/.fasync)"]
    "FD_DEV"["FD_DEV: /dev/demo_async"]
    "FD_SIG"["FD_SIG: signalfd(SIGIO)"]
    "URING"["io_uring ctx\n(SQ/CQ)"]

    "APP"["APP: 用户程序\n(统一事件循环)"]

    "DEV" --> "FD_DEV"

    "FD_DEV" --> "URING"
    "FD_SIG" --> "URING"

    "APP" --> "URING"

    subgraph "内核"
        "URING"
    end

    subgraph "用户态 FD"
        "FD_DEV"
        "FD_SIG"
    end
```

可能的使用方式：

- 对 `/dev/demo_async` 使用：
  - `IORING_OP_POLL_ADD` 监视 `FD_DEV` 的可读事件；
  - 或直接在 io_uring 上发起 `READ` 请求。
- 对 SIGIO（来自 fasync）：
  - 使用 signalfd 把 SIGIO 转成 fd（`FD_SIG`）；
  - 把 `FD_SIG` 也加入 io_uring 的 `POLL_ADD` 或 `READ` 监视；
- 整个应用只维护一个 io_uring 事件循环，内部处理“来自 socket/文件/设备/信号”的所有事件。

------

#### (6)\_示例代码\_使用\_io\_uring\_轮询字符设备\_fd(依赖驱动.poll)

这里给一个简化的用户态示例，演示：

- 内核驱动只要实现 `.poll`，我们就能用 io_uring 的 `POLL_ADD` 来监听设备；
- 和 fasync 本身没有冲突，二者可以共存。

> 说明：示例使用 liburing 风格 API，省略错误处理与完整初始化，仅保留关键逻辑。

```c
/* demo_uring_poll.c: 使用 io_uring 监视 /dev/demo_async */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <liburing.h>

#define DEMO_RING_ENTRIES        8U
#define DEMO_POLL_MASK           POLLIN
#define DEMO_MAX_EVENTS          4U

int main(void)
{
	const char *dev_path = "/dev/demo_async";
	struct io_uring ring;
	int fd;
	int ret;

	fd = open(dev_path, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	ret = io_uring_queue_init(DEMO_RING_ENTRIES, &ring, 0);
	if (ret < 0) {
		fprintf(stderr, "io_uring_queue_init: %s\n", strerror(-ret));
		close(fd);
		return EXIT_FAILURE;
	}

	for (;;) {
		struct io_uring_sqe *sqe;
		struct io_uring_cqe *cqe;
		unsigned int head;
		unsigned int events = 0U;
		ssize_t n;

		/* 1. 提交一个 POLL_ADD 请求 */
		sqe = io_uring_get_sqe(&ring);
		if (!sqe) {
			fprintf(stderr, "get_sqe failed\n");
			break;
		}

		io_uring_prep_poll_add(sqe, fd, DEMO_POLL_MASK);
		io_uring_sqe_set_data(sqe, (void *)(uintptr_t)fd);

		ret = io_uring_submit(&ring);
		if (ret < 0) {
			fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
			break;
		}

		/* 2. 等待完成事件 */
		ret = io_uring_wait_cqe(&ring, &cqe);
		if (ret < 0) {
			fprintf(stderr, "io_uring_wait_cqe: %s\n", strerror(-ret));
			break;
		}

		/* 3. 处理完成事件 (poll 返回, fd 可读) */
		if (cqe->res >= 0) {
			n = read(fd, &events, sizeof(events));
			if (n == (ssize_t)sizeof(events))
				printf("io_uring poll: events=%u\n", events);
		} else {
			fprintf(stderr, "poll cqe error: %d\n", cqe->res);
		}

		/* 4. 回收 CQE */
		io_uring_cq_advance(&ring, 1);

		/* 这里简单循环，每次完成后重新提交一条 POLL_ADD */
	}

	io_uring_queue_exit(&ring);
	close(fd);
	return EXIT_SUCCESS;
}
```

要点：

- 驱动完全不需要知道这是 io_uring；
- 对驱动来说，这只是“某个进程调用 `.poll`，并在状态变化时被唤醒”；
- 用户态通过 io_uring 把 `poll + read` 集成在统一的异步框架中。

------

#### (7)\_调试与验证\_在\_io\_uring\_+\_fasync\_混用场景下的注意点

当你的应用同时使用：

- fasync（SIGIO 或 signalfd）；
- poll/epoll；
- io_uring（POLL_ADD/READ/WRITE）；

需要关注以下问题：

1. **通知路径重复**
   - 同一设备事件同时：
     - 驱动 `wake_up_interruptible` → `.poll` → epoll/io_uring；
     - 驱动 `kill_fasync` → SIGIO → signalfd/io_uring；
   - 可能导致“同一事件被两条途径感知两次”，如果用户态没有正确去重，会导致重复处理。
   - 解决策略：
     - 明确一种“主通知路径”，另一种仅用于调试或兼容；
     - 或在用户态中对同一 fd 来源做统一抽象，确保事件只被消费一次。
2. **fd 生命周期管理**
   - io_uring 中使用的 fd 必须在其整个 I/O 生命周期内保持有效；
   - fasync 也是绑定在 `struct file` 上，`release` 中必须清理 `.fasync` 状态；
   - 调试时注意关闭 fd 的顺序：
     - 先取消 io_uring 上挂起的操作，再 close；
     - 确保 `.release` 中正确调用 fasync 清理 helper。
3. **性能观测**
   - 使用 `strace`/`perf` 观察 syscalls：
     - io_uring 多数场景下 `io_uring_enter` 次数应该小于等于传统模式的 `epoll_wait + read` 次数；
   - 在高频场景中，尽量避免“同时用信号 + io_uring”，否则信号开销可能抵消 io_uring 带来的好处。
4. **行为一致性验证**
   - 不管使用 fasync、epoll 还是 io_uring，驱动端事件状态模型应当统一：
     - “有数据” → `event_count > 0` → `.poll` 返回可读 → fasync 触发 SIGIO；
     - `read` 后清空状态 → `.poll` 变为不可读 → 不再触发 SIGIO；
   - 可以用同一套回归测试，分别在三种通知方式下验证行为一致性。

------

#### (8)\_小结\_在现代异步\_I/O\_体系中\_fasync\_仍然是\_简单\_可组合\_的一环

本小节从数据结构与编程模型的角度，把 fasync 与 AIO / io_uring 放在一起比较，结论可以整理为：

1. **语义层面**
   - fasync：
     - “某个 fd 有事件可处理，通过 SIGIO 提醒你”；
     - 通知载体是“信号”，数据载体仍是 `read/write/ioctl`。
   - AIO：
     - 早期面向“异步 read/write”的接口，现代内核中热度下降；
     - 语义复杂、实现不统一，更多被 io_uring 替代。
   - io_uring：
     - 统一了“请求队列 + 完成队列”的异步抽象；
     - 覆盖读写、poll、超时、特殊命令等多种操作。
2. **驱动作者视角**
   - 对大多数字符设备驱动来说：
     - 把 `.read/.write/.poll/.fasync` 实现好，就已经可以兼容传统阻塞 I/O、poll/epoll、fasync、io_uring（通过 `POLL_ADD`）；
     - 是否实现 `->uring_cmd` 是高阶优化选项，不是基本要求。
   - 设计目标仍然是一个：**在内核端维护一个清晰、一致的事件状态机**，让上层任何通知/异步框架都能可靠工作。
3. **用户态选型建议**
   - 少量设备 + 简单逻辑 → fasync + SIGIO/signalfd 足够；
   - 大量 fd + 高吞吐 I/O → 优先 io_uring（或 epoll + 线程池）；
   - 需要与历史代码/库兼容 → fasync/epoll/io_uring 可以在同一进程内组合，但要清晰划分职责，避免重复通知与资源管理混乱。
4. **本书视角**
   - 本书关注的是“驱动级异步通知机制（fasync）”，目标是把这套机制讲清楚，并说明如何与 poll/waitqueue/input/netlink 等配合；
   - 对 io_uring，我们仅从“驱动如何与其协同”的角度说明边界：**做好 `.poll` 即可与 io_uring 的 `POLL_ADD` 自然协作**；
   - 真正深入 io_uring 的使用与优化，更适合在用户态 I/O 框架专题中展开。



------

### 12.1.5\_在实际工程中如何选择通知机制\_决策矩阵与推荐组合

#### (1)\_引入\_从\_我有一个设备\_到\_我该选哪一种通知机制

前面几节分别把 fasync 与：

- `select/poll/epoll`（通用 fd 事件多路复用）；
- input 子系统（标准化输入事件模型）；
- netlink/kobject_uevent/sysfs（系统级控制/拓扑通知）；
- AIO / io_uring（现代异步 I/O 框架）

逐一对比了作用范围与边界。

实际项目落地时，典型的起点是一个具体问题：

- *“我有一个 GPIO 中断设备，要通知用户态，怎么做最好？”*
- *“我要监控上百个 fd，还有几个字符设备，用 epoll 还是 SIGIO？”*
- *“这个设备既有高频数据，又有偶尔的模式切换，是否要 netlink + fasync 并用？”*

本小节的目标不是再引入新知识，而是把前面 12.1–12.4 的内容压缩成一套 **可操作的决策流程 + 对照表 + 推荐组合**，让你在面对新设备/新子系统时可以按步骤做出合理选择，而不是凭感觉“随便挑一个”。

------

#### (2)\_数据结构视角\_用关键维度构造\_通知机制决策矩阵

为了方便决策，可以先从几个关键维度抽象出一个“矩阵”，而不是从 API 名字出发：

1. **事件类型**
   - 数据事件（Data）：缓冲区可读/可写、采样数据到达、报文收发完成；
   - 控制/拓扑事件（Control/Topology）：设备插拔、模式切换、电源状态、路由更新等；
   - 人机输入事件（HID-like）：按键、鼠标、触摸、遥控等，面向统一输入栈。
2. **消费者数量与角色**
   - 单应用、明确 owner（一个业务进程）；
   - 多应用共享（多个非协作进程都关心）；
   - 系统守护进程（udevd、NetworkManager、专用管理 agent 等）。
3. **频率与吞吐要求**
   - 低频（偶发、秒级以下次数）；
   - 中频（几十~几百 Hz）；
   - 高频或持续流（kHz 级或更高、长期占用带宽）。
4. **延迟要求**
   - 松散：100 ms~秒级可接受；
   - 中等：几 ms~几十 ms；
   - 严格：亚毫秒级、需要精确测量并控制抖动。
5. **是否需要进入现有生态/栈**
   - 是否需要接入桌面输入框架（X/Wayland/libinput）；
   - 是否需要被通用网络管理、存储管理等守护进程感知；
   - 还是完全由某个专用应用私有使用。

在这个维度下，可以把主要机制粗略映射如下（只列出典型组合）：

| 机制                          | 主要适用事件类型 | 消费者角色          | 频率/吞吐特征            | 延迟特征           |
| ----------------------------- | ---------------- | ------------------- | ------------------------ | ------------------ |
| 阻塞 I/O（阻塞 `read`）       | 数据             | 单应用              | 低~中频                  | 依赖调度           |
| `.poll` + `select/poll/epoll` | 数据             | 单/多应用皆可       | 低~高频（适合大规模 fd） | 合理（事件型）     |
| fasync + SIGIO/signalfd       | 数据（少量 fd）  | 单应用/少数进程     | 低~中频                  | 较好（“主动提醒”） |
| input + evdev                 | 人机输入         | 通用应用/系统输入栈 | 中频                     | 由 input 栈保证    |
| netlink/kobject_uevent        | 控制/拓扑        | 系统守护进程        | 低频                     | 不适合高频         |
| sysfs + 轮询/`poll`           | 控制/状态        | 管理工具/脚本       | 低频                     | 慢，但易使用       |
| io_uring（含 `POLL_ADD`）     | 数据（大量操作） | 高性能服务/守护进程 | 中~高频、大量并发操作    | 非常好（批量提交） |

这个表不是用来死记，而是帮助你：在面对一个设备时先判断“它属于哪一列”，再映射到合适机制，而不是一开始就纠结“要不要用 fasync”。

------

#### (3)\_开发者视角\_驱动作者的分步决策流程

从“写驱动”的视角出发，你可以用如下顺序做决策。为了更明确，下面按步骤给出“是/否”式流程：

##### 1)\_第一步\_区分\_数据面\_vs\_控制面

1. 先问自己：**我要通知的是“数据可用”吗？还是“设备/模式改变”？**

   - 如果主要是：

     - 报文/采样数据到达；
     - 按键按下/松开；
     - FIFO/环形缓冲区中积累的记录；

     → 这是数据面，**优先考虑 `.read/.poll` +（可选）fasync**。

   - 如果主要是：

     - 插拔、拓扑变化；
     - 电源/模式状态改变；
     - 路由/配置策略变化；

     → 这是控制面，**优先考虑 netlink/uevent/sysfs**，而不是 fasync。

> 数据面 = 设备的“内容”，控制面 = 设备的“存在/配置”。

##### 2)\_第二步\_对于\_数据面\_先把.poll\_+\_waitqueue\_写好

- 不管最终要不要使用 fasync、io_uring、AIO，只要你的设备需要异步通知：
  1. 在 `struct demo_*_dev` 中维护：
     - `wait_queue_head_t wait`；
     - 用于判断“是否有数据”的状态变量（如 `event_count` / 缓冲区队列长度）。
  2. `.read` / `.write` 中配合 `wait_event_interruptible`，给出阻塞 I/O 语义；
  3. `.poll` 中调用 `poll_wait()` 注册 waitqueue，并根据状态返回 `EPOLLIN`/`EPOLLOUT`。
- 这是**基础设施**，也是与 `select/poll/epoll` 和 io_uring 协同的前提。

##### 3)\_第三步\_判断是否需要\_fasync

在 `.poll` 已经正确实现的前提下，考虑 fasync 的条件可以简化为：

- 满足以下至少一项时，推荐实现 fasync：

  1. **应用数量少，fd 数量少，但需要及时“被推一把”**
     - 比如一两个 GPIO 中断设备，事件稀疏但很关键；
     - 可以用 SIGIO/`signalfd` 触发逻辑；
  2. 应用已有基于 SIGIO 的框架，希望直接接入；
  3. 想要通过 `signalfd` 把多个不同来源的信号统一接入 epoll/io_uring 事件循环。

- 如果场景是：

  - 大量 fd（上百/上千）；
  - 事件频率高；
  - 逻辑更适合“中心化 epoll/io_uring”，

  → 可以只用 `.poll`，不强制引入 fasync，以免信号路径带来额外复杂度。

##### 4)\_第四步\_判断是否接入\_input\_子系统

- 如果你的设备具有如下特征：

  - 面向人机输入（按钮、键盘、鼠标、触摸板、遥控）；
  - 希望被桌面系统/标准输入栈感知；
  - 事件自然可映射为 `EV_KEY`/`EV_REL`/`EV_ABS` 等类型；

  → **优先接入 input 子系统**，驱动暴露为 `struct input_dev`，由 evdev 输出 `/dev/input/eventX`。
   此时 `.poll`/`.fasync` 等由核心层实现，你只负责调用 `input_report_*`。

- 如果设备不属于这些范畴，或语义非常特化，则保留为“普通字符设备 + fasync”更合理。

##### 5)\_第五步\_判断是否需要\_netlink/uevent/sysfs\_作为\_控制面补充

对于同一个子系统，可以同时：

- 用字符设备传递数据事件（+ fasync/poll）；
- 用 netlink/uevent/sysfs 传递拓扑/状态变化。

适用情形：

- 一块采集卡：
  - 数据缓冲区 → `/dev/demo_capture` + `.poll` + fasync；
  - 模式切换/故障/板卡插拔 → `kobject_uevent` + sysfs 属性 + 可选 netlink。

注意：同一事件不要在数据通道和控制通道上重复广播，以免重复处理。

##### 6)\_第六步\_是否需要面向\_io\_uring\_的专门优化

- 对于一般嵌入式字符设备、GPIO 驱动：
  - **不必**为 io_uring 显式实现 `->uring_cmd`；
  - 保证 `.read/.write/.poll` 实现规范即可，被 io_uring 通过 `POLL_ADD` 和 `READ/WRITE` 自然支持。
- 只有在如下场景，才考虑 io_uring 专用接口：
  - 面向高性能存储/网络/加速器，I/O 操作本身需要极低开销；
  - 上游项目已经广泛采用 io_uring，且驱动在热点路径上。

------

#### (4)\_用户/平台视角\_统一事件循环里的\_推荐组合

从用户/平台设计角度，通常希望有一个统一的事件循环（epoll 或 io_uring），把各种来源的事件统一处理。结合前面章节，几种推荐模式如下：

##### 1)\_模式\_A\_简单场景(少量设备\_+\_少量\_socket)

- 驱动：`.read/.poll`（可选 `.fasync`）。
- 用户态：
  - 使用 `poll()` 或 `epoll`；
  - 通过 `EPOLLIN` 监控 `/dev/demo_*` 和 socket/管道；
  - 若有 fasync，则可用 SIGIO/`signalfd` 做“额外提醒”，但通常一个 epoll 循环就够。

适用于：

- 单一业务进程；
- 设备数量有限；
- 性能有要求但不极端。

##### 2)\_模式\_B\_中等复杂度(多设备\_+\_多进程\_+\_系统守护)

- 驱动：
  - 数据面：字符设备 + `.poll` +（可选）fasync；
  - 控制面：sysfs 属性 + kobject_uevent / netlink。
- 用户/平台：
  - 业务进程：epoll 或 io_uring 处理数据面；
  - 系统守护（如 udev、管理 agent）：监听 netlink/uevent；
  - 对关键设备（报警类）可用 fasync + `signalfd`，并将 `signalfd` 加入 epoll/io_uring。

特征：

- 数据通道和控制通道分离，有利于调试和可扩展；
- 每类进程只关心自己应当处理的那部分事件。

##### 3)\_模式\_C\_高性能服务(大量\_fd\_+\_高频\_I/O)

- 驱动：
  - 严格实现 `.read/.write/.poll`；
  - 视情况使用 fasync 作为兼容层（不一定在高性能路径中使用）。
- 用户态：
  - 使用 io_uring 作为统一 I/O 框架；
  - 对设备 fd 使用 `IORING_OP_POLL_ADD` + `IORING_OP_READ/WRITE`；
  - 对 legacy SIGIO 驱动，假如必须使用，可通过 `signalfd` 把信号转 fd 再纳入 io_uring。

在这种场景下：

- fasync 更多作为兼容历史驱动/应用的机制；
- 新开发的设备，更推荐“`.poll` + io_uring/epoll”而不是“只用信号”。

------

#### (5)\_可视化\_通知机制选型流程图(驱动作者视角)

下面用一个流程图，把 12.5.3 的决策步骤串联起来。

```mermaid
flowchart TD
    "Q0"["Q0: 这是新设备/子系统, 需要用户态感知事件?"]

    "Q1"["Q1: 事件本质是数据面(Data)还是控制/拓扑(Control)?"]
    "DATA"["数据面(Data): 缓冲区可读/可写/采样数据/按键事件等"]
    "CTRL"["控制面(Control): 插拔/模式变化/路由/电源状态等"]

    "Q2"["Q2: 是否属于人机输入(HID): 键盘/按钮/鼠标/触摸/遥控?"]
    "USE_INPUT"["选择 input 子系统 + evdev\n(驱动: input_report_*, 上层: /dev/input/eventX)"]
    "GEN_CHAR"["通用字符设备 + .read/.poll\n(事件=自定义结构/流)"]

    "Q3"["Q3: 事件消费者主要是系统守护进程/管理进程?"]
    "USE_NETLINK"["控制面通知: netlink/kobject_uevent\n+ sysfs 属性 (低频)"]
    "NO_NETLINK"["无需系统级广播, 可留在字符设备/私有协议中"]

    "Q4"["Q4: fd 数量少, 事件稀疏, 需要被\"推一把\"?"]
    "ADD_FASYNC"["在 .poll 基础上增加 fasync + SIGIO/signalfd\n(少量 fd, 便于维护)"]
    "ONLY_POLL"["只实现 .poll, 用 select/poll/epoll/io_uring\n(不引入信号复杂度)"]

    "Q5"["Q5: 是否需要大规模 fd + 高频 I/O?"]
    "USE_IOURING"["应用层使用 io_uring\n(驱动只需保证 .poll/.read 行为规范)"]
    "NO_IOURING"["应用层采用 epoll + 线程池/单线程事件循环"]

    "Q0" --> "Q1"
    "Q1" -->|"数据面(Data)"| "Q2"
    "Q1" -->|"控制面(Control)"| "Q3"

    "Q2" -->|"是(HID)"| "USE_INPUT"
    "Q2" -->|"否"| "GEN_CHAR"

    "Q3" -->|"是"| "USE_NETLINK"
    "Q3" -->|"否"| "NO_NETLINK"

    "GEN_CHAR" --> "Q4"
    "USE_INPUT" --> "Q4"

    "Q4" -->|"是"| "ADD_FASYNC"
    "Q4" -->|"否"| "ONLY_POLL"

    "ADD_FASYNC" --> "Q5"
    "ONLY_POLL" --> "Q5"

    "Q5" -->|"是"| "USE_IOURING"
    "Q5" -->|"否"| "NO_IOURING"
```

你可以在书稿中在这张图旁边加上简要的说明文字，方便读者按图索骥。

------

#### (6)\_示例代码\_一个\_混合通知策略\_模板(伪代码级别)

下面给一个“伪代码级”的配置示例，描述一个典型工程中三个设备的选型：GPIO 按键、采集卡、控制状态对象。重点是结构与组合方式，而非完整可编译代码。

##### 1)\_设定场景

- 设备 A：板载 GPIO 按键
  - 面向人机输入；
  - 需要被桌面系统识别。
     → 使用 **input 子系统 + evdev**。
- 设备 B：高速采集卡
  - 持续传输数据块；
  - 有专用业务进程消费数据；
     → 使用 **字符设备 + `.read/.poll` + 非必须 fasync**，应用用 epoll/io_uring。
- “设备” C：板级运行模式状态（normal/safe/debug）
  - 变更不频繁；
  - 系统守护进程和配置工具都需要感知；
     → 使用 **sysfs 属性 + kobject_uevent + 可选 netlink**。

##### 2)\_用一个\_配置结构\_在内核注释中总结选型(示意)

```c
/*
 * 通知机制选型示意:
 *
 * 设备A: GPIO按键 -> input子系统
 *
 * - 驱动: demo_key_input.c
 *      - struct input_dev *idev;
 *      - input_report_key(idev, KEY_ENTER, value);
 *      - input_sync(idev);
 * - 内核: evdev 统一实现 .read/.poll/.fasync
 * - 用户: /dev/input/eventX + poll/epoll + 可选 signalfd(SIGIO)
 *
 * 设备B: 采集卡数据面 -> 字符设备 + poll (+可选fasync)
 *
 * - 驱动: demo_capture.c
 *      - struct demo_capture_dev:
 *          - wait_queue_head_t wait;
 *          - spinlock_t lock;
 *          - unsigned int buf_level;
 *          - struct fasync_struct *async_queue; (可选)
 *      - .read: 从环形缓冲区复制数据
 *      - .poll: 返回 EPOLLIN 当 buf_level > 0
 *      - .fasync: 维护 async_queue (如有需要)
 *      - 中断: 更新 buf_level, wake_up_interruptible(&wait), 可选 kill_fasync()
 * - 用户: epoll 或 io_uring(POLL_ADD + READ)
 *
 * 控制C: 板级运行模式 -> sysfs + uevent + 可选netlink
 *
 * - 驱动: demo_mode.c
 *      - struct kobject *kobj;
 *      - 属性: /sys/devices/platform/demo_board/mode
 *      - 写入 mode 时:
 *          - 更新内部状态
 *          - kobject_uevent(kobj, KOBJ_CHANGE);
 *      - 如有必要, 通过netlink广播详细状态
 * - 用户:
 *      - systemd-udevd: 收到 KOBJ_CHANGE, 触发配置脚本
 *      - 管理守护进程: 周期读 sysfs 或监听 netlink
 */
```

这个注释块不是要你直接放进驱动，而是作为“工程设计文档”的一部分——一眼就能看到每个设备采用了什么通知机制，以及它们的角色。

------

#### (7)\_调试与验证\_验证\_选型是否正确\_的检查点清单

做完选型和实现之后，建议从下面几个角度检查：

1. **语义是否清晰且不重复**
   - 数据事件是否只走一条明确的路径（字符设备/input）；
   - 控制事件是否统一走 netlink/uevent/sysfs；
   - 避免同一事件在两条通道重复广播（比如既通过 fasync 又通过 netlink）。
2. **接口与角色对应是否正确**
   - 面向普通应用的输入设备 → 是否出现在 `/dev/input/eventX` 中；
   - 面向系统管理的事件 → 是否能被 `udevadm monitor` 或 netlink 监听工具看到；
   - 面向单一业务进程的数据 → 是否通过 `/dev/demo_*` 等字符设备可达。
3. **压力和边界条件测试**
   - 高频事件下 CPU 利用率与延迟是否满足需求（可配合第 11.6 的时间戳机制）；
   - 多进程/多线程情况下，信号（fasync）路由和 fd 共享是否符合预期；
   - 插拔/异常断电/错误路径场景下，是否有遗漏通知或通知风暴。
4. **回归测试与文档**
   - 用统一的测试脚本同时测试：
     - 阻塞 `read` 行为；
     - `poll/epoll` 行为；
     - fasync/SIGIO 行为（如实现了）；
     - netlink/uevent/sysfs 行为（如实现了）；
   - 在驱动/子系统文档中明确记录：
     - 哪些事件走哪种机制；
     - 用户应当如何正确使用，避免误用。

------

#### (8)\_小结\_把\_fasync\_放回\_通知机制家族\_的工程化位置上

通过第 12 章（尤其是 12.5）的整理，我们可以给出一个工程化层面的总结：

1. **fasync 不是“孤立功能”，而是“设备级通知机制家族”中的一个成员**
   - 在字符设备场景中，fasync 提供了“驱动主动触发 + SIGIO 传递”的能力；
   - 它与 `.poll`、waitqueue、input、netlink、sysfs、io_uring 等共同构成完整的通知机制谱系；
   - 选型时应从事件类型/消费者角色/频率/延迟等维度出发，而不是先锁死在某个 API 上。
2. **驱动作者的基本原则**
   - 首先将设备抽象为“数据面 + 控制面”，为两者分别选择机制；
   - 对数据面设备，优先实现 `.read/.poll`，在此基础上再考虑 fasync；
   - 对 “人机输入” 类型设备，优先使用 input 子系统；
   - 对控制/拓扑变化，优先使用 netlink/uevent/sysfs。
3. **用户/平台的统一事件循环思路**
   - 简单场景：epoll 足以；
   - 复杂/高性能场景：可以用 io_uring 作为统一异步框架；
   - fasync 与 signalfd 可以作为现有信号型设备与现代事件循环之间的“适配层”。
4. **本书后续章节的衔接**
   - 本章解决的是“fasync 与其他机制的协作与取舍”；
   - 在后面的 **第 13 章 工程实践案例与编码核对表** 中，会将这些选型原则具体化为多个完整示例和核对表，帮助读者在真实项目中避免遗漏和误用。

至此，第 12 章“与其他通知机制的对比与整合”内容告一段落。
