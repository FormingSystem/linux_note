---
id: research.uboot.source_explanations.image_handoff
title: "镜像范围与Linux交接实现"
kind: source
status: evolving
domains:
  - boot
  - source_reading
---

# 第11章\_镜像范围与Linux交接实现

本页固定上游 U-Boot v2022.10、提交 `4debc57a3da6c3f4d3f89a637e99206f4cea0a96`；从 [模块导读](../navigation/P11_镜像解析与Linux交接导读.md) 按 M0～M5 阅读，整体版本见 [总索引](../navigation/README.md)。裁剪源码沿用上游 GPL-2.0+，中文 Doxygen 和行内解释为仓库补充；未展示的分支明确说明，不把片段当成可替换的完整实现。

## 11.1\_M1与M2\_阶段位怎样驱动实际操作

**讲解层次：实现层，`cmd/bootm.c`、`boot/bootm.c` 与 `include/image.h`。** `do_bootm` 消费命令名，区分阶段子命令与地址/组件参数；普通完整路径向 `do_bootm_states` 传一组请求位。该函数依次检查每一位及此前返回值，并不是根据宏的数值大小自动生成执行顺序。

| 位名 | 本版数值 | 当前操作 |
| --- | --- | --- |
| `START` | `0x001` | 清空全局描述、确定校验选择、初始化范围管理 |
| `PRE_LOAD` | `0x800` | 配置适用时执行预加载检查 |
| `FINDOS` | `0x002` | 识别已有内存镜像并记录内核属性 |
| `FINDOTHER` | `0x004` | 查找 RAM 盘、DTB 及配置适用的其他组件 |
| `LOADOS` | `0x008` | 关闭有关活动并安置/解压内核 |
| `RAMDISK`、`FDT` | `0x010`、`0x020` | 相应配置下的独立安置阶段 |
| `OS_CMDLINE`、`OS_BD_T` | `0x040`、`0x080` | 操作系统特定准备；ARM Linux 回调不支持这两个独立状态 |
| `OS_PREP` | `0x100` | 处理命令行并调用操作系统准备函数 |
| `OS_FAKE_GO`、`OS_GO` | `0x200`、`0x400` | trace 假交接或真正交接 |

表中统一省略 `BOOTM_STATE_` 前缀。`PRE_LOAD` 数值最大，却在 `FINDOS` 之前执行，已经说明数值不是流水线序号。下面裁剪最初的推进部分。

```c
/**
 * @brief 仓库阅读说明：请求位先写入，随后才执行相应操作。
 * @note 裁剪 do_bootm_states；后面的装载、OS 回调与错误出口见正文。
 */
images->state |= states;
if (states & BOOTM_STATE_START)
    ret = bootm_start(cmdtp, flag, argc, argv);
if (!ret && (states & BOOTM_STATE_PRE_LOAD))
    ret = bootm_pre_load(cmdtp, flag, argc, argv);
if (!ret && (states & BOOTM_STATE_FINDOS))
    ret = bootm_find_os(cmdtp, flag, argc, argv);
if (!ret && (states & BOOTM_STATE_FINDOTHER))
    ret = bootm_find_other(cmdtp, flag, argc, argv);
```

`bootm_start` 另对全局 `images` 做 `memset`，保存环境的校验选择、初始化 LMB，并把状态设为 `START`。所以这个字段既有预先并入请求位的行为，又有开始阶段重置，不能作为事务式成功记录。子命令路径的顺序检查也不能替代参数及前阶段结果核对。

`bootm_find_os` 经 `boot_get_kernel` 从参数或默认地址取得镜像地址，`map_sysmem` 后识别格式。旧式格式读取头部并复制一份头供后续使用；FIT 路径选择配置/组件并取得载荷属性。查得的 `os.image_start/image_len` 是源载荷，`os.load` 是目的位置，`ep` 则是入口。这里没有一个统一的 NAND/MMC 文件读取调用。

M2 中 `bootm_load_os` 把源和目的映射后调用 `image_decomp`，再用 `load_end-load` 更新 `images->os.image_len`，进行缓存维护及重叠检查，成功后登记目标范围。无压缩且原地执行有专门的不重叠处理。旧式镜像返回 `BOOTM_ERR_OVERLAP` 时，当前调用者会清掉该返回值继续；其他容器被覆盖可能返回 `BOOTM_ERR_RESET`。这不是“所有重叠都安全”或“所有重叠都在写入前拒绝”的保证。

