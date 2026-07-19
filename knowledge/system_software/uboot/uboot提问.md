# 第1章_SPL和main构建的时候_是如何区分对应的函数不会冲突的

uboot 的common/spl.c和common/board_r.c都有board_init_r()的定义，它两是如何解决重定义冲突的？

###### a)_ChatGPT_说

这个问题问得很关键 👍。表面上看，`common/spl.c` 和 `common/board_r.c` 都有一个 `board_init_r()` 函数，好像会触发 C 编译的 **重定义错误**。实际上并不会，原因在于 U-Boot 的编译系统和条件编译机制。

------

## 1.2_先看两个文件的情况

1. **`common/board_r.c`**
   - 这里的 `board_init_r()` 是 **“正常 U-Boot 启动流程”** 的入口之一。
   - 它负责从 SPL 或直接从固件跳进来后，做后续初始化（设备、控制台、环境变量、启动命令等）。
   - 在完整 U-Boot 构建时，这个文件一定会编译进去。
2. **`common/spl.c`**
   - SPL = Secondary Program Loader，U-Boot 的小型版本，主要运行在 BootROM 之后，用来初始化最基本的外设并加载“完整版 U-Boot”或内核。
   - 这里也实现了一个 **自己的 `board_init_r()`**，但是这是 **SPL 专用的版本**。
   - 它的逻辑跟完整版的 `board_init_r()` 大不相同：通常只做 RAM 初始化、镜像加载，然后跳转到镜像入口。

------

## 1.3_️_为什么不会冲突

U-Boot 的构建系统 **只会编译其中一个版本**：

- 当编译 **SPL 阶段**（`CONFIG_SPL_BUILD=y`）时：
  - `common/spl.c` 会编译进来
  - `common/board_r.c` **不会**被编译（或者内部代码被 `#ifndef CONFIG_SPL_BUILD` 排除了）
- 当编译 **正常 U-Boot** 时：
  - `common/board_r.c` 会编译进来
  - `common/spl.c` **不会**被编译

这样，最终产物里始终只有一个 `board_init_r()` 符号，避免了重定义。

------

## 1.4_原理总结

- `board_init_r()` 之所以出现在两个文件里，是因为 **SPL 和 U-Boot 主体有不同的启动逻辑需求**。
- **不会冲突的根本原因**：
  - U-Boot 构建系统在不同阶段（SPL / main U-Boot）选择性编译不同文件
  - 再辅以 `#ifdef CONFIG_SPL_BUILD` 条件宏控制

------

✅ 总结一句：
 `common/spl.c` 和 `common/board_r.c` 的 `board_init_r()` 分别服务于 **SPL** 和 **主 U-Boot**，编译系统会根据 `CONFIG_SPL_BUILD` 选择性编译其中之一，因此在最终目标文件里不会同时出现，不存在重定义冲突。

------

要不要我帮你画一个 **U-Boot 启动流程（SPL → board_init_f → board_init_r）** 的 Mermaid 流程图？这样对比 SPL/正常 U-Boot 的分工会更直观。

##### 1)_您说

问题就是common/board_r.o的加入是直接假如到ALL-y makefile变量。并不是通过宏分支来判断的，它是哪里做了分流吗？

###### a)_ChatGPT_说

好问题 👍，你已经注意到 **`common/board_r.o` 是通过 `ALL-y` 直接加到编译列表里的**，看起来无论 `SPL` 还是 `正常 U-Boot` 都会被编译进去，那岂不是会和 `common/spl.o` 一起产生两个 `board_init_r()` 吗？
 其实这里确实做了**分流处理**，原理要分两层来看：

------

## 1.5_Kbuild_构建体系的_两个世界

U-Boot 的构建系统里，**SPL 和 main U-Boot 是两个独立目标**，它们虽然都叫 `u-boot`，但实际上有：

- `u-boot` → 主 U-Boot ELF/镜像
- `spl/u-boot-spl` → SPL ELF/镜像

