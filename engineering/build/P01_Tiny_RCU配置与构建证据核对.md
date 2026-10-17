---
id: engineering.build.tiny_rcu_configuration_build_evidence
title: "Tiny_RCU配置与构建证据核对"
kind: engineering
status: evolving
domains:
  - linux
  - build
  - rcu
---

# 第1章\_Tiny\_RCU配置与构建证据核对

## 1.1\_源码能跳转不代表镜像用了这条路径

同一份内核树同时放着 Tree 和 Tiny 实现，编辑器也可能仍在使用上次构建的宏。要判断实际执行的是哪一套，必须把配置种子、生成配置、真实编译命令和最终镜像连成证据链。本篇以专用 ARM Tiny 构建为例，解决如何建立和核对这条链；运行机制见 [Tiny 源码实现](../../research/source_reading/rcu/source_explanations/P13_Linux_6.12_Tiny_RCU源码实现.md#13.1_实现所有权与本章读者任务)。

内核源码固定为 NXP Linux 6.12.20、提交 `dfaf2136deb2af2e60b994421281ba42f1c087e0`。2026 年 9 月的自定义配置仅作为历史样本，本轮没有重新构建目标内核，也未核验当前目标工作树配置。命令面向 Linux 构建环境，要求可用的目标 ARM 交叉工具链、Kbuild 构建依赖、Bear 与 clangd；先记录工具版本，不能把本章命令当成已执行日志。

2026-09-05 的历史记录指向 `lf-6.12.y` 分支提交 `7b60e547d2783f8fee61ff7d7be3e066825b9c3a`，比固定发布提交前进三个提交；当时记录的 Tiny、公共头文件、配置规则、调度与时钟入口在两者间无差异。自定义 `imx_v7_test_defconfig` 不属于固定标签，Tiny 输入还是相对当时分支头的未提交差量。这个记录只能说明那次配置，不能代替今天重新核验。

## 1.2\_配置怎样在链接期选择Tiny

先把“源码树里存在 Tiny”收紧成“当前镜像确实链接 Tiny”。需要依次核对生成配置、Kconfig 推导和 Makefile 目标；三层证据缺一层，都不能只凭文件名判断实际路径。

### 1.2.1\_从内核根目录分清应该修改与只应阅读的文件

以下路径全部以 **Linux kernel 源码根目录** 为起点。切换 Tiny RCU 时，不应看到 `kernel/rcu/Kconfig` 后就直接修改它；先按文件职责确定操作位置：

| 根目录相对路径 | 文件性质 | 本次应该怎样处理 |
| --- | --- | --- |
| `arch/arm/configs/imx_v7_test_defconfig` | 历史项目使用的配置种子 | **需要让以后重新生成配置仍选择 Tiny 时，修改这里** |
| `$O/.config` | 某一个输出目录本次构建的最终配置 | 用 `menuconfig` 或 `scripts/config` 做一次性试验；重新执行 `make ... imx_v7_test_defconfig` 会覆盖它 |
| `arch/arm/Kconfig` | `SMP` 等 ARM 构建能力的定义 | 只阅读依赖与提示，不为切换后端而改规则 |
| `kernel/Kconfig.preempt` | `PREEMPT_NONE`、`PREEMPT`、`PREEMPT_DYNAMIC` 等抢占模型的定义 | 只阅读选择关系，不为一个板级配置改全局规则 |
| `kernel/rcu/Kconfig` | `TREE_RCU`、`PREEMPT_RCU`、`TINY_RCU` 等隐藏派生符号的定义 | 只阅读派生关系；不要把修改这里当成配置内核 |
| `kernel/rcu/Makefile` | 把最终配置映射到 `tree.o` 或 `tiny.o` | 只核对链接结果，不在这里硬编码目标文件 |
| `$O/include/config/auto.conf`、`$O/include/generated/autoconf.h` | Kbuild 从 `.config` 生成的消费文件 | 只核对，禁止手工修改；下一次配置同步会重写 |

这里的 `$O` 表示传给 Kbuild 的独立输出目录。若构建时没有使用 `O=...`，表中的 `$O/.config` 就是源码根目录的 `.config`；若使用了 `O=out/tiny_rcu`，则必须检查 `out/tiny_rcu/.config`，不能继续查看源码根目录里可能过期的旧文件。

固定提交中的标准 `arch/arm/configs/imx_v7_defconfig` 同时包含 `CONFIG_SMP=y` 与 `CONFIG_PREEMPT=y`，会派生可抢占 Tree RCU。历史项目为了 i.MX6ULL 单 CPU 验证而使用的 `imx_v7_test_defconfig` 是自定义配置种子；应修改这个专用文件，不能为了得到 Tiny 而把面向多个 i.MX SoC 的标准 `imx_v7_defconfig` 一并改成 UP。

### 1.2.2\_在板级配置种子中写输入而不是硬写隐藏结果

先确认目标源码身份和输出目录。下述命令要求源码工作树适合 out-of-tree 构建；若 Kbuild 报告源目录已有生成文件，应换用经过核验的干净工作树，不能为了本实验擅自清理正在使用的树。固定发布标签没有历史自定义种子，可以在自己的实验工作树从标准 `imx_v7_defconfig` 复制出专用 `imx_v7_test_defconfig`，再按下文审查差量；已有专用种子时不覆盖它。

若目标是让 Tiny 配置可以反复生成，打开并修改 `<linux-kernel-root>/arch/arm/configs/imx_v7_test_defconfig`。

在这个文件中删除或改写会选择 SMP、完整抢占或动态抢占的输入，并明确选择无强制抢占模型：

```text
# 文件：arch/arm/configs/imx_v7_test_defconfig

# 若原文件存在下面几行，应删除或改为未启用：
# CONFIG_SMP=y
# CONFIG_PREEMPT=y
# CONFIG_PREEMPT_DYNAMIC=y

# Tiny RCU 这条构建路径需要保留的直接输入：
# CONFIG_SMP is not set
CONFIG_PREEMPT_NONE=y
# CONFIG_PREEMPT_DYNAMIC is not set
```

`choice` 是 Kconfig 用来表达互斥选项组的语法。`CONFIG_PREEMPT_NONE` 与 `CONFIG_PREEMPT`、`CONFIG_PREEMPT_VOLUNTARY`、`CONFIG_PREEMPT_RT` 属于同一个抢占模型 choice，最终只能有一个为 `y`。上面的关键不是机械凑齐所有 `# ... is not set` 注释，而是确保求解后满足 `CONFIG_SMP=n`、`CONFIG_PREEMPTION=n`。如果原配置还显式选择了其他抢占模型，必须先删除冲突项。

不要在这个 defconfig 中把 `CONFIG_PREEMPTION`、`CONFIG_PREEMPT_RCU`、`CONFIG_TREE_RCU`、`CONFIG_TINY_RCU` 四个隐藏结果当成手工输入。

它们没有面向用户的提示字符串，值由 `kernel/Kconfig.preempt` 与 `kernel/rcu/Kconfig` 根据前面的直接输入重新求解。即使把 `CONFIG_TINY_RCU=y` 硬写进配置种子，也不能替代依赖条件；下一次 `olddefconfig` 仍以 Kconfig 规则为准。

修改种子以后，从内核根目录生成一份独立配置：

```bash
kernel_root=$PWD
tiny_rcu_out="$kernel_root/out/tiny_rcu"
mkdir -p "$tiny_rcu_out"
make ARCH=arm O="$tiny_rcu_out" imx_v7_test_defconfig
make ARCH=arm O="$tiny_rcu_out" olddefconfig
```

生成目标使用配置种子，随后 `olddefconfig` 读取刚生成的 `.config` 并补齐默认值；两条命令共同把结果写入专用输出目录。不是两条命令都重新读取 defconfig。

第一条 `make` 命令读取的是 `arch/arm/configs/imx_v7_test_defconfig`，写入的是 `out/tiny_rcu/.config`。它不会把最终的几千个配置项反向写回 defconfig。

若只想临时验证，不准备修改长期配置种子，可以在生成 `$O/.config` 后使用根目录自带的 `scripts/config`：

```bash
scripts/config --file "$tiny_rcu_out/.config" \
    --disable SMP \
    --disable PREEMPT \
    --disable PREEMPT_VOLUNTARY \
    --disable PREEMPT_RT \
    --disable PREEMPT_DYNAMIC \
    --enable PREEMPT_NONE

make ARCH=arm O="$tiny_rcu_out" olddefconfig
```

这条临时路径只修改 `$O/.config`。要把结果变成项目配置，仍应审查 `make ARCH=arm O="$tiny_rcu_out" savedefconfig` 生成的 `$O/defconfig`，再把确认后的最小差量写回 `arch/arm/configs/imx_v7_test_defconfig`；不能复制整份 `.config` 冒充可维护的配置种子。

### 1.2.3\_真正生效的是生成配置

历史记录在 2026-09-05 核对、2026-09-06 复核 `.config`、`include/config/auto.conf` 和 `include/generated/autoconf.h`，三者共同确认下列配置状态（关闭项按 `.config` 的形式展示，生成头文件中对应宏未定义）：

```text
CONFIG_PREEMPT_NONE_BUILD=y
CONFIG_PREEMPT_NONE=y
CONFIG_TINY_RCU=y
# CONFIG_SMP is not set
CONFIG_PROVE_LOCKING=y
CONFIG_PROVE_RCU=y
CONFIG_DEBUG_LOCK_ALLOC=y
CONFIG_PREEMPT_COUNT=y
# CONFIG_DEBUG_OBJECTS is not set
# CONFIG_RCU_TRACE is not set
```

其中 `CONFIG_DEBUG_OBJECTS_RCU_HEAD` 依赖 `CONFIG_DEBUG_OBJECTS`，当时父开关关闭，子开关也未启用，生成头文件没有定义这两个宏。**`CONFIG_PROVE_RCU=y` 不会代替节点生命周期检查开关。** 后文必须分别判断普通 Tiny 功能路径、Lockdep 检查路径和对象生命周期检查路径，不能把它们合成“调试已开启”。

这里记录的是当时开发工作树的实际结果。读者重新生成时应检查自己的 `$O/.config`，预期的直接输入和派生结果为：

| 符号 | 预期值 | 谁决定它 |
| --- | --- | --- |
| `CONFIG_SMP` | `n` | `arch/arm/configs/imx_v7_test_defconfig` 的直接输入，定义位于 `arch/arm/Kconfig` |
| `CONFIG_PREEMPT_NONE` | `y` | 板级配置种子在 `kernel/Kconfig.preempt` 的抢占 choice 中选择 |
| `CONFIG_PREEMPT_DYNAMIC` | `n` | 板级配置种子的直接输入；避免它再选择 `PREEMPT_BUILD` |
| `CONFIG_PREEMPT_NONE_BUILD` | `y` | `PREEMPT_NONE` 在非动态抢占条件下派生 |
| `CONFIG_PREEMPTION` | `n` | 没有 `PREEMPT_BUILD` 或 `PREEMPT_RT` 选择它 |
| `CONFIG_PREEMPT_RCU` | `n` | `PREEMPTION=n`，其默认条件不成立 |
| `CONFIG_TREE_RCU` | `n` | `SMP=n` 且没有 `PREEMPT_RCU` 选择它 |
| `CONFIG_TINY_RCU` | `y` | `!PREEMPT_RCU && !SMP` 条件成立后由 `kernel/rcu/Kconfig` 派生 |
| `CONFIG_TINY_SRCU` | `y` | 由 `TINY_RCU` 默认派生；它选择另一套 SRCU 后端，不属于本章普通 RCU 状态机 |

板级 defconfig 是 Kconfig 的输入，不必显式写出隐藏符号 `CONFIG_TINY_RCU=y`。真正回答“本次编译选中了什么”的是 Kconfig 求解后的 `$O/.config` 和生成头文件。

还要注意 Kconfig 的输出形式：`CONFIG_SMP` 有用户提示，关闭后在 `.config` 中保留 `# CONFIG_SMP is not set`；`PREEMPTION`、`PREEMPT_RCU` 与 `TREE_RCU` 是没有提示字符串的隐藏布尔符号，值为 `n` 时通常直接不出现在 `.config`、`auto.conf` 和 `autoconf.h` 中。不能因为找不到一行 `# CONFIG_TREE_RCU is not set` 就认为配置证据缺失，应同时确认 Tiny 的正向选择和这些隐藏符号不存在任何 `=y` 定义。

### 1.2.4\_Kconfig先排除多CPU和可抢占普通RCU

固定 [`kernel/Kconfig.preempt`](https://github.com/nxp-imx/linux-imx/blob/dfaf2136deb2af2e60b994421281ba42f1c087e0/kernel/Kconfig.preempt) 先把用户选择的抢占模型转换成内部能力。下面按固定提交节选影响本章推导的条目；省略帮助文本、完整 choice 中的其他选项及非 RCU select，不是可直接复制替换的 Kconfig 文件。依赖条件保留在相关条目中：

```kconfig
config PREEMPT_NONE_BUILD
	bool

config PREEMPT_BUILD
	bool
	select PREEMPTION

choice
	prompt "Preemption Model"
	default PREEMPT_NONE

config PREEMPT_NONE
	bool "No Forced Preemption (Server)"
	select PREEMPT_NONE_BUILD if !PREEMPT_DYNAMIC

config PREEMPT
	bool "Preemptible Kernel (Low-Latency Desktop)"
	depends on !ARCH_NO_PREEMPT
	select PREEMPT_BUILD

config PREEMPT_RT
	bool "Fully Preemptible Kernel (Real-Time)"
	depends on EXPERT && ARCH_SUPPORTS_RT
	select PREEMPTION
endchoice

config PREEMPTION
	bool

config PREEMPT_DYNAMIC
	bool "Preemption behaviour defined on boot"
	depends on HAVE_PREEMPT_DYNAMIC && !PREEMPT_RT
	select PREEMPT_BUILD
```

Tiny 并不强制只能选 PREEMPT_NONE；可用架构上的非动态 VOLUNTARY 也可能满足非 PREEMPTION 条件。本篇采用 PREEMPT_NONE 是为了固定验证输入。

因此 `CONFIG_PREEMPT_NONE=y` 还不够；若同时让 `CONFIG_PREEMPT_DYNAMIC=y` 生效，后者仍会通过 `PREEMPT_BUILD` 选中 `PREEMPTION`，进而派生 `PREEMPT_RCU` 与 Tree RCU。当前 Tiny 验证路径明确要求 `CONFIG_PREEMPT_DYNAMIC=n`。

随后，`kernel/rcu/Kconfig` 才根据 `SMP` 与 `PREEMPTION` 的求解结果选择普通 RCU 后端：

普通后端规则的唯一展开见 [RCU 配置选择实现](../../research/source_reading/rcu/source_explanations/P17_Linux_6.12_RCU配置选择参数与树形容量源码实现.md#17.2.2_SMP与抢占模型怎样派生RCU实现)：SMP 默认选择 Tree；PREEMPTION 默认选择 PREEMPT_RCU 并带入 Tree；两者都没有时才默认选择 Tiny。

完整因果链是：

```text
arch/arm/configs/imx_v7_test_defconfig
    ├─ CONFIG_SMP=n
    ├─ CONFIG_PREEMPT_NONE=y
    └─ CONFIG_PREEMPT_DYNAMIC=n
            ↓ kernel/Kconfig.preempt
       CONFIG_PREEMPTION=n
            ↓ kernel/rcu/Kconfig
       CONFIG_PREEMPT_RCU=n
       CONFIG_TREE_RCU=n
       CONFIG_TINY_RCU=y
```

这不是运行时探测在线 CPU 数量。Kconfig 在构建前已经决定后端；同一内核镜像不会运行到一半从 Tiny 切换成 Tree。

### 1.2.5\_Makefile让同名函数互斥进入链接

固定 [RCU Makefile](../../research/source_reading/linux/kernel/rcu/Makefile) 总会链接 `update.o`/`sync.o`，再按 `CONFIG_TREE_RCU` 选择 `tree.o`、按 `CONFIG_TINY_RCU` 选择 `tiny.o`；完整片段见 [链接对象选择](../../research/source_reading/rcu/source_explanations/P17_Linux_6.12_RCU配置选择参数与树形容量源码实现.md#17.2.3_最终配置怎样决定链接哪个rcu_init)。

`update.o` 是两种后端共享的公共代码；`tree.o` 与 `tiny.o` 则由配置互斥选择。因此满足这些配置条件的构建中 `call_rcu()`、`synchronize_rcu()`、`rcu_qs()` 和 `rcu_init()` 来自 `kernel/rcu/tiny.c`。Tree 文档里的同名函数是另一种链接结果的证据，不是 Tiny 调用完成后继续执行的下一段代码。

样本配置还会得到 `CONFIG_TINY_SRCU=y`，但 Tiny SRCU 位于 `kernel/rcu/srcutiny.c`，保护的是显式 `srcu_struct` 私有域。本章的 Tiny RCU 没有 `srcu_struct` 参数，两者只因同一 UP 构建条件同时被选中，不能按文件名合并成一套机制。

## 1.3\_用Bear和构建产物核对实际编译路径

本篇采用 Bear 的实时截获模式，记录真实构建期间发生的编译器调用。因此顺序必须是：**先生成 Tiny 的 `$O/.config` → 再让专用输出目录完整重编 → 再把 Bear 生成的数据库交给语言服务器**。如果在旧的增量构建上直接运行 Bear，没有重新编译的文件不会进入数据库。

### 1.3.1\_固定源码根目录与独立输出目录

下面的命令都从 Linux kernel 根目录开始执行，并继续使用 1.2 节生成的专用输出目录：

```bash
kernel_root=$PWD
tiny_rcu_out="$kernel_root/out/tiny_rcu"
compile_db="$tiny_rcu_out/compile_commands.json"

export ARCH=arm
: "${CROSS_COMPILE:?请先把CROSS_COMPILE设置为目标ARM工具链前缀}"
export CROSS_COMPILE

# 沿用1.2已经生成并检查过的配置；不要重新加载种子覆盖临时试验差量。
test -f "$tiny_rcu_out/.config" || { echo "请先完成配置生成" >&2; exit 1; }
make O="$tiny_rcu_out" olddefconfig
```

Bear、Kbuild 与编译数据库的术语：Bear 是记录构建命令的工具；Kbuild 是 Linux 内核构建系统；`O=` 指定独立输出目录；`compile_commands.json` 按翻译单元保存工作目录和编译参数。clangd 是消费这些参数的语言服务器。

若项目的真实输出目录不是 `out/tiny_rcu`，三个变量和后续编辑器配置必须一起改，不能一边构建 `$O`，一边把源码根目录的旧 `.config` 或旧 `compile_commands.json` 交给 clangd。

### 1.3.2\_先验证配置再让Bear截获一次完整构建

先核对 Kconfig 最终结果：

```bash
grep -E '^(CONFIG_(TINY_RCU|TREE_RCU|PREEMPT_RCU|TINY_SRCU|SMP|PREEMPT_NONE|PREEMPT_NONE_BUILD|PREEMPTION|PREEMPT_DYNAMIC|PROVE_RCU)=|# CONFIG_(TINY_RCU|TREE_RCU|PREEMPT_RCU|TINY_SRCU|SMP|PREEMPT_NONE|PREEMPT_NONE_BUILD|PREEMPTION|PREEMPT_DYNAMIC|PROVE_RCU) is not set)' \
    "$tiny_rcu_out/.config"
```

本篇要求的 Tiny 路径在 `.config` 中至少应能直接看到：

```text
# CONFIG_SMP is not set
CONFIG_PREEMPT_NONE=y
CONFIG_PREEMPT_NONE_BUILD=y
CONFIG_TINY_RCU=y
CONFIG_TINY_SRCU=y
```

随后单独确认会改走其他后端的符号没有任何有效定义：

```bash
if grep -Eq '^CONFIG_(SMP|PREEMPTION|PREEMPT_DYNAMIC|PREEMPT_RCU|TREE_RCU)=y$' \
    "$tiny_rcu_out/.config"; then
    echo "错误：当前配置不是Tiny RCU的UP非抢占路径" >&2
    exit 1
fi
```

这里没有要求隐藏符号必须以注释行出现；检查的是它们不存在 `=y`。任何一项不符，都先回到 `arch/arm/configs/imx_v7_test_defconfig` 或 `$O/.config` 修正输入并重新执行 `olddefconfig`，不要带着 Tree 配置继续生成名为“Tiny”的编译数据库。

为了让 Bear 看见所有真实编译命令，只清理上面专门建立的输出目录，然后执行完整构建：

```bash
# clean只作用于O=指定的专用输出目录，保留源码树和.config。
make O="$tiny_rcu_out" clean

# Bear自身参数位于--之前，--之后完整保留实际Kbuild命令。
bear --output "$compile_db" -- \
    make -C "$kernel_root" O="$tiny_rcu_out" V=1 -j"$(nproc)"
```

这里不应复用正在承载其他配置的共享输出目录。`make clean` 的目的也不是证明 Tiny，而是确保 `tiny.c`、`update.c`、调度器和 softirq 等翻译单元都会重新调用编译器，从而被 Bear 收进同一份数据库。若构建中途失败，数据库只能覆盖失败前已经执行的编译命令，必须先修复构建再重新生成。

### 1.3.3\_核对数据库并把语言服务器指向正确位置

生成完成后，再检查 Bear 实际截获了哪个普通 RCU 实现文件：

```bash
rg -n 'kernel/rcu/(tiny|tree)\.c' "$compile_db"
```

预期实际翻译单元条目只出现 `kernel/rcu/tiny.c`，不出现 `kernel/rcu/tree.c`。这是初筛；还要检查条目的 `directory`、`file` 和 `arguments`/`command`，确认路径解析到本次源码及输出目录，而不是日志字符串或旧条目。`include/linux/rcutiny.h` 是头文件，本来就不会作为独立 C 翻译单元出现在 `compile_commands.json` 中；clangd 会通过 `tiny.c`、`update.c`、`kernel/sched/core.c` 等条目的真实 `-I`、`-D` 和生成头文件路径解析它。

打开编辑器时应把工作区根目录设为 `kernel_root`，再把 clangd 的编译数据库目录设为：

```text
<linux-kernel-root>/out/tiny_rcu
```

例如 clangd 对应的启动参数是：

```text
--compile-commands-dir=<linux-kernel-root>/out/tiny_rcu
```

不要为了让编辑器“自动发现”而复制一份旧数据库到源码根目录。必须确认编辑器实际加载的文件就是 `$compile_db`；否则界面可能仍按 Tree 的旧宏和旧头文件路径解析，出现源码明明被条件编译排除却还能跳转的假象。

配置正确后，先从数据库条目中的 `kernel/rcu/tiny.c::call_rcu()` 开始，沿下面这条最短链阅读：

```text
call_rcu
  → rcu_ctrlblk.curtail
  → rcu_note_context_switch / rcu_sched_clock_irq
  → rcu_qs
  → rcu_ctrlblk.donetail
  → raise_softirq_irqoff(RCU_SOFTIRQ)
  → rcu_process_callbacks
  → rcu_reclaim_tiny
```

若某一步不能跳转，先检查对应 `.c` 文件是否真的出现在 `$compile_db`，再检查该条目的 `directory`、`arguments` 和 `-I` 路径；不要先猜函数不存在。头文件跳转错误则继续核对编译命令是否引用了 `$O/include/generated` 对应的生成配置。

### 1.3.4\_用最终镜像与运行顺序完成验收

编译数据库只证明“编译器用什么参数处理了哪些翻译单元”，还不能单独证明最终镜像链接了对应符号。继续使用同一个输出目录核对 `vmlinux`：

```bash
"${CROSS_COMPILE}nm" -n "$tiny_rcu_out/vmlinux" \
    | rg ' (call_rcu|rcu_qs|rcu_sched_clock_irq|rcu_process_callbacks|synchronize_rcu)$'
```

旧目录里的 `tree.o` 可能是前一次配置留下的构建产物，单看文件是否存在不能证明它被当前镜像链接。这些普通 RCU 符号在不同后端可能同名，`nm` 名称本身不能区分 Tree/Tiny；局部符号也可能受优化、LTO 和裁剪影响。使用该检查时记录这些构建条件。最终判断必须同时满足：`$O/.config` 选择 Tiny、Bear 数据库包含 `tiny.c` 而不包含 `tree.c`、同一次构建生成的 `$O/vmlinux` 含有预期 RCU 符号。

如果要做最小运行观察，可以给测试模块同时布置：一个由硬中断或定时器打断的普通 reader、一个 `call_rcu()` 回调、一个在回调中置位的计数器。预期顺序必须是“reader 开始 → callback 入队 → reader 结束/调度 QS → callback 计数器变化”；若 callback 在 reader 结束前变化，就直接违反Tiny 主篇 T0～T5 状态机。


## 1.4\_把每项观察限制在它能证明的范围

```mermaid
flowchart LR
    D["板级配置种子"] -->|"Kconfig求解"| C["专用输出目录.config"]
    C -->|"Kbuild选择目标"| B["真实编译与链接"]
    B -->|"Bear截获本次调用"| DB["编译数据库"]
    DB -->|"clangd读取参数"| E["编辑器条件分支解析"]
    B -->|"链接产物"| V["同一次构建的vmlinux"]
    V -->|"部署并核对运行身份"| R["目标运行与回调观察"]
```

配置证明选项，数据库证明捕获到的编译调用，镜像证明这次链接产物，运行日志才证明目标执行。图中没有一条反向箭头允许“编辑器能跳转”直接推出“设备正在执行”。

若只临时修改输出目录的 `.config`，随后又执行板级 defconfig 目标，会发生什么？**答案：** 种子重新求解并覆盖临时输入；必须重新检查，不继续沿用此前的 Tiny 判断。若 Bear 缺少 `tiny.c`，也不能立即判成 Tree：可能根本没有重编或截获失败，先检查构建结果和数据库覆盖。

Bear 命令形式参考 [官方使用说明](https://github.com/rizsotto/Bear#how-to-use)，版本相关选项以安装版本的 `bear --help` 核对；clangd 的独立数据库位置见 [官方项目配置说明](https://clangd.llvm.org/installation.html#project-setup)。本轮只核对文档和固定源码，未执行这些交叉构建、截获或目标运行步骤。