## 11.2\_booti怎样计算ARM64Image的目标地址

`cmd/booti.c` 自行处理裸 Image 的前段：可选解压、调用架构 `booti_setup`、必要时搬移、写 `images.ep/os.start/os.end` 并登记范围，然后查找其他镜像。`do_booti` 另关闭中断、设 Linux/架构类型，再进入公共的 RAM 盘及 `OS_PREP/GO` 阶段。它复用后半段，不是先把裸 Image 伪装成完整 FIT。

在 `arch/arm/lib/image.c`，`Image_header` 中本次用到以下字段。偏移是 Image 文件头内的字节位置，不是 MMIO 寄存器。

| 字段 | 偏移/宽度 | 用途 |
| --- | --- | --- |
| `text_offset` | `0x08` / 64 位 | 内核相对对齐基址的偏移 |
| `image_size` | `0x10` / 64 位 | 有效镜像大小；零值有历史兼容分支 |
| `flags` | `0x18` / 64 位 | 此函数用 bit 3 决定是否尽量保留输入所在基址 |
| `magic` | `0x38` / 32 位 | 核对 ARM64 Image 标识 `0x644d5241` |

```c
/**
 * @brief 仓库阅读说明：目标计算只决定地址和长度，不证明整段内存可写。
 * @note 裁剪 booti_setup 中已通过 magic 检查后的部分。
 */
if (ih->image_size == 0) {
    image_size = 16 << 20;       /* 旧头部的兼容假定 */
    text_offset = 0x80000;
} else {
    image_size = le64_to_cpu(ih->image_size);
    text_offset = le64_to_cpu(ih->text_offset);
}
*size = image_size;
if (!force_reloc && (le64_to_cpu(ih->flags) & BIT(3)))
    dst = image - text_offset;
else
    dst = gd->bd->bi_dram[0].start;
*relocated_addr = ALIGN(dst, SZ_2M) + text_offset;
```

例如 RAM 起点为 `0x40000000`、偏移为 `0x80000`，要求按 RAM 基址布局时得到 `0x40080000`；若输入在 `0x42080000`、bit 3 允许且不强制搬移，计算仍得 `0x42080000`。输入改成不符合该对齐关系的地址时，`ALIGN` 可能改变结果，所以“允许任意物理位置”不等于忽略 Image 的对齐合同。头部大小为零的旧格式分支也不能套用到已提供有效大小的新镜像。

函数不在这里完整证明目标长度处于所有可用 DRAM 范围内。压缩路径还需要额外工作区，其地址、大小和与其他对象的关系要独立规划；仅满足 `kernel_comp_addr_r` 的起始地址检查，不足以证明整个解压输出安全。

## 11.3\_M3\_设备树位置和内容怎样一起改变

**讲解层次：实现层，`arch/arm/lib/bootm.c`、`boot/image-board.c`、`boot/image-fdt.c`。** ARM Linux 准备路径在 `OF_LIBFDT`、`LMB` 和有效树长度条件下进入 DTB 准备，否则按配置使用 ATAGS 或报错。不能把 ARM32 的 ATAGS 退路写成 ARM64 Linux 的通用入口协议。

```c
/**
 * @brief 仓库阅读说明：DTB 地址通过 images 内字段的地址传递，安置可更新它。
 * @note 裁剪 image_setup_linux；省略保留区登记及可选命令行分配。
 */
ulong of_size = images->ft_len;
char **of_flat_tree = &images->ft_addr;
struct lmb *lmb = &images->lmb;
/* 前面先处理已有保留记录和配置适用的命令行范围。 */
if (CONFIG_IS_ENABLED(OF_LIBFDT)) {
    ret = boot_relocate_fdt(lmb, of_flat_tree, &of_size);
    if (ret)
        return ret;
}
if (CONFIG_IS_ENABLED(OF_LIBFDT) && of_size) {
    ret = image_setup_libfdt(images, *of_flat_tree, of_size, lmb);
    if (ret)
        return ret;
}
```

`of_size` 是局部变量，`of_flat_tree` 却指向描述对象里的地址字段；不要看到局部长度变化，就假设每处都同步写回 `images.ft_len`。安置函数在普通路径分配并 `fdt_open_into`，在 `fdt_high=~0UL` 路径原地扩展总长度并登记范围，代码明确假定原树后有可用余量。