构建时，SPL 进入 `spl/Makefile` 的 Kbuild 环境；而正常 U-Boot 使用顶层 `Makefile`。
 这两个目标分别维护自己的一套 `obj-y` / `ALL-y` 文件列表。

👉 所以 **SPL 并不会去编译主 U-Boot 的 `common/board_r.c`**，即便它在 `ALL-y` 里出现。

------

## 1.6_那_board_r.o_怎么被屏蔽的

进入细节你会发现：

- `common/board_r.c` 的内容里，大部分函数（包括 `board_init_r()`）都有条件编译保护：

```
#ifndef CONFIG_SPL_BUILD
void board_init_r(gd_t *id, ulong dest_addr)
{
    ...
}
#endif
```

也就是说：

- 即便 `common/board_r.c` 在 SPL 的文件列表里被编译了
- **只要 `CONFIG_SPL_BUILD=y`，源文件里 `board_init_r()` 的定义就不会生成**

这样最终 SPL 编译出来的 `common/board_r.o` 里就不会有 `board_init_r()` 符号，自然和 `spl.c` 里的定义不冲突。

------

## 1.7_双保险机制

所以总结下其实有两层“分流”：

1. **构建目标分离**
   - SPL 构建用自己的 obj-y，正常 U-Boot 构建用自己的 obj-y。
   - 实际情况是 SPL 部分代码也确实会编译 `common/board_r.o`，但这只是局部 obj 的行为。
2. **宏条件裁剪**
   - `common/board_r.c` 里的 `board_init_r()` 被 `#ifndef CONFIG_SPL_BUILD` 包围。
   - 在 SPL 构建时，它根本不会生成函数定义。

这样保证了 **最终链接时只有一个有效的 `board_init_r()`**。

------

✅ 总结一句：
 虽然 `common/board_r.o` 确实是加在 `ALL-y` 里的，但在 **SPL 构建**时，源码中的 `board_init_r()` 会被 `#ifndef CONFIG_SPL_BUILD` 屏蔽掉；而在 **正常 U-Boot 构建**时，`common/spl.o` 不会编译。因此两边不会产生重定义冲突。

------

要不要我帮你写一个 **符号分流表**（SPL vs main U-Boot 的 `board_init_r()` 版本对照），把这两个文件在不同构建阶段各自的生效逻辑画出来？

##### 1)_您说

可以

###### a)_ChatGPT_说

好 👌 我帮你整理一个 **符号分流表**，明确说明在 **SPL** 和 **正常 U-Boot** 两个构建目标下，`board_init_r()` 的来源。

------

## 1.8_U-Boot_中_board_init_r()_的分流表

| 构建阶段                                         | 来源文件           | 是否编译 | 是否生成 `board_init_r()`                      | 说明                                                         |
| ------------------------------------------------ | ------------------ | -------- | ---------------------------------------------- | ------------------------------------------------------------ |
| **SPL 构建** (`CONFIG_SPL_BUILD=y`)              | `common/spl.c`     | ✅ 编译   | ✅ 生成                                         | 提供 SPL 的 `board_init_r()`，负责初始化 RAM、加载 U-Boot 镜像/内核等 |
|                                                  | `common/board_r.c` | ✅ 编译   | ❌ 不生成（被 `#ifndef CONFIG_SPL_BUILD` 屏蔽） | 只保留部分通用函数，不包含 `board_init_r()`                  |
| **正常 U-Boot 构建** (`CONFIG_SPL_BUILD` 未定义) | `common/spl.c`     | ❌ 不编译 | ❌ 无                                           | SPL 代码不会进入 main U-Boot                                 |
|                                                  | `common/board_r.c` | ✅ 编译   | ✅ 生成                                         | 提供 main U-Boot 的 `board_init_r()`，负责设备初始化、环境变量、命令行、启动内核等 |

------

## 1.9_原理总结

- **SPL 模式**：
  - `spl.c` 提供自己的 `board_init_r()`
  - `board_r.c` 编译但其中 `board_init_r()` 被宏排除
- **正常 U-Boot 模式**：
  - 只编译 `board_r.c` 的版本
  - 不会编译 `spl.c`

所以最终无论哪个阶段，链接时都只有一个 `board_init_r()`。

------

## 1.10_️_Mermaid_图示(SPL_vs_main_U-Boot)

```
flowchart TD
    subgraph SPL["SPL 构建 (CONFIG_SPL_BUILD=y)"]
        A1[common/spl.c] -->|生成| B1[board_init_r() (SPL版)]
        A2[common/board_r.c] -->|宏屏蔽| B2[无 board_init_r()]
    end

    subgraph MAIN["正常 U-Boot 构建"]
        C1[common/spl.c] -->|不编译| C2[无 board_init_r()]
        C3[common/board_r.c] -->|生成| D1[board_init_r() (main版)]
    end
```

------

这样你就能清晰看到：

- **SPL 阶段**：来自 `spl.c`
- **主 U-Boot 阶段**：来自 `board_r.c`

------

要不要我再帮你把两个版本的 `board_init_r()` 的 **典型执行内容对比**（SPL 和 main U-Boot 各自负责的初始化任务）也列出来？

##### 1)_您说

① Kbuild 构建体系的“两个世界” U-Boot 的构建系统里，SPL 和 main U-Boot 是两个独立目标，它们虽然都叫 u-boot，但实际上有： u-boot → 主 U-Boot ELF/镜像 spl/u-boot-spl → SPL ELF/镜像 构建时，SPL 进入 spl/Makefile 的 Kbuild 环境；而正常 U-Boot 使用顶层 Makefile。 这两个目标分别维护自己的一套 obj-y / ALL-y 文件列表。 👉 所以 SPL 并不会去编译主 U-Boot 的 common/board_r.c，即便它在 ALL-y 里出现。 详细展开讲解这部分内容。在哪个官方文档可以查阅到这些信息？

###### a)_ChatGPT_说

好问题 👍，这部分是 **理解 U-Boot 构建体系的关键**。我帮你详细展开「SPL 和 main U-Boot 是两个独立目标」这一点，并给出在哪些 **官方文档 / 源码路径** 能确认。

------

## 1.11_U-Boot_构建的两个目标

U-Boot 的顶层构建体系是基于 **Kbuild**（和 Linux 内核类似），所以最终产物不是单一的 ELF，而是可能有多个目标：

- **主 U-Boot**
  - 最终产物：`u-boot` (ELF) / `u-boot.bin` / `u-boot.img`
  - 目标由 **顶层 `Makefile`** 驱动
  - 使用 `obj-y`/`ALL-y` 定义需要编译的对象文件
  - 链接脚本：`u-boot.lds`
- **SPL**（Secondary Program Loader）
  - 最终产物：`spl/u-boot-spl` (ELF) / `spl/u-boot-spl.bin`
  - 目标由 **`spl/Makefile`** 驱动
  - 拥有一套独立的 `obj-y` 列表（并不是主 U-Boot 的子集，而是根据 `CONFIG_SPL_BUILD` 裁剪出来的）
  - 链接脚本：`spl/u-boot-spl.lds`

这就是为什么即便 `common/board_r.o` 出现在顶层的 `ALL-y`，**在 SPL 编译时，真正生效的是 SPL 的 `obj-y` 文件列表**，而不是 main U-Boot 的 `ALL-y`。

------

## 1.12_代码里是怎么实现分流的

可以从源码里看到：

1. **顶层 `Makefile`**

   - 定义了 main U-Boot 的编译规则
   - 其中目标 `u-boot` 和 `u-boot.bin`、`u-boot.img` 都依赖主 `obj-y`

   📍 位置：

   ```
   u-boot/Makefile
   ```

2. **SPL 的 Makefile**

   - 进入 `spl/` 目录后，有自己的 `Makefile`
   - 这里定义了 `obj-y`，并且在执行时 `CONFIG_SPL_BUILD=y` 会被打开
   - 这使得 SPL 的构建“切换”到另一个编译上下文

   📍 位置：

   ```
   u-boot/spl/Makefile
   ```