`image_setup_libfdt` 顺序准备根和 `/chosen`、架构修订、其他配置适用的属性及板钩子，随后调整树大小、更新范围登记并写 RAM 盘边界。`common/fdt_support.c::fdt_chosen` 通过可覆盖的 `board_fdt_chosen_bootargs` 取得字符串，默认返回环境 `bootargs`；非空时写 `/chosen/bootargs`，不是无条件覆盖或删除。

`arch/arm/lib/bootm-fdt.c::arch_fixup_fdt` 按板 DRAM 信息更新内存范围，按配置调用 spin-table 或 PSCI 修订。固定版 `spin_table_update_dt` 先确认各 CPU 节点已经采用 spin-table，再写同一个共享释放单元地址及等待区保留记录，不负责把 PSCI 节点转换成 spin-table。共享状态的读取和释放协议见 [等待核与内存保留](P06_ARM入口与异常向量实现.md#6.4_等待核怎样消费共享释放地址)。

## 11.4\_M4与M5\_准备跳转和返回不是同一个结果

`boot/bootm_os.c` 的配置表把 Linux 类型映射到架构 `do_bootm_linux`；未编入相关处理器时没有可用回调。ARM 实现明确区分 PREP 与 GO。

```c
/**
 * @brief 仓库阅读说明：不同 flag 调用同一个 Linux 处理器的不同阶段。
 */
int do_bootm_linux(int flag, int argc, char *const argv[],
                   bootm_headers_t *images)
{
    if (flag & BOOTM_STATE_OS_BD_T || flag & BOOTM_STATE_OS_CMDLINE)
        return -1;
    if (flag & BOOTM_STATE_OS_PREP) {
        boot_prep_linux(images);
        return 0;
    }
    if (flag & (BOOTM_STATE_OS_GO | BOOTM_STATE_OS_FAKE_GO)) {
        boot_jump_linux(images, flag);
        return 0;
    }
    boot_prep_linux(images);
    boot_jump_linux(images, flag);
    return 0;
}
```

参数准备和最终跳转是分开的调用，不是这个函数每次都完整执行两者。`boot_jump_linux` 的 ARM64 分支先调用 `announce_and_cleanup(fake)`；其中打印提示、移除配置适用的设备，再执行 `cleanup_before_linux`。即使 fake 非零，这段清理也会运行；只有其后的实际执行状态交接被 `if (!fake)` 跳过。fake 因此不是无副作用的“只读预览”。

正常 ARM64 路径按配置准备 PSCI，调用 `do_nonsec_virt_switch` 唤醒次级核并处理数据缓存，再更新次级核架构信息，最后通过 EL 切换帮助器进入内核或 `switch_to_el1`。传给 ARM64 内核的是最终 DTB 地址和三个零。帮助器读取当前 EL，不保证每次都有实际 EL3→EL2 转换；32 位内核分支的参数与执行状态另有处理。

默认 ARMv8 `cleanup_before_linux` 调板清理、关闭中断、关闭并失效 I-cache，再关闭 D-cache 并失效；其缓存实现的效果和板覆盖都必须一起核对。`start.S` 的弱 `smp_kick_all_cpus` 在相应 GIC 配置下发送 SGI 0，否则返回；平台可覆盖它，不能仅凭调用名就断言所有次级核已经开始执行 Linux。

最后追踪 `boot_selected_os`，才能理解为什么“回调返回 0”仍可能要求复位。

```c
/**
 * @brief 仓库阅读说明：正常 OS 交接不应返回，允许返回的类型单独列出。
 */
int boot_selected_os(int argc, char *const argv[], int state,
                     bootm_headers_t *images, boot_os_fn *boot_fn)
{
    arch_preboot_os();
    board_preboot_os();
    boot_fn(state, argc, argv, images); /* 此处没有采纳回调的整数返回值 */
    if (images->os.type == IH_TYPE_STANDALONE ||
        IS_ENABLED(CONFIG_SANDBOX) || state == BOOTM_STATE_OS_FAKE_GO)
        return 0;
    bootstage_error(BOOTSTAGE_ID_BOOT_OS_RETURNED);
    /* 裁剪调试输出。 */
    return BOOTM_ERR_RESET;
}
```

普通目标上的 Linux GO 返回会走 `BOOTM_ERR_RESET`，`do_bootm_states` 的错误出口调用 `do_reset`。恢复中断标志也不等于把已停止设备、已覆盖内存和所有钩子副作用回滚。因此失败后是否可重试要看发生阶段和实际副作用，不能承诺所有错误都能回到完整可用的提示符。