3. **Kbuild 驱动**

   - 无论是顶层还是 SPL，构建最终都会通过 `scripts/Makefile.build`（从 Linux 内核借鉴的）来递归处理 `obj-y`/`obj-m`
   - 所以主 U-Boot 和 SPL 虽然共享代码目录（`common/`, `arch/`, `drivers/`），但实际编译的文件集不同

------

## 1.13_官方文档可以查的地方

1. **U-Boot 官方文档目录**

   - 📍 `doc/README.SPL`

     > 专门讲了 SPL 的用途、构建方式、和主 U-Boot 的关系

   - 📍 `doc/develop/build.rst`

     > 讲 U-Boot 的构建系统，如何使用 Kbuild，以及多目标的处理方式

   - 📍 `doc/develop/makefiles.rst`

     > 解释了 Makefile/Kbuild 在 U-Boot 中的角色（和 Linux 内核文档类似）

2. **Kconfig 配置入口**

   - 📍 `Kconfig` 里 `menuconfig SPL` 一节
   - 可以看到 `CONFIG_SPL_BUILD` 等宏如何控制 **是否启用 SPL 专用的文件列表**

3. **源码验证路径**

   - `spl/Makefile`（SPL 的独立目标规则）
   - `common/Makefile`（可以看到某些对象带有 `ifdef CONFIG_SPL_BUILD` 条件）
   - `scripts/Makefile.build`（共享的 Kbuild 基础设施）

------

## 1.14_总结一句

- **SPL 和 main U-Boot 是两个独立的构建目标**
- 它们分别由 **顶层 `Makefile`** 和 **`spl/Makefile`** 驱动，最终各自生成 `u-boot` 和 `spl/u-boot-spl`
- `obj-y`/`ALL-y` 是 **在不同上下文下单独维护的**，所以 `common/board_r.o` 出现在 `ALL-y` 并不意味着它一定进入 SPL
- 这部分在 **`doc/README.SPL` 和 `doc/develop/build.rst`** 中有明确描述

------

要不要我帮你整理一张 **“构建流程图（顶层 Makefile → SPL Makefile → obj-y 选择）”** 的 Mermaid 图？这样能直观看出 `board_r.o` 是如何被排除在 SPL 链接之外的。

##### 1)_您说

也就是说在makefile中构建目标的时候用make -f命令指定makefile执行的时候，这个指定的makefile将不会继承构建目标的makefile的环境变量，通过这种方式实现了环境上的隔离？

###### a)_ChatGPT_说

你理解的方向对了一半，但 **U-Boot 构建系统并不是简单地用 `make -f` 切换到一个完全独立的 Makefile**，而是通过 **Kbuild 体系** 实现“同一套 make 环境里，不同目标使用不同的文件列表”，具体来说：

------

## 1.15_U-Boot_构建并没有彻底的环境隔离

- 构建 SPL 时，并不是重新开一个 `make -f spl/Makefile` 的干净环境。
- 实际上，顶层 `Makefile` 在解析配置后，会根据 `CONFIG_SPL_BUILD` 来调用 **`subdir` 的构建规则**。
- 构建 SPL 时会进入 `spl/Makefile`，但它依然使用顶层 Makefile 导出的 Kbuild 环境变量（比如编译器、CFLAGS、CONFIG 宏等）。

👉 所以 SPL 的构建并不是“完全隔离”，而是 **继承了全局环境，但切换了自己的 `obj-y` 文件集和链接规则**。

------

## 1.16_隔离是通过「构建上下文」实现的

Kbuild 的核心逻辑在 `scripts/Makefile.build` 里。大致机制是：

- **顶层 Makefile**
  - 定义 `obj-y`（主 U-Boot 的对象文件列表）。
  - 定义目标 `u-boot`。
- **spl/Makefile**
  - 定义 `obj-y`（SPL 专用对象文件列表）。
  - 定义目标 `u-boot-spl`。

当顶层要构建 SPL 时，会调用：

```
$(Q)$(MAKE) obj=spl -f $(srctree)/spl/Makefile
```

这时：

- 环境变量（编译器、CONFIG 宏等）依然继承顶层。
- 但 `obj-y` 列表和链接脚本切换为 SPL 版本。

👉 这就是“逻辑隔离”的本质：**不是丢掉环境变量，而是切换了 Kbuild 的上下文**。

------

## 1.17_为什么这样设计

- 保证 SPL 和 main U-Boot **共享同一套工具链、CONFIG 配置、编译规则**，避免环境漂移。
- 但在目标级别（`u-boot` vs `u-boot-spl`）保持独立，方便裁剪。

------

## 1.18_总结一句

不是通过 `make -f` 打开一个完全独立的 Makefile 来隔离，而是：

- **环境变量继承**：编译器、CONFIG 宏等保持一致；
- **构建上下文隔离**：通过 `obj-y` 文件列表、链接脚本和 `CONFIG_SPL_BUILD` 条件来控制；
- 结果就是同一套源码在不同上下文下生成两个独立 ELF：`u-boot` 和 `u-boot-spl`。

------

要不要我帮你画一个 **“继承 vs 隔离”关系的流程图**（顶层环境变量继承 + 构建上下文分流），这样能更直观地理解 SPL 和 main U-Boot 的关系？

##### 1)_您说

让我们重新整理下这里的规则： 1. makefile的环境变量还是会下传。 2. uboot顶层makefile还是会有ALL-y变量存在。 3. 在构建u-boot和u-boot-spl是两套makefile。 也就是说构建u-boot和u-boot-spl的两套makefile之间的环境变量是不互通的。而顶层makefile的环境会下放到-f指定的makefile。这样隔离的环境变量的方法不是通过消灭继承关系，而是直接分家来处理的？

###### a)_ChatGPT_说

你总结得很到位 👍，我帮你再捋清楚一下逻辑，把「继承 vs 分家」这个点说透：

------

## 1.19_环境变量的传递

- GNU Make 的特性决定了：当顶层 Makefile 里执行

  ```
  $(MAKE) -f spl/Makefile ...
  ```

  时，**环境变量和大多数 Make 变量都会传递下去**。

- 所以 **工具链、CFLAGS、CONFIG 宏** 等全局编译环境，在 `u-boot` 和 `u-boot-spl` 之间是共享的。

------

## 1.20_文件列表的分家

- 顶层 Makefile 会维护 **main U-Boot 的 `obj-y/ALL-y`**，对应产物是 `u-boot`。
- `spl/Makefile` 会维护 **SPL 的 `obj-y`**，对应产物是 `u-boot-spl`。
- 这两个 Makefile **不会互相干扰**，因为它们各自有独立的 `obj-y` 集合和链接规则。
- 虽然两者运行在同一个编译环境下，但“要编哪些文件”完全分家。

------

## 1.21_所以隔离的本质

- **不是**：阻断继承（make -f 并不会打断环境变量的传递）。
- **而是**：
  - 编译环境（变量、工具链） → **继承共享**
  - 构建上下文（obj-y/ALL-y、链接脚本） → **分家管理**

这就是为什么你能在主 U-Boot 里看到 `common/board_r.o` 出现在 `ALL-y`，但在 SPL 构建结果里不会出现：
 👉 因为 SPL 根本就用的 **另一套 obj-y 列表**。

------

## 1.22_总结一句

U-Boot 的做法是：

- **继承编译环境**（保证一致性）；
- **分家文件列表和目标**（保证裁剪和隔离）；

所以你说的很对：隔离并不是靠“消灭继承”，而是通过 **分家 Makefile 各自维护构建上下文** 来实现的。



# 第2章_uboot是如何分流出armv7和armv8架构的

根据上述的uboot的文件的规则。我想要知道的是怎么区分出armv7和armv8的区别的，在哪个目标或者宏做了这种分流的开关？

###### a)_ChatGPT_说

好问题 👍，你已经把 SPL vs main U-Boot 的构建隔离搞清楚了，现在你问的是 **在 U-Boot 构建体系里，armv7 和 armv8 是怎么区分的**。我来帮你把“分流开关”梳理一下：

------

## 2.2_分流关键变量_CONFIG_SYS_ARCH_和_CONFIG_SYS_CPU

在你贴的那个 `config.mk` 里有这么一段：

```
ARCH := $(CONFIG_SYS_ARCH:"%"=%)
CPU  := $(CONFIG_SYS_CPU:"%"=%)
CPUDIR = arch/$(ARCH)/cpu$(if $(CPU),/$(CPU),)
```

这几个宏变量是 **Kconfig 配置系统生成的**，对应 `.config` 里的值。

- 对于 ARM 架构：
  - `CONFIG_SYS_ARCH=arm`
- 对于 ARMv7 vs ARMv8：
  - ARMv7: `CONFIG_SYS_CPU=armv7`
  - ARMv8: `CONFIG_SYS_CPU=armv8`

于是 `CPUDIR` 分别会指向：

- ARMv7 → `arch/arm/cpu/armv7/`
- ARMv8 → `arch/arm/cpu/armv8/`

**这就是代码分流的根源。**

------

## 2.3_谁来设置_CONFIG_SYS_CPU

这个值来自 **目标板 defconfig + arch/arm Kconfig**。

比如 `configs/qemu_arm_defconfig`（ARMv7）里面会有：

```
CONFIG_SYS_ARCH="arm"
CONFIG_SYS_CPU="armv7"
```

而 `configs/qemu_arm64_defconfig`（ARMv8）则是：

```
CONFIG_SYS_ARCH="arm"
CONFIG_SYS_CPU="armv8"
```

这些配置是 **板级 defconfig → `Kconfig` → 生成 `.config` → 导出到 Makefile** 的过程。

------

## 2.4_具体入口文件的选择

- 当 `CONFIG_SYS_CPU=armv7` → `CPUDIR=arch/arm/cpu/armv7/`
  - `_start` 来自 `arch/arm/cpu/armv7/start.S`
- 当 `CONFIG_SYS_CPU=armv8` → `CPUDIR=arch/arm/cpu/armv8/`
  - `_start` 来自 `arch/arm/cpu/armv8/start.S`

这里的选择是 **Kbuild 自动根据 obj-y 引入**，因为每个 `arch/arm/cpu/armv*/Makefile` 都会把 `start.o` 加入对应的目标文件里。

------

## 2.5_额外的辅助宏

在 C 代码里，有时会看到：

```
#ifdef CONFIG_ARM64
   // ARMv8 专属代码
#else
   // ARMv7 专属代码
#endif
```

这个 `CONFIG_ARM64` 也是由 `CONFIG_SYS_CPU=armv8` 派生出来的（见 `arch/arm/Kconfig`）。

- ARMv7 defconfig → **没开 CONFIG_ARM64**
- ARMv8 defconfig → **自动启用 CONFIG_ARM64=y**

这样不仅汇编入口不同，C 文件编译时也能做条件编译。

------

## 2.6_总结

所以 ARMv7 vs ARMv8 的分流机制是：

1. **Kconfig 层**：
   - 板级 defconfig 里指定 `CONFIG_SYS_CPU=armv7/armv8`
   - ARMv8 同时打开 `CONFIG_ARM64=y`
2. **Makefile 层**：
   - `config.mk` 解析 `CONFIG_SYS_CPU`，生成 `CPUDIR` 路径
   - Kbuild 在 `arch/arm/cpu/armv7/` 或 `armv8/` 下找 `start.o` 作为 `_start`
3. **C 代码层**：
   - `CONFIG_ARM64` 宏控制 ARMv8 专属代码编译
   - ARMv7 不定义该宏

👉 所以，**开关点在 defconfig/Kconfig 中的 `CONFIG_SYS_CPU` 和 `CONFIG_ARM64`**。

------

要不要我帮你把 **“从 defconfig 到 start.S 的路径选择”** 画一张 Mermaid 图？
 这样你能直观看到 ARMv7 vs ARMv8 的分流点在哪里。

##### 1)_您说

问题就在于_start:不在arch/arm/cpu/armv7/start.S中定义，而armv8却有对应的定义。是哪里对他两做了区分？既然都区分了armv7和armv8为何不直接在这种对称目录里面进行区分反而一样一个文件，还不对称存放？

###### a)_ChatGPT_说

啊，明白你的意思了 👍
 你注意到：

- **ARMv8** → `arch/arm/cpu/armv8/start.S` 里面就直接有 `_start:` 符号；
- **ARMv7** → `arch/arm/cpu/armv7/start.S` 里面没有 `_start:`，入口在别的地方；

于是问题是：
 👉 为什么 ARMv7/ARMv8 **不对称**？是谁决定了 `_start` 放在哪个文件？

------

## 2.7_ARMv7_的_start_在哪里

在 U-Boot 的 ARMv7 架构里，入口点 `_start` **不在 `arch/arm/cpu/armv7/start.S`**，而是在 **通用目录 `arch/arm/lib/vectors.S`** 中定义的。

具体逻辑是：

- `arch/arm/cpu/armv7/start.S` → 做的是低级初始化，比如设置栈、关 MMU、跳转到 C 入口；
- 真正的 `_start:` 标签定义在 `arch/arm/lib/vectors.S`，这个文件提供了 ARMv7 **中断向量表 + reset 向量（也就是 `_start`）**。

也就是说，ARMv7 体系里把“复位向量表”和“启动代码”拆开了：

- **vectors.S** 提供 `_start` (Reset 入口) + 其他异常向量；
- **start.S** 负责 Reset handler 后的初始化。

------

## 2.8_ARMv8_为什么不同

ARMv8 架构变化比较大：

- ARMv8 没有固定的“异常向量表 = 固定地址”这种 legacy 机制了，而是通过 `VBAR_ELx` 来设定；
- U-Boot 在 ARMv8 中选择 **把 `_start:` 直接放在 `arch/arm/cpu/armv8/start.S` 里**，而不是像 ARMv7 那样分两个文件。

所以：

- ARMv7 遵循“ARM 传统模式”：复位向量和异常表在同一张表（`vectors.S`），所以 `_start` 必须放那里；
- ARMv8 没有这种硬约束，U-Boot 开发者就把 `_start` 和初始化放到一个文件里了。

------

## 2.9_构建系统怎么区分

这就是你问的“谁来分流”的部分：

在 `arch/arm/Makefile` 里：

```
obj-$(CONFIG_ARM64) += cpu/armv8/
obj-$(CONFIG_ARM64) += lib/...
obj-$(CONFIG_ARM64) += ...
```

而在非 `CONFIG_ARM64` 的情况（ARMv7 及以下）：

```
obj-y += lib/vectors.o
obj-y += cpu/armv7/start.o
```

也就是说：

- 对 **ARMv7** 架构，Kbuild 会拉进 `arch/arm/lib/vectors.o`（里面有 `_start`），再配合 `cpu/armv7/start.o`；
- 对 **ARMv8** 架构，Kbuild 则直接编译 `cpu/armv8/start.o`（里面自带 `_start`）。

------

## 2.10_为什么不_对称

这是历史遗留 + 架构差异的结果：

- **ARMv7** 设计时必须要中断向量表（`vectors.S`）放在地址 `0x0` 或 `0xFFFF0000`，所以 `_start` 只能放在 vectors 里；
- **ARMv8** 没有这个死板的向量表地址要求，可以自由决定 `_start` 放在哪，于是 U-Boot 干脆放到 `start.S`，减少文件拆分。

👉 所以不对称其实是 **架构差异 + 兼容旧 ARMv7 设计**导致的。

------

## 2.11_总结

1. ARMv7 的 `_start` 在 `arch/arm/lib/vectors.S`，而不是 `cpu/armv7/start.S`；
2. ARMv8 的 `_start` 在 `arch/arm/cpu/armv8/start.S`，因为异常向量表机制不同；
3. 构建系统通过 `CONFIG_ARM64` 宏在 `arch/arm/Makefile` 分流：
   - ARMv7 → vectors.S + start.S
   - ARMv8 → start.S

所以 **不对称**是合理的，因为两代架构对异常向量和 reset 入口的要求完全不同。
