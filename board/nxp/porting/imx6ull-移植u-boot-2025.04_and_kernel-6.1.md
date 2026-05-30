> [!NOTE] 
>
> 本文档的网络参考网址：https://blog.csdn.net/charlie114514191/article/details/147116372。

[TOC]

# 阅读人群

* 必须要会移植一款芯片的uboot和kernel的读者，如果一开始新手来阅读本文档，会容易对一些步骤忽略其关联性，造成阅读壁垒。
* 这篇博客不支持小白阅读，因为笔者没有加入过多的分析说明。大多数情况下需要读者通过参考网页获得具体细节。但是关键步骤均已给出。

# 前言

本博客仅仅针对正点原子imx6ull阿尔法开发板核心板v2.2，网口phy芯片为LAN8720A。uboot版本为nxp官方2025.04if版本。具体细节请看详情。

# 参考链接：

* https://blog.csdn.net/ZHONGCAI0901/article/details/118310802
* https://blog.csdn.net/charlie114514191/article/details/147116372
* 【正点原子【第三期】手把手教你学Linux之系统移植和根文件系统构建篇】https://www.bilibili.com/video/BV12E411h71h?p=24&vd_source=b387713a15d6517575ab4761525174e7

# 环境说明

系统：ubuntu22.04

宿主机架构：intel i9（x64架构）

开发板：正点原子imx6ull阿尔法核心板，EMMC。

# 虚拟机 NFS 和 tftp 配置

虚拟机ubuntu22中配置nfs和tftp，参考[视频](【正点原子【第二期】手把手教你学Linux之ARM（MX6U）裸机篇】https://www.bilibili.com/video/BV1yE411h7uQ?vd_source=b387713a15d6517575ab4761525174e7)。

<iframe 
  src="https://player.bilibili.com/player.html?bvid=BV1yE411h7uQ&autoplay=0" 
  frameborder="0" 
  allowfullscreen 
  style="width: 100%; height: 60vh;">
</iframe>

# uboot和kernel获取

nxp官方源码获取：https://github.com/nxp-imx。在这个网址有很多仓库，注意查看uboot和kernel的仓库。

# 编译器下载

在uboot的根目录下 doc/build/gcc.rst里面有关于gcc版本的要求：

```rst
Building with GCC
=================

Dependencies
------------

For building U-Boot you need a GCC compiler for your host platform. If you
are not building on the target platform you further need  a GCC cross compiler.

Debian based
~~~~~~~~~~~~

On Debian based systems the cross compiler packages are named
gcc-<architecture>-linux-gnu.

You could install GCC and the GCC cross compiler for the ARMv8 architecture with

.. code-block:: bash

    sudo apt-get install gcc gcc-aarch64-linux-gnu

Depending on the build targets further packages maybe needed

.. code-block:: bash

    sudo apt-get install bc bison build-essential coccinelle \
      device-tree-compiler dfu-util efitools flex gdisk graphviz imagemagick \
      liblz4-tool libgnutls28-dev libguestfs-tools libncurses-dev \
      libpython3-dev libsdl2-dev libssl-dev lz4 lzma lzma-alone openssl \
      pkg-config python3 python3-asteval python3-coverage python3-filelock \
      python3-pkg-resources python3-pycryptodome python3-pyelftools \
      python3-pytest python3-pytest-xdist python3-sphinxcontrib.apidoc \
      python3-sphinx-rtd-theme python3-subunit python3-testtools \
      python3-virtualenv swig uuid-dev

SUSE based
~~~~~~~~~~

On suse based systems the cross compiler packages are named
cross-<architecture>-gcc<version>.

You could install GCC and the GCC 10 cross compiler for the ARMv8 architecture
with
/// 这里要求GCC的版本是10.虽然imx6ull是armv7-a架构，但是都是ok的。
```

交叉编译器下载网址：https://developer.arm.com/downloads/-/gnu-a。下载“[gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf.tar.xz](https://developer.arm.com/-/media/Files/downloads/gnu-a/10.3-2021.07/binrel/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf.tar.xz?rev=302e8e98351048d18b6f5b45d472f406&hash=B981F1567677321994BE1231441CB60C7274BB3D)”

将它传递给ubuntu。然后放到读者想放入的位置。一般是放置在 `/usr/local/cross_compiler/arm/` 下，这个目录是所以笔者自己定义的目录，所以读者可以把它放在读者想放入的位置都行。然后再/etc/profile里面将它添加到环境变量里面，重启ubuntu。

```shell
# /etc/profile: system-wide .profile file for the Bourne shell (sh(1))
# and Bourne compatible shells (bash(1), ksh(1), ash(1), ...).

if [ "${PS1-}" ]; then
  if [ "${BASH-}" ] && [ "$BASH" != "/bin/sh" ]; then
    # The file bash.bashrc already sets the default PS1.
    # PS1='\h:\w\$ '
    if [ -f /etc/bash.bashrc ]; then
      . /etc/bash.bashrc
    fi
  else
    if [ "$(id -u)" -eq 0 ]; then
      PS1='# '
    else
      PS1='$ '
    fi
  fi
fi

if [ -d /etc/profile.d ]; then
  for i in /etc/profile.d/*.sh; do
    if [ -r $i ]; then
      . $i
    fi
  done
  unset i
fi

# 添加下面第一行的配置为系统添加交叉编译器
export PATH=$PATH:/usr/local/cross_compiler/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin
export PATH=$PATH:/usr/local/cross_compiler/arm64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin
```

记得添加完成后重启ubuntu让配置文件生效。

# uboot移植

1. 获取nxp官方uboot：

   ```shell
   git clone https://github.com/nxp-imx/uboot-imx.git
   ```

2. 切换分支到2025.04版本

   ```shell
   git checkout origin/lf_v2025.04
   ```

3. uboot的文件夹说明

   | 名称                | 用途说明                                               |
   | ------------------- | ------------------------------------------------------ |
   | api                 | 提供供外部模块调用的公共API接口定义与实现              |
   | arch                | 与架构相关的代码，如ARM、x86等，包含启动代码、头文件等 |
   | board               | 各种开发板相关的配置与初始化代码                       |
   | boot                | 启动加载相关的功能代码，例如启动映像加载器             |
   | cmd                 | 实现U-Boot命令行接口支持的各类命令                     |
   | common              | U-Boot核心功能的通用代码                               |
   | config.mk           | 编译过程的配置文件之一，定义变量和规则                 |
   | configs             | 针对不同开发板的默认配置文件目录                       |
   | disk                | 与磁盘驱动及分区处理相关的模块                         |
   | doc                 | 文档目录，包含U-Boot的使用、开发文档与说明             |
   | drivers             | 设备驱动目录，包含各种硬件设备支持代码                 |
   | dts                 | 设备树源文件目录，用于描述硬件资源                     |
   | env                 | 环境变量处理模块，负责U-Boot环境配置的保存与恢复       |
   | examples            | 提供示例代码和参考实现                                 |
   | fs                  | 文件系统支持模块，如FAT、EXT4等                        |
   | generated_defconfig | 由配置生成的默认配置文件                               |
   | include             | 各类头文件目录，供全局使用                             |
   | Kbuild              | 控制构建系统行为的文件之一                             |
   | Kconfig             | 配置项定义文件，供menuconfig等工具使用                 |
   | lib                 | 提供常用函数的通用库模块                               |
   | MAINTAINERS         | 维护者信息，标明各模块负责人                           |
   | Makefile            | 项目的主构建脚本，定义整体编译规则                     |
   | net                 | 网络协议栈支持代码，如以太网驱动、TCP/IP协议等         |
   | post                | 电源自检（POST）相关代码                               |
   | README              | 项目总览与基础说明文档                                 |
   | scripts             | 编译或配置过程中调用的脚本集合                         |
   | spl                 | Secondary Program Loader，最小引导加载器代码           |
   | System.map          | 映像中各符号与地址的映射表                             |
   | test                | 各类测试代码与测试框架                                 |
   | tools               | 工具代码，如镜像制作、打包工具等                       |
   | u-boot              | 编译生成的主U-Boot ELF可执行文件                       |
   | u-boot.bin          | 编译生成的U-Boot二进制镜像                             |
   | u-boot.cfg          | 编译生成的配置选项摘要文件                             |
   | u-boot.dtb          | 编译生成的设备树二进制文件                             |
   | u-boot-dtb.bin      | 将设备树与U-Boot合并的镜像文件                         |
   | u-boot-dtb.cfgout   | 设备树合并后的配置摘要                                 |
   | u-boot-dtb.imx      | 针对i.MX平台生成的U-Boot镜像文件                       |
   | u-boot-dtb.imx.log  | 生成上述镜像时的日志输出                               |
   | u-boot.lds          | U-Boot链接脚本，定义内存布局                           |
   | u-boot.map          | 编译后符号映射文件                                     |
   | u-boot-nodtb.bin    | 不包含设备树的纯U-Boot镜像                             |
   | u-boot.srec         | SREC格式的U-Boot映像，用于某些烧录工具                 |
   | u-boot.sym          | 含调试符号的U-Boot符号表                               |

4. 修改uboot的顶层Makefile文件，在下面添加对应的架构和交叉编译器的配置

   ```makefile
   # SPDX-License-Identifier: GPL-2.0+
   
   VERSION = 2025
   PATCHLEVEL = 04
   SUBLEVEL =
   EXTRAVERSION =
   NAME =
   
   # 添加下面两行代码，用于确定芯片架构和交叉编译器的。
   ARCH ?=arm		
   CROSS_COMPILE ?= arm-none-linux-gnueabihf-
   ```

   修改后的配置：

   ![image](../../images/uboot/nxp/root_makefile_change.png)

## 添加开发板对应的xxx_defconfig文件

1. 将官方的defconfig拷贝出来重命名，笔者需要在副本上进行一定的修改

   ```shell
   cd configs
   cp mx6ull_14x14_evk_emmc_defconfig mx6ull_test_emmc_defconfig
   ```

2. 修改 `mx6ull_test_emmc_defconfig` 文件的一些内容

   ```makefile
   ...
   # 添加自己定义的开发板配置
   # CONFIG_TARGET_MX6ULL_14X14_EVK=y  修改为
   CONFIG_TARGET_MX6ULL_TEST_EMMC=y
   ...
   # 添加自己定义的开发板的设备树
   # CONFIG_DEFAULT_DEVICE_TREE="imx6ull-14x14-evk-emmc"  修改为
   CONFIG_DEFAULT_DEVICE_TREE="imx6ull-14x14-test-emmc"
   ...
   ```

   修改后的配置为：

   ![image](../../images/uboot/nxp/xxx_defconfig_change.png)

   笔者来对比下2025年和2016年的xxx_defconfig的区别，下面图片展示的是mx6ull_14x14_evk_emmc_defconfig文件的对比详情：
   
   ![image](../../images/uboot/nxp/xxx_defconfig_compire.png)

   经过对比，发现2025里面多了很多宏定义，这些宏定义从哪里来的呢？是将2016年里面针对board的配置宏从 `mx6ullevk.h` 或者 `mx6ull_14x14_evk_emmc.h`转移到了xxx_defconfig里面来定义了。因此，下面讲解 `mx6ullevk.h` 或者 `mx6ull_14x14_evk_emmc.h` 的对比图，发现2016年里面的很多宏在2025年的头文件里都不见了。 

## 添加开发板对应的头文件

在 目 录 include/configs 下 添 加 开 发 板 对 应 的 头 文 件 ， 复 制include/configs/mx6ullevk.h，并重命名为 mx6ull_test_emmc.h，命令如下：

```shell
cp include/configs/mx6ullevk.h include/configs/mx6ull_test_emmc.h
```

拷贝完成以后，更改一些宏：

```c
/*
#ifndef __MX6ULLEVK_CONFIG_H
#define __MX6ULLEVK_CONFIG_H
*/

// 更改为
#ifndef __MX6ULL_TEST_EMMC_CONFIG_H
#define __MX6ULL_TEST_EMMC_CONFIG_H
```

2025与2016的对比 `include/configs/mx6ullevk.h` 变化：

![image](../../images/uboot/nxp/mx6ullevk.h_compire.png)

* 2016年的很多宏定义在2025年都将由xxx_defconfig来配置。

* 针对ddr的大小获取不再通过宏 `PHYS_SDRAM_SIZE` 定义来获取，而是通过动态方法来获取。

  ```c
  // board/freescale/mx6ullevk/mx6ullevk.c
  int dram_init(void)
  {
  	gd->ram_size = imx_ddr_size();
  
  	return 0;
  }
  ```

  * imx_ddr_size()函数定义位置：arch/arm/mach-imx/mmdc_size.c

    ```c
    /*
     * imx_ddr_size - return size in bytes of DRAM according MMDC config
     * The MMDC MDCTL register holds the number of bits for row, col, and data
     * width and the MMDC MDMISC register holds the number of banks. Combine
     * all these bits to determine the meme size the MMDC has been configured for
     */
    unsigned int imx_ddr_size(void)
    {
    	struct esd_mmdc_regs *mem = (struct esd_mmdc_regs *)MEMCTL_BASE;
    	unsigned int ctl = readl(&mem->ctl);
    	unsigned int misc = readl(&mem->misc);
    	int bits = 11 + 0 + 0 + 1;      /* row + col + bank + width */
    
    	bits += ESD_MMDC_CTL_GET_ROW(ctl);
    	bits += col_lookup[ESD_MMDC_CTL_GET_COLUMN(ctl)];
    	bits += bank_lookup[ESD_MMDC_MISC_GET_BANK(misc)];
    	bits += ESD_MMDC_CTL_GET_WIDTH(ctl);
    	bits += ESD_MMDC_CTL_GET_CS1(ctl);
    
    	/* The MX6 can do only 3840 MiB of DRAM */
    	if (bits == 32)
    		return 0xf0000000;
    
    	return 1 << bits;
    }
    ```

* `CONFIG_SYS_MALLOC_LEN` 宏在xxx_defconfig文件里面定义，大小为：0x1000000 = （16 * 1024 * 1024）也就是16MB。

* `CONFIG_BOARD_EARLY_INIT_F` 宏在xxx_defconfig文件里面定义，值为Y。这样 board_init_f 函数就会调用board_early_init_f 函数。

* `CONFIG_BOARD_LATE_INIT` 宏取消，整合进入标准board_init流程。

* 2025年采用 `CFG_MXC_UART_BASE` 替换 2016 年 `CONFIG_MXC_UART_BASE`，表示串口寄存器基地址，这里使用的串口1，基地址为UART1_BASE（定义在arch/arm/include/asm/arch-mx6/imx-regs.h）文件中。

* 2025年采用 `CFG_SYS_FSL_ESDHC_ADDR` 替换 2016年 `CONFIG_SYS_FSL_ESDHC_ADDR`，表示EMMC所使用接口的寄存器基地址，也就是USDHC2的基地址。

* 2025年采用 `CFG_EXTRA_ENV_SETTINGS` 替换 2016年 `CONFIG_EXTRA_ENV_SETTINGS`，此宏会设置 bootargs 这个环境变量。

* 2025年将不在头文件中直接定义`CONFIG_SYS_LOAD_ADDR`，而是在 `Kconfig` 直接定义了对应芯片的型号值，该值表示linux kernel在DRAM中的加载地址，也就是linux kernel在DRAM中的存储首地址：

  ```Kconfig
  # /Kconfig
  
  config SYS_LOAD_ADDR
  	hex "Address in memory to use by default"
  	default 0x01000000 if ARCH_SOCFPGA
  	default 0x02000000 if PPC || X86
  	default 0x81000000 if MACH_SUNIV
  	default 0x22000000 if MACH_SUN9I
  	default 0x42000000 if ARCH_SUNXI
  	default 0x82000000 if ARCH_KEYSTONE || ARCH_OMAP2PLUS || ARCH_K3
  	default 0x80800000 if ARCH_MX6 && (MX6SL || MX6SLL  || MX6SX || MX6UL || MX6ULL)
  	default 0x12000000 if ARCH_MX6 && !(MX6SL || MX6SLL  || MX6SX || MX6UL || MX6ULL)
  	default 0x80800000 if ARCH_MX7
  	default 0x90000000 if FSL_LSCH2 || FSL_LSCH3
  	default 0x0 if ARCH_SC5XX
  	help
  	  Address in memory to use as the default safe load address.
  ```

  但是笔者在2016年的“mx6ull_14x14_evk_emmc.h”中依然看得到它的定义。

* 2025年将不在头文件直接定义 `CONFIG_SYS_HZ` ，而是在 `lib/Kconfig` 直接定义了系统时钟频率，定义如下：

  ```Kconfig
  config SYS_HZ
  	int
  	default 1000
  	help
  	  The frequency of the timer returned by get_timer().
  	  get_timer() must operate in milliseconds and this option must be
  	  set to 1000.
  ```

  

* 2025年中为何没有`CONFIG_STACKSIZE` 宏，被什么替换了？

  * `CONFIG_STACKSIZE` 这个宏在新版 U-Boot 已经逐步被 Kconfig 体系下的栈相关配置（如 `CONFIG_SPL_STACK`、`CONFIG_SPL_STACK_R` 等）取代。主线 U-Boot 通常不再在 defconfig 或头文件中直接定义 `CONFIG_STACKSIZE`，而是通过 Kconfig 自动管理栈的分配和大小。

  * 简而言之：  

    - 普通 U-Boot 阶段的栈大小现在由架构相关代码和 Kconfig 体系自动分配。

    - SPL 阶段的栈用 `CONFIG_SPL_STACK` 等 Kconfig 选项配置。

  * 如需自定义栈大小，请查阅对应平台的 Kconfig 选项。

  ```Kconfig
  # /common/spl/Kconfig
  
  config SPL_SYS_MALLOC_SIMPLE
  	bool "Only use malloc_simple functions in the SPL"
  	help
  	  Say Y here to only use the *_simple malloc functions from
  	  malloc_simple.c, rather then using the versions from dlmalloc.c;
  	  this will make the SPL binary smaller at the cost of more heap
  	  usage as the *_simple malloc functions do not re-use free-ed mem.
  
  config SPL_SHARES_INIT_SP_ADDR
  	bool "SPL and U-Boot use the same initial stack pointer location"
  	depends on (ARM || ARCH_JZ47XX || MICROBLAZE || RISCV) && SPL_FRAMEWORK
  	default n if ARCH_SUNXI || ARCH_MX6 || ARCH_MX7 || ARCH_SC5XX
  	default y
  	help
  	  In many cases, we can use the same initial stack pointer address for
  	  both SPL and U-Boot itself.  If you need to specify a different address
  	  however, say N here and then set a different value in CONFIG_SPL_STACK.
  
  config SPL_STACK
  	hex "Initial stack pointer location"
  	depends on (ARM || ARCH_JZ47XX || MICROBLAZE || RISCV) && \
  		SPL_FRAMEWORK || ROCKCHIP_RK3036
  	depends on !SPL_SHARES_INIT_SP_ADDR
  	default 0x946bb8 if ARCH_MX7
  	default 0x93ffb8 if ARCH_MX6 && MX6_OCRAM_256KB
  	default 0x91ffb8 if ARCH_MX6 && !MX6_OCRAM_256KB
  	default 0x118000 if MACH_SUN50I_H6
  	default 0x52a00 if MACH_SUN50I_H616
  	default 0x40000 if MACH_SUN8I_R528
  	default 0x54000 if MACH_SUN50I || MACH_SUN50I_H5
  	default 0x18000 if MACH_SUN9I
  	default 0x8000 if ARCH_SUNXI
  	default 0x200E4000 if ARCH_SC5XX && (SC59X_64 || SC59X)
  	default 0x200B0000 if ARCH_SC5XX && SC58X
  	default 0x200D0000 if ARCH_SC5XX && SC57X
  	help
  	  Address of the start of the stack SPL will use before SDRAM is
  	  initialized.
  
  config SPL_STACK_R
  	bool "Enable SDRAM location for SPL stack"
  	help
  	  SPL starts off execution in SRAM and thus typically has only a small
  	  stack available. Since SPL sets up DRAM while in its board_init_f()
  	  function, it is possible for the stack to move there before
  	  board_init_r() is reached. This option enables a special SDRAM
  	  location for the SPL stack. U-Boot SPL switches to this after
  	  board_init_f() completes, and before board_init_r() starts.
  
  config SPL_STACK_R_ADDR
  	depends on SPL_STACK_R
  	hex "SDRAM location for SPL stack"
  	default 0x82000000 if ARCH_OMAP2PLUS
  	help
  	  Specify the address in SDRAM for the SPL stack. This will be set up
  	  before board_init_r() is called.
  
  config SPL_STACK_R_MALLOC_SIMPLE_LEN
  	depends on SPL_STACK_R && SPL_SYS_MALLOC_SIMPLE
  	hex "Size of malloc_simple heap after switching to DRAM SPL stack"
  	default 0x400000 if ARCH_K3 && ARM64
  	default 0x200000 if ARCH_K3 && CPU_V7R
  	default 0x100000
  	help
  	  Specify the amount of the stack to use as memory pool for
  	  malloc_simple after switching the stack to DRAM. This may be set
  	  to give board_init_r() a larger heap then the initial heap in
  	  SRAM which is limited to SYS_MALLOC_F_LEN bytes.
  
  config SPL_SEPARATE_BSS
  	bool "BSS section is in a different memory region from text"
  	help
  	  Some platforms need a large BSS region in SPL and can provide this
  	  because RAM is already set up. In this case BSS can be moved to RAM.
  	  This option should then be enabled so that the correct device tree
  	  location is used. Normally we put the device tree at the end of BSS
  	  but with this option enabled, it goes at _image_binary_end.
  
  config SPL_SYS_MALLOC
  	bool "Enable malloc pool in SPL"
  	depends on SPL_FRAMEWORK
  
  config SPL_HAS_CUSTOM_MALLOC_START
  	bool "For the SPL malloc pool, define a custom starting address"
  	depends on SPL_SYS_MALLOC
  
  config SPL_CUSTOM_SYS_MALLOC_ADDR
  	hex "SPL malloc addr"
  	depends on SPL_HAS_CUSTOM_MALLOC_START
  
  config SPL_SYS_MALLOC_SIZE
  	hex "Size of the SPL malloc pool"
  	depends on SPL_SYS_MALLOC
  	default 0x180000 if BIOSEMU && RISCV
  	default 0x100000
  ```

* 2025年将不在头文件直接定义 `CONFIG_NR_DRAM_BANKS` ，而是在 `Kconfig` 中默认配置为了4，定义如下所示：

  ```Kconfig
  # /Kconfig
  
  config NR_DRAM_BANKS
  	int "Number of DRAM banks"
  	default 1 if ARCH_SC5XX || ARCH_SUNXI || ARCH_OWL
  	default 2 if OMAP34XX
  	default 4
  	help
  	  This defines the number of DRAM banks.
  ```

  这和2016年的有所不同，因为2016年的配置是在头文件中定义为1，表示使用了1个DRAM BANK。那么为什么呢？

  > imx6ull 硬件上通常只支持一个 DRAM bank，2016 年的 U-Boot 头文件里确实是手动 `#define CONFIG_NR_DRAM_BANKS 1`。  
  >
  > 但在新版 U-Boot（Kconfig 体系）中，`CONFIG_NR_DRAM_BANKS` 默认值是 4，只有特定架构（如 ARCH_SC5XX、ARCH_SUNXI、ARCH_OWL）才自动设为 1。
  >
  > 这不是硬件变动，而是 U-Boot 配置体系的变化。新版 U-Boot 统一用 Kconfig 默认 4 个 bank，实际运行时只会初始化和使用硬件支持的 bank 数量，多余的不影响功能。如果读者想和老版本一样严格限制为 1，可以在 defconfig 里手动加上 `CONFIG_NR_DRAM_BANKS=1`。

* `PHYS_SDRAM` 宏为I.MX6ULL的DRAM控制器MMDC0所管辖的DRAM范围起始地址，也就是0X8000'0000。

* `CONFIG_SYS_SDRAM_BASE` 宏变更为 `CFG_SYS_SDRAM_BASE`，为 DRAM 的起始地址。

* `CONFIG_SYS_INIT_RAM_ADDR` 宏变更为 `CFG_SYS_INIT_RAM_ADDR`，值 `IRAM_BASE_ADDR` 在 `arch/arm/include/asm/arch-mx6/imx-regs.h` 下，定义为0X0090'0000。

* `CONFIG_SYS_INIT_RAM_SIZE`宏变更为 `CFG_SYS_INIT_RAM_SIZE`，值 `IRAM_SIZE`在`arch/arm/include/asm/arch-mx6/imx-regs.h` 下，定义为0x0004'0000=128KB。

* `CFG_SYS_INIT_SP_OFFSET` 和 `和 CONFIG_SYS_INIT_SP_ADDR` 宏在哪里定义？

  * `CFG_SYS_INIT_SP_OFFSET` 这个宏在读者的 mx6ullevk.h 文件和相关头文件中没有直接定义。 新版 U-Boot 通常只定义 `CFG_SYS_INIT_RAM_ADDR` 和 `CFG_SYS_INIT_RAM_SIZE`，而 `CFG_SYS_INIT_SP_OFFSET` 的计算（如 `CFG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE`）一般在启动代码或链接脚本中实现，而不是通过头文件的宏。

    如果需要，可以在自己的头文件中手动添加：

    ```c
    #define CFG_SYS_INIT_SP_OFFSET (CFG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
    ```

  * 但主线 U-Boot 越来越多地通过自动生成和 Kconfig 体系管理这些偏移量。`CONFIG_SYS_INIT_SP_ADDR` 是通过 Kconfig 体系自动生成的，用于指定 U-Boot 启动时初始栈指针（SP）的地址。 其值通常由平台相关的 Kconfig 或头文件（如 `CFG_SYS_INIT_RAM_ADDR` + 偏移量）决定。

    在 Kconfig 体系下，如果读者的平台需要自定义初始 SP 地址，可以通过 `HAS_CUSTOM_SYS_INIT_SP_ADDR` 和 `CUSTOM_SYS_INIT_SP_ADDR` 相关配置来指定。否则，U-Boot 会根据默认规则自动计算和分配 `CONFIG_SYS_INIT_SP_ADDR`。

* `CONFIG_SYS_MMC_ENV_DEV` 宏在2025年中不在头文件定义，是在 `xxx_defconfig` 直接指定的，这里默认为USDHC2，也就是 EMMC，值为1。

* `CONFIG_SYS_MMC_ENV_PART`宏在2025年中不在头文件中定义，是在 `Kconfig` 文件中配置的，定义如下：

  ```Kconfig
  # /env/Kconfig
  config SYS_MMC_ENV_PART
  	int "mmc partition number"
  	depends on ENV_IS_IN_MMC || ENV_IS_IN_FAT
  	default 0
  	help
  	  MMC hardware partition device number on the platform where the
  	  environment is stored.  Note that this is not related to any software
  	  defined partition table but instead if we are in the user area, which is
  	  partition 0 or the first boot partition, which is 1 or some other defined
  	  partition.
  	  
  ...
  config ENV_IS_IN_MMC
  	bool "Environment in an MMC device"
  	depends on !CHAIN_OF_TRUST
  	depends on MMC
  	default y if ARCH_EXYNOS4
  	default y if MX6SX || MX7D
  	default y if TEGRA30 || TEGRA124
  	default y if TEGRA_ARMV8_COMMON
  	help
  	  Define this if you have an MMC device which you want to use for the
  	  environment.
  
  	  CONFIG_SYS_MMC_ENV_DEV:
  
  	  Specifies which MMC device the environment is stored in.
  
  	  CONFIG_SYS_MMC_ENV_PART (optional):
  
  /*************************************************************************
   * 	这里说明了默认值为0，当没有设定值的时候，目前xxx_defconfig没有配置值。
   *************************************************************************/
  	  Specifies which MMC partition the environment is stored in. If not
  	  set, defaults to partition 0, the user area. Common values might be
  	  1 (first MMC boot partition), 2 (second MMC boot partition).
  
  	  CONFIG_ENV_OFFSET:
  	  CONFIG_ENV_SIZE:
  
  	  These two #defines specify the offset and size of the environment
  	  area within the specified MMC device.
  
  	  If offset is positive (the usual case), it is treated as relative to
  	  the start of the MMC partition. If offset is negative, it is treated
  	  as relative to the end of the MMC partition. This can be useful if
  	  your board may be fitted with different MMC devices, which have
  	  different sizes for the MMC partitions, and you always want the
  	  environment placed at the very end of the partition, to leave the
  	  maximum possible space before it, to store other data.
  
  	  These two values are in units of bytes, but must be aligned to an
  	  MMC sector boundary.
  
  	  CONFIG_ENV_OFFSET_REDUND (optional):
  
  	  Specifies a second storage area, of CONFIG_ENV_SIZE size, used to
  	  hold a redundant copy of the environment data. This provides a
  	  valid backup copy in case the other copy is corrupted, e.g. due
  	  to a power failure during a "saveenv" operation.
  
  	  This value may also be positive or negative; this is handled in the
  	  same way as CONFIG_ENV_OFFSET.
  
  	  In case CONFIG_SYS_MMC_ENV_PART is 1 (i.e. environment in eMMC boot
  	  partition) then setting CONFIG_ENV_OFFSET_REDUND to the same value
  	  as CONFIG_ENV_OFFSET makes use of the second eMMC boot partition for
  	  the redundant environment copy.
  
  	  This value is also in units of bytes, but must also be aligned to
  	  an MMC sector boundary.
  
  	  CONFIG_ENV_MMC_USE_DT (optional):
  
  	  These define forces the configuration by the config node in device
  	  tree with partition name: "u-boot,mmc-env-partition" or with
  	  offset: "u-boot,mmc-env-offset", "u-boot,mmc-env-offset-redundant".
  	  CONFIG_ENV_OFFSET and CONFIG_ENV_OFFSET_REDUND are not used.
  ```

* `CONFIG_ENV_SIZE` 宏在2025年不在头文件中定义，而是在xxx_defconfig指定，为环境变量大小，值为 0x2000 = 8*1024，默认为8KB。

* `CONFIG_ENV_OFFSET` 宏在2025年不在头文件中定义，而是在xxx_defconfig 指定，为环境变量偏移地址，这里的偏移地址是相对于存储器的首地址。在emmc中定义为0xE0000 = 14\*64\*1024.

头文件讲解到此为止。笔者这里是针对正点原子驱动开发指南来排列讲解的，可以针对2016年来理解某些改动和移植的一些选项修改原则。

## 添加开发板对应的板级文件夹

uboot 中每个板子都有一个对应的文件夹来存放板级文件，比如开发板上外设驱动文件等等。NXP 的 I.MX 系列芯片的所有板级文件夹都存放在 board/freescale 目录下，在这个目录下有个名为 mx6ullevk 的文件夹，这个文件夹就是 NXP 官方 I.MX6ULL EVK 开发板的板级文件夹。查看下里面的文件：

```shell
$ cd board/freescale/mx6ullevk
$ ls
built-in.o  imximage.cfg  imximage.cfg.cfgtmp  imximage_lpddr2.cfg  Kconfig  MAINTAINERS  Makefile  mx6ullevk.c  mx6ullevk.o  mx6ullevk.su  plugin.S
```

下面是 mx6ullevk 目录下常见文件的作用说明表：

| 文件名/目录                     | 作用说明                                                     |
| ------------------------------- | ------------------------------------------------------------ |
| mx6ullevk.c                     | 板级初始化代码，负责硬件初始化（如 DDR、IOMUX、外设等），实现 board_init/board_late_init 等函数。 |
| mx6ullevk.h                     | 板级头文件，定义本板相关的宏、引脚配置、外设参数等（有时已迁移到 Kconfig/设备树）。 |
| Makefile                        | 指定本目录下源文件的编译规则，决定哪些文件被编译进 U-Boot。  |
| README                          | 简要说明本板支持情况、编译方法、硬件特性等。                 |
| Kconfig                         | 板级 Kconfig 配置入口，定义本板可选的配置项。                |
| <board>.env（如 mx6ullevk.env） | 板级默认环境变量文件（如有），可通过 Kconfig 配置 ENV_SOURCE_FILE 使用。 |
| <board>_spl.c                   | SPL 阶段的板级初始化代码（如有，负责最小硬件初始化）。       |
| <board>_u-boot.dtsi             | 板级设备树 include 文件，供主设备树引用（如有）。            |
| .../                            | 可能还有 board-specific 的外设驱动、引脚配置、辅助文件等。   |

说明：
- 具体文件名可能因板子不同而略有差异，但结构类似。
- 主要分为代码实现（.c）、配置（.h/Kconfig/Makefile）、文档（README）、环境变量（.env）、设备树（.dtsi）等几类。
- 板级初始化的核心通常在 `<board>.c` 文件。

### 新增mx6ull_test_emmc文件夹

下面根据文件名排列来进行更改。

复制 mx6ullevk，将其重命名为mx6ull_test_emmc，命令如下：

```shell
cd board/freescale/
cp mx6ullevk/ -r mx6ull_test_emmc
```

### .built-in.o.cmd修改

将mx6ull_test_emmc 目录下.built-in.o.cmd的内容修改为如下内容：

```makefile
# 只是将内容中的mx6ullevk替换为了mx6ull_test_emmc.
cmd_board/freescale/mx6ull_test_emmc/built-in.o :=  rm -f board/freescale/mx6ull_test_emmc/built-in.o; arm-none-linux-gnueabihf-ar cDPrsT board/freescale/mx6ull_test_emmc/built-in.o board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.o
```

### .mx6ullevk.o.cmd修改

将mx6ull_test_emmc目录下.mx6ullevk.o.cmd的内容修改为如下内容：

```makefile
# 只是将内容中的mx6ullevk替换为了mx6ull_test_emmc.
cmd_board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.o := arm-none-linux-gnueabihf-gcc -Wp,-MD,board/freescale/mx6ull_test_emmc/.mx6ull_test_emmc.o.d -nostdinc -isystem /usr/local/cross_compiler/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/../lib/gcc/arm-none-linux-gnueabihf/10.3.1/include -Iinclude      -I./arch/arm/include -include ./include/linux/kconfig.h -I./dts/upstream/include  -D__KERNEL__ -D__UBOOT__ -Wall -Wstrict-prototypes -Wno-format-security -fno-builtin -ffreestanding -std=gnu11 -fshort-wchar -fno-strict-aliasing -fno-PIE -Os -fno-stack-protector -fno-delete-null-pointer-checks -Wno-pointer-sign -Wno-stringop-truncation -Wno-zero-length-bounds -Wno-array-bounds -Wno-stringop-overflow -Wno-maybe-uninitialized -fmacro-prefix-map=./= -gdwarf-4 -fstack-usage -Wno-format-nonliteral -Wno-address-of-packed-member -Wno-unused-but-set-variable -Werror=date-time -Wno-packed-not-aligned -D__ARM__ -Wa,-mimplicit-it=always -mthumb -mthumb-interwork -mabi=aapcs-linux -mword-relocations -fno-pic -mno-unaligned-access -ffunction-sections -fdata-sections -fno-common -ffixed-r9 -msoft-float -mgeneral-regs-only -pipe -march=armv7-a -D__LINUX_ARM_ARCH__=7 -mtune=generic-armv7-a -I./arch/arm/mach-imx/include    -DKBUILD_BASENAME='"mx6ull_test_emmc"'  -DKBUILD_MODNAME='"mx6ull_test_emmc"' -c -o board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.o board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.c

source_board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.o := board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.c

deps_board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.o := \
    $(wildcard include/config/dm/pmic.h) \
    $(wildcard include/config/ldo/bypass/check.h) \
    $(wildcard include/config/fsl/qspi.h) \
    $(wildcard include/config/dm/spi.h) \
    $(wildcard include/config/nand/mxs.h) \
    $(wildcard include/config/fec/mxc.h) \
    $(wildcard include/config/video.h) \
    $(wildcard include/config/cmd/bmode.h) \
    $(wildcard include/config/imx/optee.h) \
    $(wildcard include/config/env/vars/uboot/runtime/config.h) \
    $(wildcard include/config/env/is/in/mmc.h) \
    $(wildcard include/config/video/mxs.h) \
  include/linux/kconfig.h \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
    $(wildcard include/config/spl/.h) \
    $(wildcard include/config/tpl/.h) \
    $(wildcard include/config/tools/.h) \
    $(wildcard include/config/tpl/build.h) \
    $(wildcard include/config/vpl/build.h) \
    $(wildcard include/config/spl/build.h) \
    $(wildcard include/config/tools/foo.h) \
    $(wildcard include/config/xpl/build.h) \
    $(wildcard include/config/spl/foo.h) \
    $(wildcard include/config/tpl/foo.h) \
    $(wildcard include/config/vpl/foo.h) \
    $(wildcard include/config/option.h) \
    $(wildcard include/config/acme.h) \
    $(wildcard include/config/spl/acme.h) \
    $(wildcard include/config/tpl/acme.h) \
    $(wildcard include/config/if/enabled/int.h) \
    $(wildcard include/config/int/option.h) \
  include/init.h \
    $(wildcard include/config/efi.h) \
    $(wildcard include/config/nr/dram/banks.h) \
    $(wildcard include/config/save/prev/bl/initramfs/start/addr.h) \
    $(wildcard include/config/save/prev/bl/fdt/addr.h) \
    $(wildcard include/config/cpu.h) \
    $(wildcard include/config/dtb/reselect.h) \
    $(wildcard include/config/android/boot/image.h) \
    $(wildcard include/config/arm.h) \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
  include/linux/posix_types.h \
  include/linux/stddef.h \
  include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/optimize/inlining.h) \
    $(wildcard include/config/cc/has/asm/inline.h) \
  include/linux/compiler_attributes.h \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/retpoline.h) \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  arch/arm/include/asm/posix_types.h \
  arch/arm/include/asm/types.h \
    $(wildcard include/config/arm64.h) \
    $(wildcard include/config/phys/64bit.h) \
    $(wildcard include/config/dma/addr/t/64bit.h) \
  include/asm-generic/int-ll64.h \
  /usr/local/cross_compiler/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/lib/gcc/arm-none-linux-gnueabihf/10.3.1/include/stdbool.h \
  arch/arm/include/asm/global_data.h \
    $(wildcard include/config/fsl/esdhc.h) \
    $(wildcard include/config/fsl/esdhc/imx.h) \
    $(wildcard include/config/acpi.h) \
    $(wildcard include/config/u/qe.h) \
    $(wildcard include/config/at91family.h) \
    $(wildcard include/config/sys/icache/off.h) \
    $(wildcard include/config/sys/dcache/off.h) \
    $(wildcard include/config/resv/ram.h) \
    $(wildcard include/config/arch/omap2plus.h) \
    $(wildcard include/config/fsl/lsch3.h) \
    $(wildcard include/config/sys/fsl/has/dp/ddr.h) \
    $(wildcard include/config/arch/imx8.h) \
    $(wildcard include/config/imx/ele.h) \
    $(wildcard include/config/arch/imx8ulp.h) \
    $(wildcard include/config/smbios.h) \
  include/config.h \
  include/configs/mx6ull_test_emmc.h \
    $(wildcard include/config/target/mx6ull/9x9/evk.h) \
    $(wildcard include/config/fsl/usdhc.h) \
    $(wildcard include/config/sys/fsl/usdhc/num.h) \
    $(wildcard include/config/nand/boot.h) \
    $(wildcard include/config/sys/mmc/env/dev.h) \
  arch/arm/include/asm/arch/imx-regs.h \
    $(wildcard include/config/mx6sl.h) \
    $(wildcard include/config/mx6sx.h) \
    $(wildcard include/config/mx6ul.h) \
    $(wildcard include/config/mx6ull.h) \
    $(wildcard include/config/mx6sll.h) \
    $(wildcard include/config/mx6dl.h) \
  arch/arm/include/asm/mach-imx/regs-lcdif.h \
    $(wildcard include/config/mx28.h) \
    $(wildcard include/config/mx7.h) \
    $(wildcard include/config/mx7ulp.h) \
    $(wildcard include/config/imx8m.h) \
    $(wildcard include/config/imx8.h) \
    $(wildcard include/config/imxrt.h) \
    $(wildcard include/config/mx23.h) \
  arch/arm/include/asm/mach-imx/regs-common.h \
  include/linux/bitops.h \
    $(wildcard include/config/sandbox.h) \
    $(wildcard include/config/sandbox/bits/per/long.h) \
  include/asm-generic/bitsperlong.h \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/stack/validation.h) \
    $(wildcard include/config/kasan.h) \
  include/linux/kernel.h \
  include/linux/printk.h \
    $(wildcard include/config/loglevel.h) \
    $(wildcard include/config/log.h) \
  include/log.h \
    $(wildcard include/config/log/max/level.h) \
    $(wildcard include/config/logf/func.h) \
    $(wildcard include/config/panic/hang.h) \
    $(wildcard include/config/log/error/return.h) \
    $(wildcard include/config/logf/file.h) \
    $(wildcard include/config/logf/line.h) \
  include/stdio.h \
    $(wildcard include/config/serial.h) \
    $(wildcard include/config/console/flush/support.h) \
  /usr/local/cross_compiler/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/lib/gcc/arm-none-linux-gnueabihf/10.3.1/include/stdarg.h \
  include/linker_lists.h \
    $(wildcard include/config/linker/list/align.h) \
  include/dm/uclass-id.h \
  include/linux/list.h \
  include/linux/poison.h \
  include/limits.h \
    $(wildcard include/config/64bit.h) \
    $(wildcard include/config/spl/64bit.h) \
  arch/arm/include/asm/bitops.h \
    $(wildcard include/config/has/thumb2.h) \
    $(wildcard include/config/sys/thumb/build.h) \
  include/asm-generic/bitops/builtin-__fls.h \
  include/asm-generic/bitops/builtin-__ffs.h \
  include/asm-generic/bitops/builtin-fls.h \
  include/asm-generic/bitops/builtin-ffs.h \
  include/asm-generic/bitops/fls64.h \
  arch/arm/include/asm/proc-armv/system.h \
  include/linux/sizes.h \
  include/linux/const.h \
  include/linux/stringify.h \
  include/configs/mx6_common.h \
    $(wildcard include/config/sys/l2cache/off.h) \
  arch/arm/include/asm/mach-imx/gpio.h \
  include/env/nxp/imx_env.h \
    $(wildcard include/config/usb/port/auto.h) \
    $(wildcard include/config/imx8mm.h) \
    $(wildcard include/config/imx8mq.h) \
    $(wildcard include/config/imx8qm.h) \
    $(wildcard include/config/imx8qxp.h) \
    $(wildcard include/config/imx8dxl.h) \
    $(wildcard include/config/imx8mn.h) \
    $(wildcard include/config/imx8mp.h) \
  arch/arm/include/asm/config.h \
    $(wildcard include/config/arch/ls1021a.h) \
    $(wildcard include/config/fsl/layerscape.h) \
  include/linux/kconfig.h \
  include/config_fallbacks.h \
    $(wildcard include/config/spl/pad/to.h) \
    $(wildcard include/config/spl/max/size.h) \
  arch/arm/include/asm/u-boot.h \
  include/asm-generic/u-boot.h \
    $(wildcard include/config/mpc8xx.h) \
    $(wildcard include/config/e500.h) \
    $(wildcard include/config/mpc86xx.h) \
    $(wildcard include/config/m68k.h) \
    $(wildcard include/config/mpc83xx.h) \
    $(wildcard include/config/extra/clock.h) \
  arch/arm/include/asm/u-boot-arm.h \
  include/asm-generic/global_data.h \
    $(wildcard include/config/env/support.h) \
    $(wildcard include/config/post.h) \
    $(wildcard include/config/board/types.h) \
    $(wildcard include/config/pre/console/buffer.h) \
    $(wildcard include/config/dm.h) \
    $(wildcard include/config/of/platdata/driver/rt.h) \
    $(wildcard include/config/of/platdata/rt.h) \
    $(wildcard include/config/timer.h) \
    $(wildcard include/config/of/live.h) \
    $(wildcard include/config/multi/dtb/fit.h) \
    $(wildcard include/config/trace.h) \
    $(wildcard include/config/sys/i2c/legacy.h) \
    $(wildcard include/config/cmd/bdinfo/extra.h) \
    $(wildcard include/config/sys/malloc/f.h) \
    $(wildcard include/config/console/record.h) \
    $(wildcard include/config/bootstage.h) \
    $(wildcard include/config/bloblist.h) \
    $(wildcard include/config/handoff.h) \
    $(wildcard include/config/translation/offset.h) \
    $(wildcard include/config/generate/smbios/table.h) \
    $(wildcard include/config/event.h) \
    $(wildcard include/config/cyclic.h) \
    $(wildcard include/config/upl.h) \
    $(wildcard include/config/event/dynamic.h) \
    $(wildcard include/config/sys/malloc/f/len.h) \
    $(wildcard include/config/trace/buffer/size.h) \
  include/board_f.h \
  include/event_internal.h \
  include/event.h \
    $(wildcard include/config/event/debug.h) \
  include/dm/ofnode_decl.h \
  include/fdtdec.h \
    $(wildcard include/config/fdt/64bit.h) \
    $(wildcard include/config/of/embed.h) \
    $(wildcard include/config/of/board.h) \
  include/linux/libfdt.h \
  include/linux/libfdt_env.h \
  include/linux/string.h \
  arch/arm/include/asm/string.h \
    $(wildcard include/config/use/arch/memcpy.h) \
    $(wildcard include/config/use/arch/memmove.h) \
    $(wildcard include/config/use/arch/memset.h) \
  include/linux/linux_string.h \
  arch/arm/include/asm/byteorder.h \
  include/linux/byteorder/little_endian.h \
  include/linux/byteorder/swab.h \
  include/linux/byteorder/generic.h \
  include/vsprintf.h \
  include/linux/../../scripts/dtc/libfdt/libfdt.h \
  include/linux/../../scripts/dtc/libfdt/libfdt_env.h \
  include/linux/../../scripts/dtc/libfdt/fdt.h \
  include/pci.h \
    $(wildcard include/config/sys/pci/64bit.h) \
    $(wildcard include/config/dm/pci/compat.h) \
    $(wildcard include/config/mpc85xx.h) \
    $(wildcard include/config/pci/sriov.h) \
  include/pci_ids.h \
  include/dm/pci.h \
  include/membuff.h \
  include/linux/build_bug.h \
  include/asm-offsets.h \
  include/generated/generic-asm-offsets.h \
  arch/arm/include/asm/arch/clock.h \
    $(wildcard include/config/sys/mx6/hclk.h) \
    $(wildcard include/config/sys/mx6/clk32.h) \
  arch/arm/include/asm/arch/iomux.h \
  arch/arm/include/asm/arch/crm_regs.h \
  arch/arm/include/asm/arch/mx6-pins.h \
    $(wildcard include/config/mx6qdl.h) \
    $(wildcard include/config/mx6q.h) \
    $(wildcard include/config/mx6qp.h) \
    $(wildcard include/config/mx6s.h) \
  arch/arm/include/asm/mach-imx/iomux-v3.h \
    $(wildcard include/config/imx93.h) \
    $(wildcard include/config/imx91.h) \
    $(wildcard include/config/mx6.h) \
    $(wildcard include/config/vf610.h) \
    $(wildcard include/config/iomux/share/conf/reg.h) \
    $(wildcard include/config/mx6d.h) \
  arch/arm/include/asm/arch/mx6ull_pins.h \
  arch/arm/include/asm/arch/sys_proto.h \
  arch/arm/include/asm/gpio.h \
    $(wildcard include/config/gpio/extra/header.h) \
  arch/arm/include/asm/arch/gpio.h \
  include/asm-generic/gpio.h \
    $(wildcard include/config/acpigen.h) \
  include/dm/ofnode.h \
    $(wildcard include/config/ofnode/multi/tree.h) \
    $(wildcard include/config/dm/inline/ofnode.h) \
  include/dm/of.h \
  include/dm/of_access.h \
  include/phy_interface.h \
    $(wildcard include/config/arch/lx2160a.h) \
    $(wildcard include/config/arch/lx2162a.h) \
    $(wildcard include/config/phy/ncsi.h) \
  include/string.h \
  include/linux/errno.h \
  arch/arm/include/asm/mach-imx/sys_proto.h \
    $(wildcard include/config/scmi/firmware.h) \
  arch/arm/include/asm/io.h \
  arch/arm/include/asm/memory.h \
    $(wildcard include/config/discontigmem.h) \
  arch/arm/include/asm/barriers.h \
  include/asm-generic/io.h \
  include/iotrace.h \
    $(wildcard include/config/io/trace.h) \
  arch/arm/include/asm/mach-imx/module_fuse.h \
    $(wildcard include/config/imx/module/fuse.h) \
  arch/arm/include/asm/mach-imx/../arch-imx/cpu.h \
  arch/arm/include/asm/mach-imx/boot_mode.h \
  arch/arm/include/asm/mach-imx/mxc_i2c.h \
    $(wildcard include/config/clk.h) \
    $(wildcard include/config/dm/i2c.h) \
  include/env.h \
    $(wildcard include/config/env/import/fdt.h) \
  include/compiler.h \
  /usr/local/cross_compiler/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/lib/gcc/arm-none-linux-gnueabihf/10.3.1/include/stddef.h \
  include/fsl_esdhc_imx.h \
    $(wildcard include/config/fsl/sdhc/v2/3.h) \
    $(wildcard include/config/sys/fsl/esdhc/le.h) \
    $(wildcard include/config/sys/fsl/esdhc/be.h) \
  include/mmc.h \
    $(wildcard include/config/dm/mmc.h) \
    $(wildcard include/config/mmc/supports/tuning.h) \
    $(wildcard include/config/mmc/hs400/es/support.h) \
    $(wildcard include/config/mmc/pwrseq.h) \
    $(wildcard include/config/mmc/uhs/support.h) \
    $(wildcard include/config/mmc/hs400/support.h) \
    $(wildcard include/config/blk.h) \
    $(wildcard include/config/mmc/write.h) \
    $(wildcard include/config/mmc/hw/partitioning.h) \
    $(wildcard include/config/dm/regulator.h) \
    $(wildcard include/config/mmc/spi.h) \
    $(wildcard include/config/sys/mmc/env/part.h) \
  include/linux/dma-direction.h \
  include/cyclic.h \
  include/u-boot/schedule.h \
  include/part.h \
    $(wildcard include/config/partition/uuids.h) \
    $(wildcard include/config/partition/type/guid.h) \
    $(wildcard include/config/dos/partition.h) \
    $(wildcard include/config/partitions.h) \
    $(wildcard include/config/spl/fs/ext4.h) \
    $(wildcard include/config/spl/fs/fat.h) \
    $(wildcard include/config/sys/mmcsd/raw/mode/u/boot/partition.h) \
    $(wildcard include/config/dual/bootloader.h) \
    $(wildcard include/config/imx/trusty/os.h) \
    $(wildcard include/config/efi/partition.h) \
  include/blk.h \
    $(wildcard include/config/sys/64bit/lba.h) \
    $(wildcard include/config/spl/legacy/block.h) \
    $(wildcard include/config/block/cache.h) \
    $(wildcard include/config/bounce/buffer.h) \
  include/bouncebuf.h \
  include/efi.h \
    $(wildcard include/config/efi/stub/64bit.h) \
    $(wildcard include/config/x86/64.h) \
    $(wildcard include/config/efi/ram/size.h) \
  include/linux/linkage.h \
  arch/arm/include/asm/linkage.h \
  include/ide.h \
    $(wildcard include/config/sys/ide/maxdevice.h) \
    $(wildcard include/config/sys/ide/maxbus.h) \
  include/u-boot/uuid.h \
  include/part_efi.h \
    $(wildcard include/config/efi/partition/entries/numbers.h) \
  include/i2c.h \
    $(wildcard include/config/sys/i2c/early/init.h) \
    $(wildcard include/config/at91rm9200.h) \
    $(wildcard include/config/at91sam9260.h) \
    $(wildcard include/config/at91sam9261.h) \
    $(wildcard include/config/at91sam9263.h) \
  include/miiphy.h \
    $(wildcard include/config/sys/fault/echo/link/down.h) \
    $(wildcard include/config/bitbangmii.h) \
  include/linux/mii.h \
  include/net.h \
    $(wildcard include/config/net/lwip.h) \
  include/net-common.h \
    $(wildcard include/config/sys/rx/eth/buffer.h) \
    $(wildcard include/config/dm/dsa.h) \
    $(wildcard include/config/dm/eth.h) \
    $(wildcard include/config/api.h) \
    $(wildcard include/config/efi/loader.h) \
    $(wildcard include/config/reset/phy/r.h) \
    $(wildcard include/config/net.h) \
  arch/arm/include/asm/cache.h \
    $(wildcard include/config/sys/cacheline/size.h) \
  arch/arm/include/asm/system.h \
    $(wildcard include/config/armv8/psci.h) \
    $(wildcard include/config/armv7/lpae.h) \
    $(wildcard include/config/cpu/v7a.h) \
    $(wildcard include/config/armv7/psci.h) \
    $(wildcard include/config/sys/arm/cache/writethrough.h) \
    $(wildcard include/config/sys/arm/cache/writealloc.h) \
    $(wildcard include/config/sys/arm/cache/writeback.h) \
  include/command.h \
    $(wildcard include/config/sys/longhelp.h) \
    $(wildcard include/config/auto/complete.h) \
    $(wildcard include/config/cmd/run.h) \
    $(wildcard include/config/cmd/memory.h) \
    $(wildcard include/config/cmd/i2c.h) \
    $(wildcard include/config/cmd/itest.h) \
    $(wildcard include/config/cmd/pci.h) \
    $(wildcard include/config/cmd/setexpr.h) \
    $(wildcard include/config/cmd/bootd.h) \
    $(wildcard include/config/cmd/bootm.h) \
    $(wildcard include/config/cmd/nvedit/efi.h) \
    $(wildcard include/config/cmd/read.h) \
    $(wildcard include/config/cmdline.h) \
    $(wildcard include/config/sys/cbsize.h) \
    $(wildcard include/config/sys/maxargs.h) \
  include/hexdump.h \
  include/linux/ctype.h \
  include/linux/if_ether.h \
  include/rand.h \
  include/time.h \
  include/linux/typecheck.h \
  include/net-legacy.h \
    $(wildcard include/config/bootp/dns2.h) \
    $(wildcard include/config/bootp/max/root/path/len.h) \
    $(wildcard include/config/cmd/dns.h) \
    $(wildcard include/config/cmd/ping.h) \
    $(wildcard include/config/cmd/cdp.h) \
    $(wildcard include/config/cmd/sntp.h) \
    $(wildcard include/config/netconsole.h) \
  include/phy.h \
    $(wildcard include/config/phy/fixed.h) \
  include/dm/read.h \
    $(wildcard include/config/dm/dev/read/inline.h) \
    $(wildcard include/config/of/platdata.h) \
    $(wildcard include/config/of/control.h) \
    $(wildcard include/config/of/libfdt.h) \
  include/dm/device.h \
    $(wildcard include/config/devres.h) \
    $(wildcard include/config/of/real.h) \
    $(wildcard include/config/dm/dma.h) \
    $(wildcard include/config/iommu.h) \
    $(wildcard include/config/pci.h) \
  include/dm/tag.h \
  include/dm/fdtaddr.h \
  include/dm/uclass.h \
  include/linux/ethtool.h \
  include/linux/mdio.h \
  include/linux/delay.h \
  include/power/pmic.h \
    $(wildcard include/config/power/legacy.h) \
  include/power/power_chrg.h \
  include/power/pfuze3000_pmic.h \
  board/freescale/mx6ull_test_emmc/../common/pfuze.h \
    $(wildcard include/config/dm/pmic/pfuze100.h) \

board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.o: $(deps_board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.o)

$(deps_board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.o):
```

### imximage_lpddr2.cfg修改

将mx6ull_test_emmc目录下imximage_lpddr2.cfg的内容修改为如下内容：

```c
/****************************************************
 * # 只是将内容中的mx6ullevk替换为了mx6ull_test_emmc.
 ***************************************************/
/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Refer docs/README.imxmage for more details about how-to configure
 * and create imximage boot image
 *
 * The syntax is taken as close as possible with the kwbimage
 */

#include <config.h>

/* image version */

IMAGE_VERSION 2

/*
 * Boot Device : one of
 * spi/sd/nand/onenand, qspi/nor
 */

#ifdef CONFIG_QSPI_BOOT
BOOT_FROM	qspi
#elif defined(CONFIG_NOR_BOOT)
BOOT_FROM	nor
#else
BOOT_FROM	sd
#endif

#ifdef CONFIG_USE_IMXIMG_PLUGIN
/*PLUGIN    plugin-binary-file    IRAM_FREE_START_ADDR*/
/****************************************************
 * # 只是将内容中的mx6ullevk替换为了mx6ull_test_emmc.
 ***************************************************/
PLUGIN	board/freescale/mx6ull_test_emmc/plugin.bin 0x00907000
#else

#ifdef CONFIG_IMX_HAB
CSF CONFIG_CSF_SIZE
#endif

/*
 * Device Configuration Data (DCD)
 *
 * Each entry must have the format:
 * Addr-type           Address        Value
 *
 * where:
 *	Addr-type register length (1,2 or 4 bytes)
 *	Address	  absolute address of the register
 *	value	  value to be stored in the register
 */

DATA 4 0x020c4068 0xffffffff
DATA 4 0x020c406c 0xffffffff
DATA 4 0x020c4070 0xffffffff
DATA 4 0x020c4074 0xffffffff
DATA 4 0x020c4078 0xffffffff
DATA 4 0x020c407c 0xffffffff
DATA 4 0x020c4080 0xffffffff

#ifdef CONFIG_IMX_OPTEE
DATA 4 0x20e4024 0x00000001
CHECK_BITS_SET 4 0x20e4024 0x1
#endif

DATA 4 0x020E04B4 0x00080000
DATA 4 0x020E04AC 0x00000000
DATA 4 0x020E027C 0x00000030
DATA 4 0x020E0250 0x00000030
DATA 4 0x020E024C 0x00000030
DATA 4 0x020E0490 0x00000030
DATA 4 0x020E0288 0x00000030
DATA 4 0x020E0270 0x00000000
DATA 4 0x020E0260 0x00000000
DATA 4 0x020E0264 0x00000000
DATA 4 0x020E04A0 0x00000030
DATA 4 0x020E0494 0x00020000
DATA 4 0x020E0280 0x00003030
DATA 4 0x020E0284 0x00003030
DATA 4 0x020E04B0 0x00020000
DATA 4 0x020E0498 0x00000030
DATA 4 0x020E04A4 0x00000030
DATA 4 0x020E0244 0x00000030
DATA 4 0x020E0248 0x00000030

DATA 4 0x021B001C 0x00008000
DATA 4 0x021B085C 0x1b4700c7
DATA 4 0x021B0800 0xA1390003
DATA 4 0x021B0890 0x23400A38
DATA 4 0x021B08b8 0x00000800

DATA 4 0x021B081C 0x33333333
DATA 4 0x021B0820 0x33333333
DATA 4 0x021B082C 0xf3333333
DATA 4 0x021B0830 0xf3333333
DATA 4 0x021B083C 0x20000000
DATA 4 0x021B0848 0x40403439
DATA 4 0x021B0850 0x4040342D
DATA 4 0x021B08C0 0x00921012
DATA 4 0x021B08b8 0x00000800

DATA 4 0x021B0004 0x00020052
DATA 4 0x021B0008 0x00000000
DATA 4 0x021B000C 0x33374133
DATA 4 0x021B0010 0x00100A82
DATA 4 0x021B0038 0x00170557
DATA 4 0x021B0014 0x00000093
DATA 4 0x021B0018 0x00201748
DATA 4 0x021B002C 0x0F9F26D2
DATA 4 0x021B0030 0x009F0010
DATA 4 0x021B0040 0x00000047
DATA 4 0x021B0000 0x83100000
DATA 4 0x021B001C 0x00008010
DATA 4 0x021B001C 0x003F8030
DATA 4 0x021B001C 0xFF0A8030
DATA 4 0x021B001C 0x82018030
DATA 4 0x021B001C 0x04028030
DATA 4 0x021B001C 0x01038030
DATA 4 0x021B0020 0x00001800
DATA 4 0x021B0818 0x00000000
DATA 4 0x021B0800 0xA1310003
DATA 4 0x021B0004 0x00025552
DATA 4 0x021B0404 0x00011006
DATA 4 0x021B001C 0x00000000
#endif
```

### imximage.cfg修改

将mx6ull_test_emmc目录下imximage.cfg内容更改为如下内容，其中包含DDR的校验信息，DDR的校验请查阅[DDR配置更新章节](#DDR配置更新)：

```makefile
/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
 *
 * Refer doc/imx/mkimage/imximage.txt for more details about how-to configure
 * and create imximage boot image
 *
 * The syntax is taken as close as possible with the kwbimage
 */

#include <config.h>

/* image version */

IMAGE_VERSION 2

/*
 * Boot Device : one of
 * spi/sd/nand/onenand, qspi/nor
 */

#ifdef CONFIG_QSPI_BOOT
BOOT_FROM	qspi
#elif defined(CONFIG_NOR_BOOT)
BOOT_FROM	nor
#else
BOOT_FROM	sd
#endif

#ifdef CONFIG_USE_IMXIMG_PLUGIN
/*PLUGIN    plugin-binary-file    IRAM_FREE_START_ADDR*/
PLUGIN	board/freescale/mx6ull_test_emmc/plugin.bin 0x00907000
#else

#ifdef CONFIG_IMX_HAB
CSF CONFIG_CSF_SIZE
#endif

/*
 * Device Configuration Data (DCD)
 *
 * Each entry must have the format:
 * Addr-type           Address        Value
 *
 * where:
 *	Addr-type register length (1,2 or 4 bytes)
 *	Address	  absolute address of the register
 *	value	  value to be stored in the register
 */

/* Enable all clocks */
DATA 4 0x020c4068 0xffffffff
DATA 4 0x020c406c 0xffffffff
DATA 4 0x020c4070 0xffffffff
DATA 4 0x020c4074 0xffffffff
DATA 4 0x020c4078 0xffffffff
DATA 4 0x020c407c 0xffffffff
DATA 4 0x020c4080 0xffffffff

#ifdef CONFIG_IMX_OPTEE
DATA 4 0x20e4024 0x00000001
CHECK_BITS_SET 4 0x20e4024 0x1
#endif

DATA 4 0x020E04B4 0x000C0000
DATA 4 0x020E04AC 0x00000000
DATA 4 0x020E027C 0x00000028
DATA 4 0x020E0250 0x00000028
DATA 4 0x020E024C 0x00000028
DATA 4 0x020E0490 0x00000028
DATA 4 0x020E0288 0x00000028
DATA 4 0x020E0270 0x00000000
DATA 4 0x020E0260 0x00000028
DATA 4 0x020E0264 0x00000028
DATA 4 0x020E04A0 0x00000028
DATA 4 0x020E0494 0x00020000
DATA 4 0x020E0280 0x00000028
DATA 4 0x020E0284 0x00000028
DATA 4 0x020E04B0 0x00020000
DATA 4 0x020E0498 0x00000028
DATA 4 0x020E04A4 0x00000028
DATA 4 0x020E0244 0x00000028
DATA 4 0x020E0248 0x00000028
DATA 4 0x021B001C 0x00008000
DATA 4 0x021B0800 0xA1390003
DATA 4 0x021B080C 0x00000000
DATA 4 0x021B083C 0x41640158
DATA 4 0x021B0848 0x40403036
DATA 4 0x021B0850 0x40403632
DATA 4 0x021B081C 0x33333333
DATA 4 0x021B0820 0x33333333
DATA 4 0x021B082C 0xf3333333
DATA 4 0x021B0830 0xf3333333
DATA 4 0x021B08C0 0x00921012
DATA 4 0x021B08b8 0x00000800
DATA 4 0x021B0004 0x0002002D
DATA 4 0x021B0008 0x1B333030
DATA 4 0x021B000C 0x676B52F3
DATA 4 0x021B0010 0xB66D0B63
DATA 4 0x021B0014 0x01FF00DB
DATA 4 0x021B0018 0x00211740
DATA 4 0x021B001C 0x00008000
DATA 4 0x021B002C 0x000026D2
DATA 4 0x021B0030 0x006B1023
DATA 4 0x021B0040 0x0000004F
DATA 4 0x021B0000 0x84180000
DATA 4 0x021B0890 0x00400a38
DATA 4 0x021B001C 0x02008032
DATA 4 0x021B001C 0x00008033
DATA 4 0x021B001C 0x00048031
DATA 4 0x021B001C 0x15208030
DATA 4 0x021B001C 0x04008040
DATA 4 0x021B0020 0x00007800
DATA 4 0x021B0818 0x00000227
DATA 4 0x021B0004 0x0002556D
DATA 4 0x021B0404 0x00011006
DATA 4 0x021B001C 0x00000000

#endif
```

### imximage.cfg.cfgtmp修改

将mx6ull_test_emmc目录下imximage.cfg.cfgtmp内容更改为如下内容：

```makefile
# 只是将内容中的mx6ullevk替换为了mx6ull_test_emmc.

# 1 "board/freescale/mx6ull_test_emmc/imximage.cfg"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "././include/linux/kconfig.h" 1



# 1 "include/generated/autoconf.h" 1
# 5 "././include/linux/kconfig.h" 2
# 1 "<command-line>" 2
# 1 "board/freescale/mx6ull_test_emmc/imximage.cfg"
# 13 "board/freescale/mx6ull_test_emmc/imximage.cfg"
# 1 "include/config.h" 1




# 1 "include/config_defaults.h" 1
# 6 "include/config.h" 2
# 1 "include/config_uncmd_spl.h" 1
# 7 "include/config.h" 2
# 1 "include/configs/mx6ull_test_emmc.h" 1
# 12 "include/configs/mx6ull_test_emmc.h"
# 1 "./arch/arm/include/asm/arch/imx-regs.h" 1
# 417 "./arch/arm/include/asm/arch/imx-regs.h"
# 1 "./arch/arm/include/asm/imx-common/regs-lcdif.h" 1
# 418 "./arch/arm/include/asm/arch/imx-regs.h" 2
# 13 "include/configs/mx6ull_test_emmc.h" 2
# 1 "include/linux/sizes.h" 1
# 14 "include/configs/mx6ull_test_emmc.h" 2
# 1 "include/configs/mx6_common.h" 1
# 49 "include/configs/mx6_common.h"
# 1 "./arch/arm/include/asm/imx-common/gpio.h" 1
# 50 "include/configs/mx6_common.h" 2
# 15 "include/configs/mx6ull_test_emmc.h" 2
# 8 "include/config.h" 2
# 1 "./arch/arm/include/asm/config.h" 1
# 9 "include/config.h" 2
# 1 "include/config_fallbacks.h" 1
# 9 "include/config.h" 2
# 14 "board/freescale/mx6ull_test_emmc/imximage.cfg" 2



IMAGE_VERSION 2
# 29 "board/freescale/mx6ull_test_emmc/imximage.cfg"
BOOT_FROM sd
# 54 "board/freescale/mx6ull_test_emmc/imximage.cfg"
DATA 4 0x020c4068 0xffffffff
DATA 4 0x020c406c 0xffffffff
DATA 4 0x020c4070 0xffffffff
DATA 4 0x020c4074 0xffffffff
DATA 4 0x020c4078 0xffffffff
DATA 4 0x020c407c 0xffffffff
DATA 4 0x020c4080 0xffffffff

DATA 4 0x020E04B4 0x000C0000
DATA 4 0x020E04AC 0x00000000
DATA 4 0x020E027C 0x00000030
DATA 4 0x020E0250 0x00000030
DATA 4 0x020E024C 0x00000030
DATA 4 0x020E0490 0x00000030
DATA 4 0x020E0288 0x000C0030
DATA 4 0x020E0270 0x00000000
DATA 4 0x020E0260 0x00000030
DATA 4 0x020E0264 0x00000030
DATA 4 0x020E04A0 0x00000030
DATA 4 0x020E0494 0x00020000
DATA 4 0x020E0280 0x00000030
DATA 4 0x020E0284 0x00000030
DATA 4 0x020E04B0 0x00020000
DATA 4 0x020E0498 0x00000030
DATA 4 0x020E04A4 0x00000030
DATA 4 0x020E0244 0x00000030
DATA 4 0x020E0248 0x00000030
DATA 4 0x021B001C 0x00008000
DATA 4 0x021B0800 0xA1390003
DATA 4 0x021B080C 0x00000004
DATA 4 0x021B083C 0x41640158
DATA 4 0x021B0848 0x40403237
DATA 4 0x021B0850 0x40403C33
DATA 4 0x021B081C 0x33333333
DATA 4 0x021B0820 0x33333333
DATA 4 0x021B082C 0xf3333333
DATA 4 0x021B0830 0xf3333333
DATA 4 0x021B08C0 0x00944009
DATA 4 0x021B08b8 0x00000800
DATA 4 0x021B0004 0x0002002D
DATA 4 0x021B0008 0x1B333030
DATA 4 0x021B000C 0x676B52F3
DATA 4 0x021B0010 0xB66D0B63
DATA 4 0x021B0014 0x01FF00DB
DATA 4 0x021B0018 0x00201740
DATA 4 0x021B001C 0x00008000
DATA 4 0x021B002C 0x000026D2
DATA 4 0x021B0030 0x006B1023
DATA 4 0x021B0040 0x0000004F
DATA 4 0x021B0000 0x84180000
DATA 4 0x021B0890 0x00400000
DATA 4 0x021B001C 0x02008032
DATA 4 0x021B001C 0x00008033
DATA 4 0x021B001C 0x00048031
DATA 4 0x021B001C 0x15208030
DATA 4 0x021B001C 0x04008040
DATA 4 0x021B0020 0x00000800
DATA 4 0x021B0818 0x00000227
DATA 4 0x021B0004 0x0002552D
DATA 4 0x021B0404 0x00011006
DATA 4 0x021B001C 0x00000000
```

这个文件貌似没有啥需要更改的，更改的都是些注释内容，不过保持一致性还是把他修改了吧。

### mx6ull_test_emmc/Kconfig修改

将mx6ull_test_emmc目录下Kconfig内容做如下更改：

```Kconfig
if TARGET_MX6ULL_TEST_EMMC

config SYS_BOARD
	default "mx6ull_test_emmc"

config SYS_VENDOR
	default "freescale"

config SYS_CONFIG_NAME
	default "mx6ull_test_emmc"

config IMX_CONFIG
	default "board/freescale/mx6ull_test_emmc/imximage.cfg"

config TEXT_BASE
	default 0x87800000
endif
```

其实就是把`mx6ullevk` 换成了 `mx6ull_test_emmc` 。

下面是 Kconfig 文件中的宏配置及其作用表格说明：

| 宏名（config）  | 默认值/内容                                     | 作用说明                                               |
| --------------- | ----------------------------------------------- | ------------------------------------------------------ |
| SYS_BOARD       | "mx6ull_test_emmc"                              | 指定板级名称，影响 include/configs/ 路径和部分宏定义。 |
| SYS_VENDOR      | "freescale"                                     | 指定厂商名称，影响路径和部分宏定义。                   |
| SYS_CONFIG_NAME | "mx6ull_test_emmc"                              | 指定配置名，影响 include/configs/xxx.h 的选择。        |
| IMX_CONFIG      | "board/freescale/mx6ull_test_emmc/imximage.cfg" | 指定 imximage 工具用的启动配置文件路径。               |
| TEXT_BASE       | 0x87800000                                      | 指定 U-Boot 代码的链接基地址（启动地址）。             |

这些配置通过 Kconfig 体系自动注入到编译系统和代码中，决定了板级初始化、启动参数、配置文件等的选择和行为。

### MAINTAINERS修改

将mx6ull_test_emmc目录下MAINTAINERS内容做如下更改：

```Kconfig
MX6ULL_TEST_EMMC BOARD
M:	Peng Fan <peng.fan@nxp.com>
S:	Maintained
F:	board/freescale/mx6ull_test_emmc/
F:	include/configs/mx6ull_test_emmc.h
F:	configs/mx6ull_test_emmc_defconfig
F:	configs/mx6ull_14x14_evk_plugin_defconfig
F:	configs/mx6ulz_14x14_evk_defconfig
```

下面是 MAINTAINERS 文件内容的表格讲解：

| 字段/内容                                    | 说明                                    |
| -------------------------------------------- | --------------------------------------- |
| MX6ULL_TEST_EMMC BOARD                       | 维护的板卡名称                          |
| M: Peng Fan <peng.fan@nxp.com>               | 主要维护者及联系方式                    |
| S: Maintained                                | 当前维护状态（Maintained 表示有人维护） |
| F: board/freescale/mx6ull_test_emmc/         | 该目录下所有文件都属于本维护范围        |
| F: include/configs/mx6ull_test_emmc.h        | 板级专用头文件，属于本维护范围          |
| F: configs/mx6ull_test_emmc_defconfig        | 板级 defconfig 配置文件，属于本维护范围 |
| F: configs/mx6ull_14x14_evk_plugin_defconfig | 相关 defconfig，属于本维护范围          |
| F: configs/mx6ulz_14x14_evk_defconfig        | 相关 defconfig，属于本维护范围          |

说明：  
- 该文件用于标明本板相关的维护者、维护状态和涉及的文件范围，便于社区协作和问题追踪。

### mx6ull_test_emmc/Makefile修改

将mx6ull_test_emmc/Makefile文件内容修改为如下内容：

```makefile
# SPDX-License-Identifier: GPL-2.0+
# (C) Copyright 2016 Freescale Semiconductor, Inc.

obj-y  := mx6ull_test_emmc.o
```

### mx6ull_test_emmc.c修改

进入 mx6ull_test_emmc 目录中 ， 将其中的 mx6ullevk.c 文件重命名为 mx6ull_test_emmc.c，命令如下：

```shell
cd mx6ull_test_emmc
mv mx6ullevk.c mx6ull_test_emmc.c
```

更新mx6ull_test_emmc.c中开发板信息：

```c
 int checkboard(void)
 {
	if (is_mx6ul_9x9_evk())
		 puts("Board: MX6UL 9x9 EVK\n");
	else if(is_cpu_type(MXC_CPU_MX6ULZ))
		 puts("Board: MX6UL 14x14 EVK\n");
	else 
		puts("Board: IMX6ULL Charliechen EMMC");
	return 0;
 }
```

### mx6ull_test_emmc.su修改

将mx6ull_test_emmc目录下mx6ullevk.su为过程文件，在编译后会消失，这里直接忽略，它不会有任何影响。

## 修改u-boot图形界面配置文件

uboot 是支持图形界面配置，现在加入新的开发板配置。

在 `arch/arm/mach-imx/mx6/Kconfig` 文件中，总共要添加两处内容：

* 581行，复制 `TARGET_MX6ULL_14X14_EVK` 的配置，粘贴后更改部分内容。
* 840行，添加新板子的kconfig配置路径。（这里是freescale的配置位置，就近添加方面查阅）。

如下所示：

```Kconfig
config TARGET_MX6ULL_TEST_EMMC
	bool "Support mx6ull_test_emmc"
	depends on MX6ULL
	select BOARD_LATE_INIT
	select DM
	select DM_THERMAL
	select IOMUX_LPSR
	select IMX_MODULE_FUSE
	select OF_SYSTEM_SETUP
	imply CMD_DM
	
...
# 在文件末尾添加 source文件路径
source "board/freescale/mx6ull_test_emmc/Kconfig"
```

如下图所示，这是新增加的配置部分：

![image](../../images/uboot/nxp/kconfig_add_new_board.png)

如下所示，这是新增加配置路径部分：

![image](../../images/uboot/nxp/kconfig_add_new_board_kconfig.png)

## DDR配置更新

关于DDR的配置和测试，笔者觉得还是直接观看[正点原子的ddr视频](https://www.bilibili.com/video/BV1yE411h7uQ?p=48&vd_source=b387713a15d6517575ab4761525174e7)吧，当然，如果是跟着正点原子的书籍来对照着看更好，因为这里涉及到一个启动方式的选择，和接线的选择。

<iframe
  src="https://player.bilibili.com/player.html?bvid=BV1yE411h7uQ&p=46&autoplay=0"
  frameborder="0"
  allowfullscreen
  style="width:100%; height:60vh;">
</iframe>

笔者自己的校验结果为：

```text
Byte 0: (0x08 - 0x5c), middle value:0x32
Byte 1: (0x0c - 0x60), middle value:0x36

MMDC0 MPWRDLCTL = 0x40403632


   MMDC registers updated from calibration 

   Write leveling calibration
   MMDC_MPWLDECTRL0 ch0 (0x021b080c) = 0x00000000
   MMDC_MPWLDECTRL1 ch0 (0x021b0810) = 0x000A000A

   Read DQS Gating calibration
   MPDGCTRL0 PHY0 (0x021b083c) = 0x014C0148
   MPDGCTRL1 PHY0 (0x021b0840) = 0x00000000

   Read calibration
   MPRDDLCTL PHY0 (0x021b0848) = 0x40403036

   Write calibration
   MPWRDLCTL PHY0 (0x021b0850) = 0x40403632


Success: DDR calibration completed!!!
```

### imximage.cfg

下一步需要更改 `board/freescale/mx6ull_test_emmc/imximage.cfg` 文件信息，**记住，只修改校验值**，下面是仅仅修改校验值后的文件内容：

```text
/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
 *
 * Refer doc/imx/mkimage/imximage.txt for more details about how-to configure
 * and create imximage boot image
 *
 * The syntax is taken as close as possible with the kwbimage
 */

#include <config.h>

/* image version */

IMAGE_VERSION 2

/*
 * Boot Device : one of
 * spi/sd/nand/onenand, qspi/nor
 */

#ifdef CONFIG_QSPI_BOOT
BOOT_FROM	qspi
#elif defined(CONFIG_NOR_BOOT)
BOOT_FROM	nor
#else
BOOT_FROM	sd
#endif

#ifdef CONFIG_USE_IMXIMG_PLUGIN
/*PLUGIN    plugin-binary-file    IRAM_FREE_START_ADDR*/
PLUGIN	board/freescale/mx6ullevk/plugin.bin 0x00907000
#else

#ifdef CONFIG_IMX_HAB
CSF CONFIG_CSF_SIZE
#endif

/*
 * Device Configuration Data (DCD)
 *
 * Each entry must have the format:
 * Addr-type           Address        Value
 *
 * where:
 *	Addr-type register length (1,2 or 4 bytes)
 *	Address	  absolute address of the register
 *	value	  value to be stored in the register
 */

/* Enable all clocks */
DATA 4 0x020c4068 0xffffffff
DATA 4 0x020c406c 0xffffffff
DATA 4 0x020c4070 0xffffffff
DATA 4 0x020c4074 0xffffffff
DATA 4 0x020c4078 0xffffffff
DATA 4 0x020c407c 0xffffffff
DATA 4 0x020c4080 0xffffffff

#ifdef CONFIG_IMX_OPTEE
DATA 4 0x20e4024 0x00000001
CHECK_BITS_SET 4 0x20e4024 0x1
#endif

DATA 4 0x020E04B4 0x000C0000
DATA 4 0x020E04AC 0x00000000
DATA 4 0x020E027C 0x00000030
DATA 4 0x020E0250 0x00000030
DATA 4 0x020E024C 0x00000030
DATA 4 0x020E0490 0x00000030
DATA 4 0x020E0288 0x000C0030
DATA 4 0x020E0270 0x00000000
DATA 4 0x020E0260 0x00000030
DATA 4 0x020E0264 0x00000030
DATA 4 0x020E04A0 0x00000030
DATA 4 0x020E0494 0x00020000
DATA 4 0x020E0280 0x00000030
DATA 4 0x020E0284 0x00000030
DATA 4 0x020E04B0 0x00020000
DATA 4 0x020E0498 0x00000030
DATA 4 0x020E04A4 0x00000030
DATA 4 0x020E0244 0x00000030
DATA 4 0x020E0248 0x00000030
DATA 4 0x021B001C 0x00008000
DATA 4 0x021B0800 0xA1390003
DATA 4 0x021B080C 0x00000000
DATA 4 0x021B083C 0x014C0148
DATA 4 0x021B0848 0x40403036
DATA 4 0x021B0850 0x40403632
DATA 4 0x021B081C 0x33333333
DATA 4 0x021B0820 0x33333333
DATA 4 0x021B082C 0xf3333333
DATA 4 0x021B0830 0xf3333333
DATA 4 0x021B08C0 0x00944009
DATA 4 0x021B08b8 0x00000800
DATA 4 0x021B0004 0x0002002D
DATA 4 0x021B0008 0x1B333030
DATA 4 0x021B000C 0x676B52F3
DATA 4 0x021B0010 0xB66D0B63
DATA 4 0x021B0014 0x01FF00DB
DATA 4 0x021B0018 0x00201740
DATA 4 0x021B001C 0x00008000
DATA 4 0x021B002C 0x000026D2
DATA 4 0x021B0030 0x006B1023
DATA 4 0x021B0040 0x0000004F
DATA 4 0x021B0000 0x84180000
DATA 4 0x021B0890 0x00400000
DATA 4 0x021B001C 0x02008032
DATA 4 0x021B001C 0x00008033
DATA 4 0x021B001C 0x00048031
DATA 4 0x021B001C 0x15208030
DATA 4 0x021B001C 0x04008040
DATA 4 0x021B0020 0x00000800
DATA 4 0x021B0818 0x00000227
DATA 4 0x021B0004 0x0002552D
DATA 4 0x021B0404 0x00011006
DATA 4 0x021B001C 0x00000000

#endif
```

### imxdownload.h修改

笔者看到其他博主都会推荐修改imxdownload.h文件，重新编译imxdownload工具。实际上，笔者经过测试，就用正点原子的原来的工具是没有问题的，如果修改里面的ddr配置也应该只修改笔者校验值里面的部分，别的都别动，除非读者打算大改DDR配置，这时候读者需要查看下DDR寄存器组，它的作用以及有什么影响。

按照笔者个人的使用经验，一年前笔者还可以头头是道的告诉大家DDR应该怎么配置，一年后的笔者已经完全陌生了，尤其是DCD数据分析部分。因此站在笔者个人角度来讲，不推荐去研究这个东西，除非读者后面运行程序出现了问题。还有一方面是在u-boot-2025的配置中，它已经不再使用imxdownload的配置了，它里面的配置已经完全变化了。笔者是没有耐心去折腾DDR配置参数了，准备将就使用nxp官方的配置加上笔者的ddr校验值。读者可以自行研究，笔者只在此提醒读者哪些部分可以更改，然后快速进入开发模式。

## 设备树文件环境新增

### 配置一份隔离环境

imx6ull-14x14-evk-emmc.dts 复制+重命名

从现在开始笔者要开始修改设备树了，复制 `arch/arm/dts/imx6ull-14x14-evk-emmc.dts` 重命名为 `arch/arm/dts/imx6ull-14x14-test-emmc.dts`，执行如下命令：

```shell
cp arch/arm/dts/imx6ull-14x14-evk-emmc.dts arch/arm/dts/imx6ull-14x14-test-emmc.dts
```

**笔者看网上很多教程老是把"-14x14-"这个中缀丢掉，笔者不推荐**，因为笔者要隔离环境，要保持和原来的配置思路一致，这样笔者命名的新开发板在以后才好观察配置数据，不是杂乱无章的状态。在开发中，跟着原来的规律配置读者的设备树是非常重要。（难道笔者会告诉读者，曾经因为丢掉这个中缀而导致配置混乱的经历吗？）

这里笔者展开 `imx6ull-14x14-evk-emmc.dts` 的内容发现配置都已经聚集在了 `imx6ull-14x14-evk.dts` 文件里面：

```dtb
/*
 * Copyright 2019 NXP
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "imx6ull-14x14-evk.dts"

&usdhc2 {
	pinctrl-names = "default", "state_100mhz", "state_200mhz";
	pinctrl-0 = <&pinctrl_usdhc2_8bit>;
	pinctrl-1 = <&pinctrl_usdhc2_8bit_100mhz>;
	pinctrl-2 = <&pinctrl_usdhc2_8bit_200mhz>;
	bus-width = <8>;
	non-removable;
	status = "okay";
};
```

#### imx6ull-14x14-evk.dts 复制+重命名

因此，笔者要对它进行修改的话，需要将 `imx6ull-14x14-evk.dts` 文件复制，然后重命名为笔者自己的配置 `imx6ull-14x14-test.dts` ，执行如下命令：

```shell
cp arch/arm/dts/imx6ull-14x14-evk.dts arch/arm/dts/imx6ull-14x14-test.dts
```

笔者展开看下 ``imx6ull-14x14-evk.dts` ` ：

```dtb
// SPDX-License-Identifier: (GPL-2.0 OR MIT)
//
// Copyright (C) 2016 Freescale Semiconductor, Inc.

/dts-v1/;

#include "imx6ull.dtsi"
#include "imx6ul-14x14-evk.dtsi"
#include "imx6ul-14x14-evk-u-boot.dtsi"

/ {
	model = "i.MX6 ULL 14x14 EVK Board";
	compatible = "fsl,imx6ull-14x14-evk", "fsl,imx6ull";
};

&clks {
	assigned-clocks = <&clks IMX6UL_CLK_PLL3_PFD2>,
			  <&clks IMX6UL_CLK_PLL4_AUDIO_DIV>;
	assigned-clock-rates = <320000000>, <786432000>;
};

&csi {
	status = "okay";
};

&ov5640 {
	status = "okay";
};

/delete-node/ &sim2;

```

会发现笔者很多内容都被聚集到了三个文件：

*  `imx6ull.dtsi` 
* `imx6ul-14x14-evk.dtsi`
* `imx6ul-14x14-evk-u-boot.dtsi`

老规矩，笔者要将这几个文件单独复制并重命名，保证环境隔离。

####  imx6ull.dtsi 复制+重命名

 将 `imx6ull.dtsi` 复制重命名为  `imx6ull-test.dtsi` 

#### imx6ul-14x14-evk.dtsi 复制+重命名

 将 `imx6ul-14x14-evk.dtsi` 复制重命名为  `imx6ul-14x14-test.dtsi` 

#### imx6ul-14x14-evk-u-boot.dtsi 复制+重命名

 将 `imx6ul-14x14-evk-u-boot.dtsi` 复制重命名为  `imx6ul-14x14-test-u-boot.dtsi`

#### imx6ul.dtsi复制+重命名

将 `imx6ul.dtsi` 复制重命名为 `imx6ul-test.dtsi`。

### 修改设备树文件的内容

* 笔者不在这里修改驱动的设备树的描述，因为这和具体驱动相关，也不会像其他的资料一样把冗余的内容删除，这是为了聚焦具体驱动而做，避免造成勿扰（让读者觉得非要删除不可）。
* 在适配驱动的设备树修改最多的文件是 `imx6ul-14x14-test.dtsi`。因为nxp几乎将后续的所有imx6ul和imx6ull相同的都合在一起了。
* 下面的文件修改只做一件事：仅仅修改引入头文件的名。目的是为了做环境隔离，下面才会具体驱动来修改驱动。

#### imx6ull-test-emmc.dts

修改 imx6ull-14x14-test-emmc.dts文件为：

```dtb
/*
 * Copyright 2019 NXP
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

/* 修改这里的设备树引入文件名 */
#include "imx6ull-14x14-test.dts"

&usdhc2 {
	pinctrl-names = "default", "state_100mhz", "state_200mhz";
	pinctrl-0 = <&pinctrl_usdhc2_8bit>;
	pinctrl-1 = <&pinctrl_usdhc2_8bit_100mhz>;
	pinctrl-2 = <&pinctrl_usdhc2_8bit_200mhz>;
	bus-width = <8>;
	non-removable;
	status = "okay";
};
```

#### imx6ull-14x14-test.dts

修改 文件的内容如下：

```dtb
// SPDX-License-Identifier: (GPL-2.0 OR MIT)
//
// Copyright (C) 2016 Freescale Semiconductor, Inc.

/dts-v1/;

/* 主要是把头文件都替换为笔者自己重命名的文件来引入 */
#include "imx6ull-test.dtsi"
#include "imx6ul-14x14-test.dtsi"
#include "imx6ul-14x14-test-u-boot.dtsi"

/ {
	model = "i.MX6 ULL 14x14 EVK Board";
	compatible = "fsl,imx6ull-14x14-evk", "fsl,imx6ull";
};

&clks {
	assigned-clocks = <&clks IMX6UL_CLK_PLL3_PFD2>,
			  <&clks IMX6UL_CLK_PLL4_AUDIO_DIV>;
	assigned-clock-rates = <320000000>, <786432000>;
};

&csi {
	status = "okay";
};

&ov5640 {
	status = "okay";
};

/delete-node/ &sim2;
```

#### imx6ull-test.dtsi

笔者这里要修改下该设备树头文件的配置信息：

```dtb
// SPDX-License-Identifier: (GPL-2.0 OR MIT)
//
// Copyright 2016 Freescale Semiconductor, Inc.

/* 这里引入笔者复制出来的隔离环境的配置 */
#include "imx6ul-test.dtsi"
#include "imx6ull-pinfunc.h"
#include "imx6ull-pinfunc-snvs.h"
...
```

#### 向 `arch/arm/dts/Makefile` 新增设备树文件

将 `arch/arm/dts/Makefile` 的 `dtb-$(CONFIG_MX6ULL)` 加入笔者新增的设备树目标（实际上笔者在这里没有加入也没有出什么大问题，但是严谨一点还是要加上的）：

```makefile
dtb-$(CONFIG_MX6ULL) += \
	imx6ull-14x14-evk.dtb \
	imx6ull-14x14-evk-emmc.dtb \
	imx6ull-14x14-evk-gpmi-weim.dtb \
	imx6ull-14x14-test-emmc.dtb \
	imx6ull-9x9-evk.dtb \
	imx6ull-colibri-emmc-eval-v3.dtb \
	imx6ull-colibri-eval-v3.dtb \
	imx6ull-myir-mys-6ulx-eval.dtb \
	imx6ull-seeed-npi-imx6ull-dev-board.dtb \
	imx6ull-phytec-segin-ff-rdk-emmc.dtb \
	imx6ull-dart-6ul.dtb \
	imx6ull-somlabs-visionsom.dtb \
	imx6ulz-14x14-evk.dtb \
	imx6ulz-14x14-evk-emmc.dtb \
	imx6ulz-14x14-evk-gpmi-weim.dtb
```

![image](../../images/uboot/nxp/add_new_dtb.png)



## 编译新加入的开发板配置的uboot

编译流程执行如下命令

```shell
# 清楚所有无关配置
make distclean

# 编译新增开发板的defconfig配置
make mx6ull_test_emmc_defconfig

# 执行多线程编译，有两种方式编译，一种是显示makefile执行流程，添加V=1;另一种是不加。
# 添加V=1，打印makefile执行命令
make V=1 -jn$(nproc)

# 默认不打印makefile执行命令，仅显示部分log
make -jn$(nproc)
```

编译示例如下：

![image](../../images/uboot/nxp/new_board_compile.png)

这是编译完成后的示例：

![image](../../images/uboot/nxp/new_board_compile_end.png)

## 新板子uboot驱动适配

### LCD驱动修改

#### LCD屏幕硬件属性

这里笔者开始移植LCD驱动，在 `arch/arm/dts/imx6ul-14x14-test.dtsi` 文件中，有 `lcdif` 设备树节点，这里笔者采用的正点原子的800x400的触摸屏ATK4384，配置参数如下：

| 属性         | 值   | 单位 |
| ------------ | ---- | ---- |
| 水平显示区域 | 800  | tCLK |
| HSPW(thp)    | 48   | tCLK |
| HBP(thb)     | 88   | tCLK |
| HFP(thf)     | 40   | tCLK |
| 垂直显示区域 | 480  | th   |
| VSPW(tvp)    | 3    | th   |
| VBP(tvb)     | 32   | th   |
| VFP(tvf)     | 13   | th   |
| 像素时钟     | 31   | MHz  |

#### lcdif设备树节点适配

因此设备树的属性节点修改配置如下：

```dtb
/* arch/arm/dts/imx6ul-14x14-test.dtsi */

&lcdif {
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_lcdif_dat
		     &pinctrl_lcdif_ctrl>;
	display = <&display0>;
	status = "okay";

	display0: display@0 {
		bits-per-pixel = <24>;
		bus-width = <24>;

		display-timings {
			native-mode = <&timing0>;

			timing0: timing0 {
				clock-frequency = <31000000>;
				hactive = <800>;
				vactive = <480>;
				hfront-porch = <40>;
				hback-porch = <88>;
				hsync-len = <48>;
				vback-porch = <32>;
				vfront-porch = <13>;
				vsync-len = <3>;
				hsync-active = <0>;
				vsync-active = <0>;
				de-active = <1>;
				pixelclk-active = <0>;
			};
		};
	};
};
```

![image](../../images/uboot/nxp/dtb_lcdif_config.png)

#### lcdif设备节点属性与lcd屏幕属性映射关系

配置映射关系：

| 设备树属性名    | 设备属性     | 值               | 单位 |
| --------------- | ------------ | ---------------- | ---- |
| clock-frequency | 像素时钟     | 31M（3'100'000） | Hz   |
| hactive         | 水平显示区域 | 800              | tCLK |
| vactive         | 垂直显示区域 | 480              | th   |
| hfront-porch    | HFP(thf)     | 40               | tCLK |
| hback-porch     | HBP(thb)     | 88               | tCLK |
| hsync-len       | HSPW(thp)    | 48               | tCLK |
| vback-porch     | VBP(tvb)     | 32               | th   |
| vfront-porch    | VFP(tvf)     | 13               | th   |
| vsync-len       | VSPW(tvp)    | 3                | th   |

#### LCD屏幕硬件配置

##### LCD屏幕硬件图

正点原子阿尔法v2.2(板子后面有硬件版本说明) LCD 屏幕硬件引脚说明：

![image](../../images/uboot/nxp/rgb_lcd_elc_route.png)

##### 设备树描述

###### LCD DATA引脚配置

```dtb
/* arch/arm/dts/imx6ul-14x14-test.dtsi */

pinctrl_lcdif_dat: lcdifdatgrp {
    fsl,pins = <
        MX6UL_PAD_LCD_DATA00__LCDIF_DATA00  0x79
        MX6UL_PAD_LCD_DATA01__LCDIF_DATA01  0x79
        MX6UL_PAD_LCD_DATA02__LCDIF_DATA02  0x79
        MX6UL_PAD_LCD_DATA03__LCDIF_DATA03  0x79
        MX6UL_PAD_LCD_DATA04__LCDIF_DATA04  0x79
        MX6UL_PAD_LCD_DATA05__LCDIF_DATA05  0x79
        MX6UL_PAD_LCD_DATA06__LCDIF_DATA06  0x79
        MX6UL_PAD_LCD_DATA07__LCDIF_DATA07  0x79
        MX6UL_PAD_LCD_DATA08__LCDIF_DATA08  0x79
        MX6UL_PAD_LCD_DATA09__LCDIF_DATA09  0x79
        MX6UL_PAD_LCD_DATA10__LCDIF_DATA10  0x79
        MX6UL_PAD_LCD_DATA11__LCDIF_DATA11  0x79
        MX6UL_PAD_LCD_DATA12__LCDIF_DATA12  0x79
        MX6UL_PAD_LCD_DATA13__LCDIF_DATA13  0x79
        MX6UL_PAD_LCD_DATA14__LCDIF_DATA14  0x79
        MX6UL_PAD_LCD_DATA15__LCDIF_DATA15  0x79
        MX6UL_PAD_LCD_DATA16__LCDIF_DATA16  0x79
        MX6UL_PAD_LCD_DATA17__LCDIF_DATA17  0x79
        MX6UL_PAD_LCD_DATA18__LCDIF_DATA18  0x79
        MX6UL_PAD_LCD_DATA19__LCDIF_DATA19  0x79
        MX6UL_PAD_LCD_DATA20__LCDIF_DATA20  0x79
        MX6UL_PAD_LCD_DATA21__LCDIF_DATA21  0x79
        MX6UL_PAD_LCD_DATA22__LCDIF_DATA22  0x79
        MX6UL_PAD_LCD_DATA23__LCDIF_DATA23  0x79
    >;
};
```

###### LCD CTRL引脚配置

lcd屏幕控制引脚配置如下：

```dtb
/* arch/arm/dts/imx6ul-14x14-test.dtsi */

pinctrl_lcdif_ctrl: lcdifctrlgrp {
    fsl,pins = <
        MX6UL_PAD_LCD_CLK__LCDIF_CLK	    0x79
        MX6UL_PAD_LCD_ENABLE__LCDIF_ENABLE  0x79
        MX6UL_PAD_LCD_HSYNC__LCDIF_HSYNC    0x79
        MX6UL_PAD_LCD_VSYNC__LCDIF_VSYNC    0x79
        /* used for lcd reset */
        /* MX6UL_PAD_SNVS_TAMPER9__GPIO5_IO09  0x79 */
    >;
};
```

图中 RGB LCD 的 RESET  (pin40)  引脚没有链接到imx6ull，也就是 `MX6UL_PAD_SNVS_TAMPER9__GPIO5_IO09  0x79` 这行是没必要存在的。

###### 驱动修改

那么对应的C源码里面也要修改：

`board/freescale/mx6ull_test_emmc/mx6ull_test_emmc.c`文件中关于LCD屏幕复位的代码就可以注释掉了：

![image](../../images/uboot/nxp/lcd_driver_change.png)

到这里LCD屏幕移植完结，下面开始最紧张的网口驱动移植。

### 网口移植

我这里是正点原子imx6ull阿尔法开发板v2.2，网口phy芯片采用的LAN8720A。这个坑爹芯片不是很稳定，每次启动需要软复位。因此操作流程上可谓复杂异常。实名羡慕后面新开发板的新phy芯片的兄弟。

#### xxx_defconfig修改phy芯片类型

`mx6ull_test_emmc_defconfig` 打开这个配置文件，向它修改phy芯片公司配置选项。LAN8720A芯片是SMSC公司的芯片，因此做出如下修改：

```makefile
# configs/mx6ull_test_emmc_defconfig

# CONFIG_PHY_MICREL=y
# CONFIG_PHY_MICREL_KSZ8XXX=y
CONFIG_PHY_SMSC=y
```

![image](../../images/uboot/nxp/xxx_defconfig_net_change.png)

#### 硬件连接

enet1的硬件图：

![image](../../images/uboot/nxp/enet1_elc.png)

enet2的硬件图

![image](../../images/uboot/nxp/enet2_elc.png)

网口复位引脚，如下图所示的SNVS_REMPER7(pin 23) 和 SNVS_REMPER8(pin 26) ：

![image](../../images/uboot/nxp/enet_reset_pin_elc.png)

#### 修改phy芯片设备树描述

##### 切换phy网卡驱动到LAN8720A

在 `drivers/net/phy/smsc.c` 文件中，有关于LAN8720A的芯片设备id说明，如下所示：

![image](../../images/uboot/nxp/lan8720_id_description.png)

仔细观察一下这个设备节点的成员函数，`.features` 、 `.config` 、 `.startup` 、 `.shutdown` 等函数。

我们在官方设备树 `arch/arm/dts/imx6ul-14x14-evk.dtsi` 文件中的设备树 compatible是描述是 `compatible = "ethernet-phy-id0022.1560";` 如下图所示：

![image](../../images/uboot/nxp/enet_type_choose_in_official.png)

那么这里选择是什么网卡呢？在 `drivers/net/phy/micrel_ksz8xxx.c` 文件中有相关配置，对比name成员的值和uid的值，代码如下所示：

```c
U_BOOT_PHY_DRIVER(ksz8081) = {
	.name = "Micrel KSZ8081",
	.uid = 0x221560,
	.mask = 0xfffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &ksz8081_config,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
};
```

图片如下所示：

![image](../../images/uboot/nxp/enet_define_in_official.png)

因此我们现在产生两个问题：

1. 要不要更换设备树的compatible属性成员的值，以便精确到具体的网卡型号？。
2. 如果要更换，应该怎么适配设备树以及驱动追踪。

* 先解决要不要换的问题：正常情况下，如果读者能够将 `ksz_8081_config` 函数的配置做到适配 LAN8720A phy芯片，那就可以不换；但是容易让后面的维护者犯困难，无法从uboot配置得到phy芯片的型号。

* 再解决如何适配：将设备树的 `fec` 节点下的 `mdio` 节点的 `compatible` 值选做LAN8720A的配置，它的配置定义在 `drivers/net/phy/smsc.c` 文件中，内容如下：

  ```c
  U_BOOT_PHY_DRIVER(lan8710) = {
  	.name = "SMSC LAN8710/LAN8720",
  	.uid = 0x0007c0f0,				// 注意看这个uid值，下面修改设备树要用
  	.mask = 0xffff0,
  	.features = PHY_BASIC_FEATURES,
  	.config = &genphy_config_aneg,
  	.startup = &genphy_startup,
  	.shutdown = &genphy_shutdown,
  };
  ```

  那么我们对fec节点的mdio节点做如下修改：

  ```dtb
  &fec1 {
  	pinctrl-names = "default";
  	pinctrl-0 = <&pinctrl_enet1
  				 &pinctrl_enet1_reset>;	// 添加复位控制引脚
  	phy-mode = "rmii";
  	phy-handle = <&ethphy0>;
  	// 添加这些 -----------------------------------------
  	phy-reset-gpios = <&gpio5 7 GPIO_ACTIVE_LOW>;
  	phy-reset-duration = <200>;
  	phy-reset-post-delay = <200>;
  	// 到这里 -----------------------------------------
  	phy-supply = <&reg_peri_3v3>;
  	status = "okay";
  };
  
  &fec2 {
  	pinctrl-names = "default";
  	pinctrl-0 = <&pinctrl_enet2
  				 &pinctrl_enet2_reset>;	// 添加复位控制引脚
  	phy-mode = "rmii";
  	phy-handle = <&ethphy1>;
  	phy-supply = <&reg_peri_3v3>;
  	// 添加这些 -----------------------------------------
  	phy-reset-gpios = <&gpio5 8 GPIO_ACTIVE_LOW>;
  	phy-reset-duration = <200>;
  	phy-reset-post-delay = <200>;
  	// 到这里 -----------------------------------------
  	status = "okay";
  
  	mdio {
  		#address-cells = <1>;
  		#size-cells = <0>;
  
  		ethphy0: ethernet-phy@0 {
  			// ------- 添加LAN8720A的指定  -----------
  			compatible = "ethernet-phy-id0007.c0f0";
  			// ----------------------------------end
  			reg = <0>;
  			smsc,led-mode = <1>;
  			clocks = <&clks IMX6UL_CLK_ENET_REF>;
  			clock-names = "rmii-ref";
  
  		};
  
  		ethphy1: ethernet-phy@1 {
  			// ------- 添加LAN8720A的指定  -----------
  			compatible = "ethernet-phy-id0007.c0f0";
  			// ----------------------------------end
  			reg = <1>;
  			smsc,led-mode = <1>;
  			clocks = <&clks IMX6UL_CLK_ENET2_REF>;
  			clock-names = "rmii-ref";
  		};
  	};
  };
  ```

  ![image](../../images/uboot/nxp/enet_dtb_add_lan8720a.png)

##### 向iomux节点添加复位控制引脚

向 `arch/arm/dts/imx6ul-14x14-test.dtsi` 设备树文件的 `iomux` 节点添加网口复位引脚配置，如下内容：

```dtb
&iomux {
	...
	pinctrl_enet1_reset: enet1resetgrp {
		fsl,pins = <
			MX6UL_PAD_SNVS_TAMPER7__GPIO5_IO07		0x10B0
		>;
	};
	pinctrl_enet2_reset: enet2resetgrp {
		fsl,pins = <
			MX6UL_PAD_SNVS_TAMPER8__GPIO5_IO08		0x10B0
		>;
	};
	...
}
```

![image](../../images/uboot/nxp/enet_reset_pin_dtb_config_1.png)

##### 解决网口复位引脚冲突

将与网口复位引脚冲突的设备树配置进行注释，在 `arch/arm/dts/imx6ul-14x14-test.dtsi` 文件下的 `spi-4` 节点，有与网口复位引脚冲突的配置，修改后的内容如下所示：

```dtb
spi-4 {
    compatible = "spi-gpio";
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_spi4>;
    status = "okay";
    gpio-sck = <&gpio5 11 0>;
    gpio-mosi = <&gpio5 10 0>;
    /* 
    cs-gpios = <&gpio5 7 GPIO_ACTIVE_LOW>;
    */
    num-chipselects = <1>;
    #address-cells = <1>;
    #size-cells = <0>;

    gpio_spi: gpio@0 {
        compatible = "fairchild,74hc595";
        gpio-controller;
        #gpio-cells = <2>;
        reg = <0>;
        registers-number = <1>;
        registers-default = /bits/ 8 <0x57>;
        spi-max-frequency = <100000>;
        /*
        enable-gpios = <&gpio5 8 GPIO_ACTIVE_LOW>;
        */
    };
};
```

![image](../../images/uboot/nxp/enet_reset_pin_conflict.png)

##### 将复位控制添加到fec设备节点

我们重新修改下fec网络节点，添加复位控制引脚，并且将网口地址进行修改，配置如下：

```dtb
&fec1 {
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_enet1
				 &pinctrl_enet1_reset>;	// 添加复位控制引脚
	phy-mode = "rmii";
	phy-handle = <&ethphy0>;
	// 添加这些 -----------------------------------------
	phy-reset-gpios = <&gpio5 7 GPIO_ACTIVE_LOW>;
	phy-reset-duration = <200>;
	phy-reset-post-delay = <200>;
	// 到这里 -----------------------------------------
	phy-supply = <&reg_peri_3v3>;
	status = "okay";
};

&fec2 {
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_enet2
				 &pinctrl_enet2_reset>;	// 添加复位控制引脚
	phy-mode = "rmii";
	phy-handle = <&ethphy1>;
	phy-supply = <&reg_peri_3v3>;
	// 添加这些 -----------------------------------------
	phy-reset-gpios = <&gpio5 8 GPIO_ACTIVE_LOW>;
	phy-reset-duration = <200>;
	phy-reset-post-delay = <200>;
	// 到这里 -----------------------------------------
	status = "okay";

	mdio {
		#address-cells = <1>;
		#size-cells = <0>;
		
		// ------- 修改 phy2为phy0 ---------------
		ethphy0: ethernet-phy@0 {
		// -----------------------------------end
			compatible = "ethernet-phy-id0007.c0f0";
			
			// ------- 修改 phy2为phy0 ---------------
			reg = <0>;
			// -----------------------------------end
			
			smsc,led-mode = <1>;
			clocks = <&clks IMX6UL_CLK_ENET_REF>;
			clock-names = "rmii-ref";

		};

		ethphy1: ethernet-phy@1 {
			compatible = "ethernet-phy-id0007.c0f0";
			reg = <1>;
			smsc,led-mode = <1>;
			clocks = <&clks IMX6UL_CLK_ENET2_REF>;
			clock-names = "rmii-ref";
		};
	};
};
```

![image](../../images/uboot/nxp/enet_dtb_add_reset_pin.png)

#### 为LAN8720A驱动添加新的复位操作

接下来，进入最为重要的一步骤，为LAN8720A添加软复位。它的参考非常意外，在一个[uboot移植网络攻略](https://blog.csdn.net/ZHONGCAI0901/article/details/118310802)的评论区：

![image](../../images/uboot/nxp/enet_sw_reset_config.png)

那么我们追踪到drivers/net/phy/phy.c，添加依据软复位代码：

![image](../../images/uboot/nxp/enet_sw_reset_driver.png)

## 编译下载

指令如下：

```shell
$ make distclean   						# 清除所有文件
$ make mx6ull_test_emmc_defconfig		# 选择新开发板的配置
$ make -j$(nproc)						# 多线程编译
$ ./imxdownload u-boot-dtb.bin /dev/sdb	# 我的sd卡是sdb，将uboot下载到sd卡
```

在正点原子中，它的拨码开关上面有丝印关于启动方式的拨码开关的设定。笔者这里要采用TF卡。后续配置网口等操作。笔者不准备往下写了。毕竟都是学新版本的移植的人，笔者不准备把读者当成小白一样对待。

[【正点原子 第三期】系统移植与根文件系统（P27，36:22处）](https://www.bilibili.com/video/BV12E411h71h?p=27&t=2182)网口调试参考视频链接如下，36:22 讲解如何配置uboot网口配置：

<iframe
  src="https://player.bilibili.com/player.html?bvid=BV12E411h71h&p=27&autoplay=0"
  frameborder="0"
  allowfullscreen
  style="width: 100%; height: 60vh;">
</iframe>


# kernel移植

参考[uboot和kernel获取](#uboot和kernel获取)章节，将kernel版本切换到 `origin/lf-6.1.y` 分支，笔者接下来将会分析lf-6.1.y分支的kernel移植操作：

```shell
git checkout origin/lf-6.1.y
```

笔者接下来将几乎参照正点原子的imx6ull-linux驱动开发指南进行编写，方便读者跟着对照印证新kernel以及根文件系统变化。

## 修改顶层Makefile，添加交叉编译器前缀和架构

为顶层Makefile添加交叉编译器前缀和架构：

```makefile
ARCH ?= arm
CROSS_COMPILE ?= arm-none-linux-gnueabihf-
```

![image](../../images/kernel/nxp/top_makefile_add_arch.png)

在分析 Linux 之前一定要先在 Ubuntu 中编译一下 Linux，因为编译过程会生成一些文件，而生成的这些恰恰是分析Linux 不可或缺的文件。

## 编译kernel

笔者这里是imx6ull芯片，`xxx_defconfig` 采用 `imx_v7_defconfig` 配置，下面是编译生产命令：

```shell
make distclean
make imx_v7_defconfig
make menuconfig
make -j$(nproc)
```

1. `make distclean` ：清除kernel的所有编译文件。

2. `make imx_v7_defconfig` ：导入imx6ull基本配置。这里有个 `imx_v7_defconfig` 这个配置，笔者需要讲解下它的来源。正常操作是直接搜索 `CONFIG_SOC_MX6UL` 选项，在 `xxx_defconfig`，看下谁含有这个配置项就基本能够锁定对应的 `xxx_defconfig`。

   ![image](../../images/kernel/nxp/choose_xxx_defconfig.png)

   在kernel，没有 `CONFIG_SOC_MX6ULL` 或者 `CONFIG_SOC_IMX6ULL` 选项，说明都被合并到了 `CONFIG_SOC_MX6UL`选项中。

   这里我们对比 `imx_v6_v7_defconfig` 和 `imx_v7_defconfig` 大多数还是SOC适配上少了几个型号，并不是所谓配置上有所差异。下面的 `multi_v7_defconfig` 这名字一看就是多核心（multiple：数量多的）的SOC才需要选用该配置。因此，**笔者这里用的 `imx_v7_defconfig` **。

3. `make menuconfig` ： `make menuconfig` 是用于内核功能裁剪的，毕竟 `xxx_defconfig` 只有基础配置，如果需要加入新的功能可以通过 `make menuconfig` 命令来添加对应的新配置。读者如果只需要默认配置即可满足开发需求的情况下，这条命令是可以省略的。

4. `make -j$(nproc)` ：多线程编译kernel。

   ![image](../../images/kernel/nxp/build_kernel_success_log.png)

编译完成以后就会在  `arch/arm/boot` 这个目录下生成一个叫做 `zImage` 的文件，`zImage` 就是我们要用的 Linux 镜像文件。另外也会在 `arch/arm/boot/dts` 下生成很多.dtb 文件，这些.dtb 就是设备树文件。

## Linux 工程目录分析

下面描述的文件夹均在编译kernel后的文件夹。

kernel中重要的文件夹或文件的含义如下所示：

**文件夹：**

| 文件夹        | 描述                              | 备注       |
| ------------- | --------------------------------- | ---------- |
| arch          | 架构相关目录。                    | Linux 自带 |
| block         | 块设备相关目录。                  | Linux 自带 |
| crypto        | 加密相关目录。                    | Linux 自带 |
| Documentation | 文档相关目录。                    | Linux 自带 |
| drivers       | 驱动相关目录。                    | Linux 自带 |
| firmeare      | 固件相关目录。                    | Linux 自带 |
| fs            | 文件系统相关目录。                | Linux 自带 |
| include       | 头文件相关目录。                  | Linux 自带 |
| init          | 初始化相关目录。                  | Linux 自带 |
| ipc           | 进程间通信相关目录。              | Linux 自带 |
| kernel        | 内核相关目录。                    | Linux 自带 |
| lib           | 库相关目录。                      | Linux 自带 |
| mm            | 内存管理相关目录。                | Linux 自带 |
| net           | 网络相关目录。                    | Linux 自带 |
| samples       | 例程相关目录。                    | Linux 自带 |
| scripts       | 脚本相关目录。                    | Linux 自带 |
| security      | 安全相关目录。                    | Linux 自带 |
| sound         | 音频处理相关目录。                | Linux 自带 |
| usr           | 与 initramfs 相关的目录，用于生成 | Linux 自带 |
| virt          | 提供虚拟机技术(KVM)。             | Linux 自带 |

**文件：**

| 文件                      | 描述                                      | 备注           |
| ------------------------- | ----------------------------------------- | -------------- |
| .config                   | Linux 最终使用的配置文件。                | 编译生成的文件 |
| .gitignore                | git 工具相关文件。                        | Linux 自带     |
| .mailmap                  | 邮件列表。                                | Linux 自带     |
| .missing-syscalls.d       |                                           | 编译生成的文件 |
| .tmp_xx                   |                                           | 编译生成的文件 |
| .version                  | 和版本有关。                              | 编译生成的文件 |
| .vmlinux.cmd              | cmd 文件，用于连接生成 vmlinux。          | 编译生成的文件 |
| COPYING                   | 版权声明。                                | Linux 自带     |
| CREDITS                   | Linux 贡献者。                            | Linux 自带     |
| Kbuild                    | Makefile 会读取此文件。                   | Linux 自带     |
| Kconfig                   | 图形化配置界面的配置文件。                | Linux 自带     |
| MAINTAINERS               | 维护者名单。                              | Linux 自带     |
| Makefile                  | Linux 顶层 Makefile                       | Linux 自带     |
| Module.xx<br />modules.xx | 一系列文件，和模块有关。                  | 编译生成的文件 |
| README                    | Linux 描述文件。                          | Linux 自带     |
| REPORTING-BUGS            | BUG 上报指南。                            | Linux 自带     |
| System.map                | 符号表。                                  | 编译生成的文件 |
| vmlinux                   | 编译出来的、未压缩的 ELF 格式Linux 文件。 | 编译生成的文件 |
| vmlinux.o                 | 编译出来的 vmlinux.o 文件。               | 编译生成的文件 |
|                           |                                           |                |
|                           |                                           |                |

#### .missing-syscalls.d是什么文件

这个问题问得很细 👍。我来详细解释一下：

------

##### `.missing-syscalls.d` 文件是什么

在 Linux kernel 的构建过程中（尤其是 **syscall 相关代码的生成**环节），内核会根据 **syscall 定义表**（如 `arch/arm/tools/syscall.tbl`、`arch/x86/entry/syscalls/syscall_64.tbl` 等）生成一系列中间文件，其中就包括 **`.missing-syscalls.d`**。

它的作用是：

1. ###### **检查遗漏的系统调用声明**

   - 内核通过脚本（例如 `scripts/syscalltbl.sh` / `scripts/syscallhdr.sh`）根据 `syscall.tbl` 自动生成头文件（如 `unistd.h`、`syscall.h`）。
   - 构建系统会顺带生成 `.missing-syscalls.d`，记录 **哪些 syscall 在表里有定义，但源码中没有对应实现（或没有正确声明）**。

2. ###### **Makefile 依赖文件**

   - 这是一个 **Make 依赖文件（\*.d）**，类似 `.depend` 文件。
   - 它会被内核 `Makefile` include，用来提示“缺少 syscall 实现”，从而在编译阶段给出 **编译错误或警告**，而不是等到运行时才发现。

------

##### 举个例子

在 `arch/arm/tools/syscall.tbl` 里加了一条新 syscall：

```
400   common   my_syscall   sys_my_syscall
```

- 如果你没在 `kernel/` 或 `arch/arm/kernel/` 下实现 `sys_my_syscall`，
- 编译时脚本会生成 `.missing-syscalls.d`，里面会标注这个 syscall 没有实现，
- 编译就会报错，提示你去补齐。

------

##### 为什么是 `.d` 后缀？

- `*.d` 文件在内核和 GCC 世界里通常表示 **dependency file（依赖文件）**。
- 它们不会进入最终产物（zImage、modules），但在编译过程中用来追踪依赖、缺失项。
- `.missing-syscalls.d` 属于这种依赖追踪文件。

------

##### 总结

- `.missing-syscalls.d` 是 **内核构建过程自动生成的依赖文件**。
- 用于检查 **syscall.tbl 定义和源码实现是否一致**。
- 如果里面有内容，说明你在 **syscall 表里声明了一个系统调用，但没有在内核代码里提供对应实现**。

------

要不要我帮你展示一份 **真实的 `.missing-syscalls.d` 文件内容示例**（比如在 `make ARCH=arm` 时生成的）？这样你能更直观看懂它的作用。



#### 编译生成的.tmp_xxx 文件作用

好问题 👍。
你在编译 **Linux kernel / U-Boot / BusyBox** 这类大工程时，确实会经常看到一堆形如 **`.tmp_xxx`** 的文件。

---

##### 1. `.tmp_xxx` 文件的本质

这些文件都是 **构建过程中生成的“临时产物”**，它们不会进入最终的内核镜像，而是用来做：

* **中间结果存放**（例如临时链接的 ELF 文件）
* **校验 / 比较**（防止无意义的重复编译）
* **生成最终目标前的过渡文件**

所以它们的存在完全是为了 **Makefile 构建系统的稳定性**。

---

##### 2. 常见几类 `.tmp_xxx`

###### (1) `.tmp_vmlinux*`

* 例如：`.tmp_vmlinux1`、`.tmp_vmlinux2`、`.tmp_vmlinux3`
* 作用：

  * **阶段性链接产物**，用来逐步合并内核各个目标文件（`built-in.o`、驱动、子系统等）。
  * 构建系统会对 `.tmp_vmlinux*` 做符号表解析，生成 **`System.map`** 和最终的 `vmlinux`。
  * 如果直接出错，说明链接脚本或符号解析有问题。

###### (2) `.tmp_System.map`

* 临时生成的符号表文件。
* 和已有的 `System.map` 比较，如果不同就更新，保证符号表和实际内核一致。
* 这样做可以避免“反复重编译但 System.map 没变化”的情况。

###### (3) `.tmp_kallsyms*`

* 内核为了支持 **kallsyms**（运行时符号表，用于 `printk` 堆栈解析等），会生成中间文件 `.tmp_kallsyms1.o`、`.tmp_kallsyms2.o`。
* 这些文件就是多次嵌套编译的临时目标。

###### (4) `.tmp_*.[o|d]`

* 某些子目录里你会看到 `.tmp_xxx.o`，这是 **编译器/链接器生成的中间目标文件**，在确认无误后才会被“移动/复制”为正式产物。
* `.tmp_xxx.d` 则是 **依赖文件**（类似 `.missing-syscalls.d`），记录 `.c` 源文件依赖了哪些头文件，方便 `make` 进行增量编译。

---

##### 3. 为什么要用 `.tmp_` 前缀？

* 表明是 **临时文件**，不会被最终安装。
* 防止和最终产物重名。
* Makefile 里通常会写类似：

```make
.tmp_vmlinux1: $(OFILES)
    $(LD) -o $@ $(OFILES)
```

最后一步再 `mv .tmp_vmlinux1 vmlinux`，保证编译原子性。

---

##### 4. 总结

* **`.tmp_xxx` = 临时文件**，由内核构建系统自动生成。
* 用途包括：

  * 逐步链接（`.tmp_vmlinux*`）
  * 校验/比较（`.tmp_System.map`）
  * 生成符号表（`.tmp_kallsyms*`）
  * 依赖追踪（`.tmp_xxx.d`）
* 编译成功后它们可以安全删除（通常 `make clean` 会清理）。

---

要不要我帮你画一张 **内核编译过程中 .tmp 文件如何演变成最终 vmlinux/zImage 的流程图（Mermaid 格式）**？

## 添加自己的开发板默认配置文件

将  `arch/arm/configs` 目 录 下 的 `imx_v7_defconfig` 重 新 复 制 一 份 ， 命 名 为 `imx_v7_test_defconfig`。

### 解决 `imx/sdma/sdma-imx6q.bin` 报错问题

#### 在xxx_defconfig添加配置

在uboot阶段，我们需要配置 kconfig 添加板级支持，配置board支持的配置，但是在linux已经不需要这样做了。为什么？因为linux这里的所有驱动都将通过模块加载启动，通过设备树来描述硬件属性。所以官方的`xxx_defconfig` 里面就只有SOC的选项，甚至将IMX6ULL合并到了IMX6UL。

![image](../../images/kernel/nxp/kernel_soc_config.png)

那么这个文件就不需要做任何改动了吗？并不是，笔者这里要加入 `firmware` 中的 `sdma.bin` 的固件配置以防止它后面启动kernel的时候出现缺少 `sdma-imx6q.bin` 固件而报错。

在 `imx_v7_defconfig` 配置加入如下两行配置：

```makefile
CONFIG_EXTRA_FIRMWARE_DIR="firmware"
CONFIG_EXTRA_FIRMWARE="imx/sdma/sdma-imx6q.bin"
```

![image](../../images/kernel/nxp/kernel_sdma_bin_config.png)

#### 下载 `sdma-imx6q.bin` 固件

从 Linux-firmware 仓库获取文件：

- 官方仓库：
   https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git
- 路径：`imx/sdma/sdma-imx6q.bin`

**官方下载链接（直接获取单文件）**

Linux-firmware 仓库在 kernel.org，文件路径就是：

```
imx/sdma/sdma-imx6q.bin
```

你可以用 `wget` 或 `curl` 单独下载：

```shell
# wget 方式
wget https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/imx/sdma/sdma-imx6q.bin -O sdma-imx6q.bin

# curl 方式
curl -L https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/imx/sdma/sdma-imx6q.bin -o sdma-imx6q.bin
```

这样就会在当前目录得到一个 `sdma-imx6q.bin` 文件。

**存放位置**

进入内核源码目录：

```
cd linux-6.1.x
```

确保存在目录：

```
mkdir -p firmware/imx/sdma
```

把 `sdma-imx6q.bin` 文件放到这里：

```
linux-6.1.x/firmware/imx/sdma/sdma-imx6q.bin
```

## 添加开发板对应的设备树文件

### imx6ull-14x14-evk-emmc.dts 复制+重命名

添加适合正点原子 EMMC 版开发板的设备树文件，进入目录 `arch/arm/boot/dts`  中，复制一份 `imx6ull-14x14-evk-emmc.dts` ，然后将其重命名为 ` imx6ull-14x14-test-emmc.dts` 。

修改imx6ull-14x14-test-emmc.dts 引入内容为：

```dtb
// #include "imx6ull-14x14-evk.dts"
#include "imx6ull-14x14-test.dts"
```

![image](../../images/kernel/nxp/kernel_dtb_config_1.png)

### imx6ull-14x14-evk.dts 复制+重命名

复制一份 `imx6ull-14x14-evk.dts`，然后将其重命名为 `imx6ull-14x14-test.dts`。

同样只修改引入内容：

```dtb
/***********************************
 * #include "imx6ull.dtsi"
 * #include "imx6ul-14x14-evk.dtsi"
 **********************************/
 
#include "imx6ull-test.dtsi"
#include "imx6ul-14x14-test.dtsi"
```

![image](../../images/kernel/nxp/kernel_dtb_config_2.png)

### imx6ull.dtsi 复制+重命名

复制一份 `imx6ull.dtsi`，然后将其重命名为 `imx6ull-test.dtsi`。不做修改

### imx6ul-14x14-evk.dtsi 复制+重命名

复制一份 `imx6ul-14x14-evk.dtsi`，然后将其重命名为 `imx6ul-14x14-test.dtsi`。  这个文件是重点需要修改的。

## LCD适配

打开 `imx6ul-14x14-test.dtsi` 设备树文件，在该文件进行修改。

### lcdif 修改

lcdif的dtsi配置如下所示：

```dtb
&lcdif {
	assigned-clocks = <&clks IMX6UL_CLK_LCDIF_PRE_SEL>;
	assigned-clock-parents = <&clks IMX6UL_CLK_PLL5_VIDEO_DIV>;
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_lcdif_dat
		     &pinctrl_lcdif_ctrl>;
	display = <&display0>;
	status = "okay";

	display0: display@0 {
		bits-per-pixel = <16>;
		bus-width = <24>;

		display-timings {
			native-mode = <&timing0>;

			timing0: timing0 {
				clock-frequency = <31000000>;
				hactive = <800>;
				vactive = <480>;
				hfront-porch = <40>;
				hback-porch = <88>;
				hsync-len = <48>;
				vback-porch = <32>;
				vfront-porch = <13>;
				vsync-len = <3>;
				hsync-active = <0>;
				vsync-active = <0>;
				de-active = <1>;
				pixelclk-active = <0>;
			};
		};
	};
};
```

![image](../../images/kernel/nxp/kernel_dtb_lcd_config_1.png)

### pinctrl 配置

pinctrl配置需要去掉复位引脚的配置，如uboot介绍阶段那样，笔者的开发板是没有lcd复位的。修改后的配置如下所示：

```dtb
pinctrl_lcdif_dat: lcdifdatgrp {
    fsl,pins = <
        MX6UL_PAD_LCD_DATA00__LCDIF_DATA00  0x49
        MX6UL_PAD_LCD_DATA01__LCDIF_DATA01  0x49
        MX6UL_PAD_LCD_DATA02__LCDIF_DATA02  0x49
        MX6UL_PAD_LCD_DATA03__LCDIF_DATA03  0x49
        MX6UL_PAD_LCD_DATA04__LCDIF_DATA04  0x49
        MX6UL_PAD_LCD_DATA05__LCDIF_DATA05  0x49
        MX6UL_PAD_LCD_DATA06__LCDIF_DATA06  0x49
        MX6UL_PAD_LCD_DATA07__LCDIF_DATA07  0x49
        MX6UL_PAD_LCD_DATA08__LCDIF_DATA08  0x49
        MX6UL_PAD_LCD_DATA09__LCDIF_DATA09  0x49
        MX6UL_PAD_LCD_DATA10__LCDIF_DATA10  0x49
        MX6UL_PAD_LCD_DATA11__LCDIF_DATA11  0x49
        MX6UL_PAD_LCD_DATA12__LCDIF_DATA12  0x49
        MX6UL_PAD_LCD_DATA13__LCDIF_DATA13  0x49
        MX6UL_PAD_LCD_DATA14__LCDIF_DATA14  0x49
        MX6UL_PAD_LCD_DATA15__LCDIF_DATA15  0x49
        MX6UL_PAD_LCD_DATA16__LCDIF_DATA16  0x49
        MX6UL_PAD_LCD_DATA17__LCDIF_DATA17  0x49
        MX6UL_PAD_LCD_DATA18__LCDIF_DATA18  0x49
        MX6UL_PAD_LCD_DATA19__LCDIF_DATA19  0x49
        MX6UL_PAD_LCD_DATA20__LCDIF_DATA20  0x49
        MX6UL_PAD_LCD_DATA21__LCDIF_DATA21  0x49
        MX6UL_PAD_LCD_DATA22__LCDIF_DATA22  0x49
        MX6UL_PAD_LCD_DATA23__LCDIF_DATA23  0x49
    >;
};

pinctrl_lcdif_ctrl: lcdifctrlgrp {
    fsl,pins = <
        MX6UL_PAD_LCD_CLK__LCDIF_CLK	    0x79
        MX6UL_PAD_LCD_ENABLE__LCDIF_ENABLE  0x79
        MX6UL_PAD_LCD_HSYNC__LCDIF_HSYNC    0x79
        MX6UL_PAD_LCD_VSYNC__LCDIF_VSYNC    0x79
        /* used for lcd reset */
        /* MX6UL_PAD_SNVS_TAMPER9__GPIO5_IO09  0x79 */
    >;
};
```

![image](../../images/kernel/nxp/kernel_dtb_lcd_config_2.png)

## 网络适配

打开 `imx6ul-14x14-test.dtsi` 设备树文件，在该文件进行修改。

### 修改fec节点

```dtb
&fec1 {
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_enet1
				 &pinctrl_enet1_reset>;	// 添加复位控制引脚
	phy-mode = "rmii";
	phy-handle = <&ethphy0>;
	phy-supply = <&reg_peri_3v3>;
	// 添加这些 -----------------------------------------
	phy-reset-gpios = <&gpio5 7 GPIO_ACTIVE_LOW>;
	phy-reset-duration = <200>;
	phy-reset-post-delay = <200>;
	// 到这里 -----------------------------------------
	status = "okay";
};

&fec2 {
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_enet2
				 &pinctrl_enet2_reset>;	// 添加复位控制引脚
	phy-mode = "rmii";
	phy-handle = <&ethphy1>;
	phy-supply = <&reg_peri_3v3>;
	// 添加这些 -----------------------------------------
	phy-reset-gpios = <&gpio5 8 GPIO_ACTIVE_LOW>;
	phy-reset-duration = <200>;
	phy-reset-post-delay = <200>;
	// 到这里 -----------------------------------------
	status = "okay";

	mdio {
		#address-cells = <1>;
		#size-cells = <0>;

		ethphy0: ethernet-phy@0 {
			compatible = "ethernet-phy-id0007.c0f0";
			reg = <0>;
			smsc,led-mode = <1>;
			clocks = <&clks IMX6UL_CLK_ENET_REF>;
			clock-names = "rmii-ref";

		};

		ethphy1: ethernet-phy@1 {
			compatible = "ethernet-phy-id0007.c0f0";
			reg = <1>;
			smsc,led-mode = <1>;
			clocks = <&clks IMX6UL_CLK_ENET2_REF>;
			clock-names = "rmii-ref";
		};
	};
};
```

### 屏蔽spi-4的冲突引脚

```dtb
spi-4 {
    compatible = "spi-gpio";
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_spi4>;
    status = "disabled";
    gpio-sck = <&gpio5 11 0>;
    gpio-mosi = <&gpio5 10 0>;
    /* cs-gpios = <&gpio5 7 GPIO_ACTIVE_LOW>; */
    num-chipselects = <1>;
    #address-cells = <1>;
    #size-cells = <0>;

    gpio_spi: gpio@0 {
        compatible = "fairchild,74hc595";
        gpio-controller;
        #gpio-cells = <2>;
        reg = <0>;
        registers-number = <1>;
        registers-default = /bits/ 8 <0x57>;
        spi-max-frequency = <100000>;
        /* enable-gpios = <&gpio5 8 GPIO_ACTIVE_LOW>; */
    };
};
```

```dtb
pinctrl_spi4: spi4grp {
    fsl,pins = <
        MX6UL_PAD_BOOT_MODE0__GPIO5_IO10	0x70a1
        MX6UL_PAD_BOOT_MODE1__GPIO5_IO11	0x70a1
        /* 
        MX6UL_PAD_SNVS_TAMPER7__GPIO5_IO07	0x70a1
        MX6UL_PAD_SNVS_TAMPER8__GPIO5_IO08	0x80000000
        */
    >;
};
```

现在就基本完成了，由于笔者的phy芯片是LAN8720A芯片。如果读者同样的芯片在kernel阶段初始化网口失败，那么可以在 `drivers/net/phy/phy_device.c` 文件加入软复位。如下所示：

![image](../../images/kernel/nxp/kernel_net_reset_sw.png)

### 添加开发板对应的设备树文件

配置好设备树文件之后，还需要修改文件 `arch/arm/boot/dts/Makefile`，找到 `dtb-$(CONFIG_SOC_IMX6ULL)` 配置项，在此配置项中加入 `imx6ull-14x14-test-emmc.dtb`，如下所示：

![image](../../images/kernel/nxp/kernel_add_dtb_into_makefile.png)

### 添加smsc公司phy配置

向 `arch/arm/configs/imx_v7_test_defconfig` 文件添加 `CONFIG_SMSC_PHY=y` 配置，同时将 `CONFIG_SMSC_PHY=y` 注释掉。

```shell 
# CONFIG_MICREL_PHY=y
CONFIG_SMSC_PHY=y
```

结果如图所示：

![image](../../images/kernel/nxp/add_smsc_phy_config.png)



## 修改cpu频率为performance

### menuconfig配置

在kernel根目录，进行如下命令进入menuconfig配置cpu power 策略：

```
make distclean
make imx_v7_test_defconfig
make menuconfig
```

进入menuconfig后，进行如下配置，将cpu主频设置在高性能模式：

![image](../../images/kernel/nxp/kernel_performance_config_1.png)

![image](../../images/kernel/nxp/kernel_performance_config_2.png)

![image](../../images/kernel/nxp/kernel_performance_config_3.png)

![image](../../images/kernel/nxp/kernel_performance_config_4.png)

设置之后将配置进行保存，它会自动更新到 `.config` 文件中：

![image](../../images/kernel/nxp/kernel_performance_config_5.png)

### 将配置到imx_v7_test_defconfig

将 `arch/arm/configs/imx_v7_test_defconfig` 文件打开，我们从 `.config` 配置中将文件中的配置复制到 `arch/arm/configs/imx_v7_test_defconfig` ：

![image](../../images/kernel/nxp/kernel_performance_config_6.png)

然后重新编译即可更新cpu主频：

![image](../../images/kernel/nxp/kernel_performance_config_7.png)

# 根文件系统构建

## 根文件系统简介

根文件系统一般也叫做 rootfs，那么什么叫根文件系统？看到“文件系统”这四个字，很多人，包括我第一反应就是 FATFS、FAT、EXT4、YAFFS 和 NTFS 等这样的文件系统。在这里，根文件系统并不是 FATFS 这样的文件系统代码，EXT4 这样的文件系统代码属于 Linux 内核的一部分。Linux 中的根文件系统更像是一个文件夹或者叫做目录(在我看来就是一个文件夹，只不过是特殊的文件夹)，在这个目录里面会有很多的子目录。根目录下和子目录中会有很多的文件，这些文件是 Linux 运行所必须的，比如库、常用的软件和命令、设备文件、配置文件等等。以后我们说到文件系统，如果不特别指明，统一表示根文件系统。

百度百科上说内核代码镜像文件保存在根文件系统中，但是我们嵌入式 Linux 并没有将内核代码镜像保存在根文件系统中，而是保存到了其他地方。比如 NAND Flash 的指定存储地址、EMMC 专用分区中。根文件系统是 Linux 内核启动以后挂载(mount)的第一个文件系统，然后从根文件系统中读取初始化脚本，比如 rcS，inittab 等。根文件系统和 Linux 内核是分开的，单独的 Linux 内核是没法正常工作的，必须要搭配根文件系统。如果不提供根文件系统，Linux 内核在启动的时候就会提示内核崩溃(Kernel panic)的提示。

根文件系统的这个“根”字就说明了这个文件系统的重要性，它是其他文件系统的根，没有这个“根”，其他的文件系统或者软件就别想工作。比如我们常用的 ls、mv、ifconfig 等命令其实就是一个个小软件，只是这些软件没有图形界面，而且需要输入命令来运行。这些小软件就保存在根文件系统中，这些小软件是怎么来的呢？这个就是我们本章教程的目的，教大家来构建自己的根文件系统，这个根文件系统是满足 Linux 运行的最小根文件系统，后续我们可以根据自己的实际工作需求不断的去填充这个最小根文件系统，最终使其成为一个相对完善的根文件系统。

在构建根文件系统之前，我们先来看一下根文件系统里面大概都有些什么内容，以 Ubuntu为例，根文件系统的目录名字为‘/’，没看错就是一个斜杠，所以输入 `cd /`就可以进入根目录中。根目录下子目录和文件不少，但是这些都是 Ubuntu 所需要的，其中有很多子目录和文件我们嵌入式 Linux 是用不到的，所以这里就讲解一些常用的子目录：

| 目录名 | 描述                                                         |
| ------ | ------------------------------------------------------------ |
| /bin   | 看到“bin”大家应该能想到 bin 文件，bin 文件就是可执行文件。所以此目录下存放着系统需要的可执行文件，一般都是一些命令，比如 ls、mv 等命令。此目录下的命令所有的客户都可以使用。 |
| /dev   | dev 是 device 的缩写，所以此目录下的文件都是和设备有关的，此目录下的文件都是设备文件。<br /><br />在 Linux 下一切皆文件，即使是硬件设备，也是以文件的形式存在的，比如/dev/ttymxc0(I.MX6ULL 根目录会有此文件)就表示 I.MX6ULL 的串口 0，我们要想通过串口 0 发送或者接收数据就要操作文件/dev/ttymxc0，通过对文件/dev/ttymxc0 的读写操作来实现串口0 的数据收发。 |
| /etc   | 此目录下存放着各种配置文件，大家可以进入 Ubuntu 的 etc 目录看一下，里面的配置文件非常多！但是在嵌入式 Linux 下此目录会很简洁。 |
| /lib   | lib 是 library 的简称，也就是库的意思，因此此目录下存放着 Linux 所必须的库文件。这些库文件是共享库，命令和用户编写的应用程序要使用这些库文件。 |
| /proc  | 此目录一般是空的，当 Linux 系统启动以后会将此目录作为 proc 文件系统的挂载点，proc是个虚拟文件系统，没有实际的存储设备。proc 里面的文件都是临时存在的，一般用来存储系统运行信息文件。 |
| /usr   | 要注意，usr 不是 user 的缩写，而是 Unix Software Resource 的缩写，也就是 Unix 操作系统软件资源目录。这里有个小知识点，那就是 Linux 一般被称为类 Unix 操作系统，苹果的 MacOS也是类 Unix 操作系统。关于 Linux 和 Unix 操作系统的渊源大家可以直接在网上找 Linux 的发展历史来看。既然是软件资源目录，因此/usr 目录下也存放着很多软件，一般系统安装完成以后此目录占用的空间最多。 |
| /var   | 此目录存放一些可以改变的数据。                               |
| /sbin  | 此目录页用户存放一些可执行文件，但是此目录下的文件或者说命令只有管理员才能使用，主要用户系统管理。 |
| /sys   | 系统启动以后此目录作为 sysfs 文件系统的挂载点，sysfs 是一个类似于 proc 文件系统的特殊文件系统，sysfs 也是基于 ram 的文件系统，也就是说它也没有实际的存储设备。此目录是系统设备管理的重要目录，此目录通过一定的组织结构向用户提供详细的内核数据结构信息。 |
| /opt   | 可选的文件、软件存放区，由用户选择将哪些文件或软件放到此目录中。<br /><br />关于 Linux 的根目录就介绍到这里，接下来的构建根文件系统就是研究如何创建上面这些子目录以及子目录中的文件。 |

## 获取 BusyBox 根文件系统

BusyBox 可以在其官网下载到，官网地址为：https://busybox.net/。下面是下载指引：

![image](../../images/rootfs/get_rootfs_source.png)

笔者这里采用的是比较新的版本：https://busybox.net/downloads/busybox-snapshot.tar.bz2

![image](../../images/rootfs/busybox_version.png)

将文件下载好，解压缩，开始配置根文件系统。

## 配置 Makefile

打开 busybox 的顶层 Makefile，添加 ARCH 和 CROSS_COMPILE 的值，如下所示：

```makefile
ARCH ?= arm
CROSS_COMPILE ?= arm-none-linux-gnueabihf-
CONFIG_PREFIX ?= /home/lizhaojun/nfs/rootfs
```

这里出现 `CONFIG_PREFIX` 表示下载位置的前缀，在正点原子文档中，配置make install 之后都要配置它。笔者觉得非常麻烦，就干脆写在顶层Makefile中了。

![image](../../images/rootfs/compiler_config.png)

## **busybox** 中文字符支持

修改文件 `libbb/printable_string.c`，找到函数 `printable_string2()`，修改后的函数内容如下 ：

```c
const char* FAST_FUNC printable_string2(uni_stat_t *stats, const char *str)
{
	char *dst;
	const char *s;

	s = str;
	while (1) {
		unsigned char c = *s;
		if (c == '\0') {
			/* 99+% of inputs do not need conversion */
			if (stats) {
				stats->byte_count = (s - str);
				stats->unicode_count = (s - str);
				stats->unicode_width = (s - str);
			}
			return str;
		}
		if (c < ' ')
			break;
		/* 注释掉下面这两行代码 */
		/*
		if (c >= 0x7f)
			break;
		*/
		s++;
	}

#if ENABLE_UNICODE_SUPPORT
	dst = unicode_conv_to_printable(stats, str);
#else
	{
		char *d = dst = xstrdup(str);
		while (1) {
			unsigned char c = *d;
			if (c == '\0')
				break;
			/* if (c < ' ' || c >= 0x7f) */
			if (c < ' ')
				*d = '?';
			d++;
		}
		if (stats) {
			stats->byte_count = (d - dst);
			stats->unicode_count = (d - dst);
			stats->unicode_width = (d - dst);
		}
	}
#endif
	return auto_string(dst);
}
```

修改文件 `libbb/unicode.c`，找到 `unicode_conv_to_printable2()` 函数，修改后的内容如下所示：

```c
static char* FAST_FUNC unicode_conv_to_printable2(uni_stat_t *stats, const char *src, unsigned width, int flags)
{
	char *dst;
	unsigned dst_len;
	unsigned uni_count;
	unsigned uni_width;

	if (unicode_status != UNICODE_ON) {
		char *d;
		if (flags & UNI_FLAG_PAD) {
			d = dst = xmalloc(width + 1);
			while ((int)--width >= 0) {
				unsigned char c = *src;
				if (c == '\0') {
					do
						*d++ = ' ';
					while ((int)--width >= 0);
					break;
				}
				/* *d++ = (c >= ' ' && c < 0x7f) ? c : '?'; */
				*d++ = (c >= ' ') ? c : '?';
				src++;
			}
			*d = '\0';
		} else {
			d = dst = xstrndup(src, width);
			while (*d) {
				unsigned char c = *d;
				/* if (c < ' ' || c >= 0x7f) */
				if (c < ' ')
					*d = '?';
				d++;
			}
		}
		if (stats) {
			stats->byte_count = (d - dst);
			stats->unicode_count = (d - dst);
			stats->unicode_width = (d - dst);
		}
		return dst;
	}

	dst = NULL;
	uni_count = uni_width = 0;
	dst_len = 0;
	while (1) {
		int w;
		wchar_t wc;

#if ENABLE_UNICODE_USING_LOCALE
		{
			mbstate_t mbst = { 0 };
			ssize_t rc = mbsrtowcs(&wc, &src, 1, &mbst);
			/* If invalid sequence is seen: -1 is returned,
			 * src points to the invalid sequence, errno = EILSEQ.
			 * Else number of wchars (excluding terminating L'\0')
			 * written to dest is returned.
			 * If len (here: 1) non-L'\0' wchars stored at dest,
			 * src points to the next char to be converted.
			 * If string is completely converted: src = NULL.
			 */
			if (rc == 0) /* end-of-string */
				break;
			if (rc < 0) { /* error */
				src++;
				goto subst;
			}
			if (!iswprint(wc))
				goto subst;
		}
#else
		src = mbstowc_internal(&wc, src);
		/* src is advanced to next mb char
		 * wc == ERROR_WCHAR: invalid sequence is seen
		 * else: wc is set
		 */
		if (wc == ERROR_WCHAR) /* error */
			goto subst;
		if (wc == 0) /* end-of-string */
			break;
#endif
		if (CONFIG_LAST_SUPPORTED_WCHAR && wc > CONFIG_LAST_SUPPORTED_WCHAR)
			goto subst;
		w = wcwidth(wc);
		if ((ENABLE_UNICODE_COMBINING_WCHARS && w < 0) /* non-printable wchar */
		 || (!ENABLE_UNICODE_COMBINING_WCHARS && w <= 0)
		 || (!ENABLE_UNICODE_WIDE_WCHARS && w > 1)
		) {
 subst:
			wc = CONFIG_SUBST_WCHAR;
			w = 1;
		}
		width -= w;
		/* Note: if width == 0, we still may add more chars,
		 * they may be zero-width or combining ones */
		if ((int)width < 0) {
			/* can't add this wc, string would become longer than width */
			width += w;
			break;
		}

		uni_count++;
		uni_width += w;
		dst = xrealloc(dst, dst_len + MB_CUR_MAX);
#if ENABLE_UNICODE_USING_LOCALE
		{
			mbstate_t mbst = { 0 };
			dst_len += wcrtomb(&dst[dst_len], wc, &mbst);
		}
#else
		dst_len += wcrtomb_internal(&dst[dst_len], wc);
#endif
	}

	/* Pad to remaining width */
	if (flags & UNI_FLAG_PAD) {
		dst = xrealloc(dst, dst_len + width + 1);
		uni_count += width;
		uni_width += width;
		while ((int)--width >= 0) {
			dst[dst_len++] = ' ';
		}
	}
	if (!dst) /* for example, if input was "" */
		dst = xzalloc(1);
	dst[dst_len] = '\0';
	if (stats) {
		stats->byte_count = dst_len;
		stats->unicode_count = uni_count;
		stats->unicode_width = uni_width;
	}

	return dst;
}
```

## **配置** busybox

**注意：以下的任何图片都是已配置后的结果。因此，读者对照配置的时候，只需要观察是否配置一致即可。**

我们一般使用默认配置即可，因此使用如下命令先使用默认配置来配置一下 busybox：

```shell
make defconfig
```

busybox 也支持图形化配置，通过图形化配置我们可以进一步选择自己想要的功能，输入如下命令打开图形化配置界面：

```shell
make menuconfig
```

打开以后如图所示：

![image](../../images/rootfs/rootfs_config_1.png)

配置路径如下：

```txt
Location: 
	-> Settings 
		-> Build static binary (no shared libs)
```

选项 `Build static binary (no shared libs)` 用来决定是静态编译 busybox 还是动态编译，静态编译的话就不需要库文件，但是编译出来的库会很大。动态编译的话要求根文件系统中有库文件，但是编译出来的 busybox 会小很多。这里我们不能采用静态编译！因为采用静态编译的话 DNS 会出问题！无法进行域名解析，配置如图所示：

![image](../../images/rootfs/rootfs_config_2.png)

继续配置如下路径配置项：

```txt
Location: 
	-> Linux Module Utilities
		-> Simplified modutils
```

结果如图所示：

![image](../../images/rootfs/rootfs_config_3.png)

![image](../../images/rootfs/rootfs_config_4.png)

继续配置如下路径配置项：

```txt
Location: 
	-> Linux System Utilities 
		-> mdev (16 kb) //确保下面的全部选中，默认都是选中的
```

结果如图所示：

![image](../../images/rootfs/rootfs_config_5.png)

![image](../../images/rootfs/rootfs_config_6.png)

最后就是使能 busybox 的 unicode 编码以支持中文，配置路径如下：

```txt
Location: 
	-> Settings
		-> Support Unicode 												//选中
		-> Check $LC_ALL, $LC_CTYPE and $LANG environment variables //选中
```

结果如图所示：

![image](../../images/rootfs/rootfs_config_7.png)

busybox 的配置就到此结束了，大家也可以根据自己的实际需求选择配置其他的选项，不过对于初学者笔者不建议再做其他的修改，可能会出现编译出错的情况发生。

## 编译busybox

将.config文件复制保存一份备份，要是不小心执行 `make distclean` 可就要单独再配置一遍了。

配置好 busybox 以后就可以编译了，我们可以指定编译结果的存放目录，我们肯定要将编译结果存放到前面创建的 rootfs 目录中，输入如下命令：

```shell
make 
make install
```

在正点原子案例中有 `make install CONFIG_PREFIX=/home/zuozhongkai/linux/nfs/rootfs`，笔者这里已经将 `CONFIG_PREFIX` 配置更新在了Makefile中，所以这里直接执行`make install` 即可。`COFIG_PREFIX` 指定编译 结果的存放目录。

## 向根文件系统添加 lib 库

一个完整的rootfs需要有如下文件夹：

```txt
bin  dev  etc  lib  mnt  proc  root  run  sbin  sys  tmp  usr
```

以及一个 `linuxrc` 文件。

**如果在下面的配置过程中，读者发现存在没有出现的目录，请手动创建目录。**

读者配置完根文件系统，在 `CONFIG_PREFIX` 指定的目录下是没有这么多文件夹的，这些文件夹是笔者手动创建，这也是进行配置根文件系统的关键步骤。

笔者的交叉编译器的安装位置为： `/usr/local/cross_compiler/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf`。

**注意：**

> 笔者准备在后文中做一些特殊位置的变量替换进行讲解，读者需要知道它是读者自己的环境位置，与笔者的位置无关，只需要进行参照印证即可。
>
> * $(cross_compile)
>   * 代替笔者交叉编译器的位置 `/usr/local/cross_compiler/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/arm-none-linux-gnueabihf` ，既简介也方便阅读。而读者可以参照自己配置的交叉编译器的位置来进行替换。
> * $(root_fs) 
>   * 代替笔者的根文件下载目录：`/home/lizhaojun/nfs/rootfs`。也就是 `CONFIG_PREFIX` 的值

### 复制$(cross_compile)/libc/lib

进入 $(root_fs)/lib 目录中，拷贝 `$(cross_compile)/libc/lib` 的\*.so\*  和 \*.a\* 等库文件。

```shell
cp *.so.* *.a* $(cross_compile)/libc/lib -d
```

拷贝完成后，观察$(root_fs)/lib目录中的文件，执行 `ls -al` 将文件都列举出来， `ld-linux-armhf.so.3` 是一个软链接文件，它链接到 `ld-2.33.so ` 文件，接下来进行如下操作：

1. 那么就地将 `ld-2.33.so `文件复制一份。
2. 将 `ld-linux-armhf.so.3` 文件删除。
3. 将复制的 `ld-2.33.so` 文件副本重名为  `ld-linux-armhf.so.3` 。

正点原子这里的解释是 `ld-linux-armhf.so.3` 不能是软链接，不过笔者这里未曾印证，读者可以试着保留它的软链接属性试试是否可以运行成功。

### 复制$(cross_compile)/lib

进入 `$(root_fs)/lib` 目录中，`$(cross_compile)/lib` 目录下也有很多的的 \*so\* 和 \*.a\* 库文件，我们将其也拷贝到 `$(root_fs)/lib` 目录中，命令如下：

```shell
cp *.so.* *.a* $(cross_compile)/lib -d
```

$(root_fs)/lib 目录的库文件就这些了，完成以后的 $(root_fs)/lib目录如下所示

![image](../../images/rootfs/rootfs_lib_files.png)

### 向$(root_fs)/usr/lib 目录添加库文件

在 $(root_fs) 的 usr 目录下创建一个名为 lib 的目录，进入该目录，将如下目录中的库文件拷贝到 $(root_fs)/usr/lib 目录下：

```shell
cp *.so* *.a* $(cross_compile)/libc/usr/lib -d
```

完成以后的 rootfs/usr/lib 目录如图所示：

![image](../../images/rootfs/rootfs_usr_lib_files.png)至此，根文件系统的库文件就全部添加好了。

### 解决无 rcS 文件的错误与hotplug错误

在 $(root_fs)/etc/ 目录下创建 init.d 文件夹，并创建rcS：

```shell
mkdir $(root_fs)/etc/init.d
cd $(root_fs)/etc/init.d
vim rcS
```

在正点原子中使用了 `echo /sbin/mdev > /proc/sys/kernel/hotplug`，这是老旧内核才使用的热插拔方式，在新内核使用该方式会弹出找不到hotplug的错误提示。新内核都使用的mdev的冷插拔。

整个 rcS 文件内容如下：

```shell
#!/bin/sh
# rcS —— 系统启动脚本，BusyBox init 会在启动时调用它
# 目标：初始化基础环境（/proc、/sys、/dev 等），执行设备节点准备，最后根据 fstab 自动挂载 NFS 等目录。

# ====== 环境变量设置 ======
PATH=/sbin:/bin:/usr/sbin:/usr/bin
LD_LIBRARY_PATH=/lib:/usr/lib
export PATH LD_LIBRARY_PATH

# ====== 1. 让根文件系统可写 ======
# 某些情况下（initramfs 或只读挂载），根分区是只读的，需要重新挂成可写
mount -o remount,rw / 2>/dev/null || true

# ====== 2. 挂载内核伪文件系统 ======
# /proc    —— 提供进程信息、内核参数 (/proc/sys/... 等)
# /sys     —— sysfs，导出设备和驱动信息
# /dev     —— devtmpfs，内核会自动创建主设备节点
mount -t proc     proc     /proc   2>/dev/null || true
mount -t sysfs    sysfs    /sys    2>/dev/null || true
mount -t devtmpfs devtmpfs /dev    2>/dev/null || true

# ====== 3. 挂载 /dev/pts /run /tmp 等常用目录 ======
# /dev/pts —— 提供伪终端 (tty)
# /run     —— 存放运行时文件（pid 文件、socket）
# /tmp     —— 临时目录，必须设置 1777 权限
mkdir -p /dev/pts /run /tmp
mount -t devpts devpts /dev/pts 2>/dev/null || true
mount -t tmpfs  tmpfs  /run     2>/dev/null || true
mount -t tmpfs  tmpfs  /tmp     2>/dev/null || true
chmod 1777 /tmp

# ====== 4. 设备节点准备 ======
# mdev 是 BusyBox 提供的简化 udev 工具。
# - “mdev -s” 扫描 sysfs 并创建设备节点（冷插阶段）。
# - 不再需要 echo /sbin/mdev > /proc/sys/kernel/hotplug （现代内核已弃用）。
[ -x /sbin/mdev ] && mdev -s

# ====== 5. 根据 /etc/fstab 挂载其它文件系统 ======
# 包含 NFS（192.168.31.142:/home/lizhaojun/nfs → /mnt/nfs），
# 以及 /proc、/sys、/tmp 等都会统一处理。
mount -a

# ====== 6. （可选）初始化网络 ======
# 如果你在 fstab 里写了 NFS，最好保证网卡已启动。
# 这里演示使用 udhcpc 从 DHCP 获取地址。
NET_IF="eth0"   # 根据实际板子网卡名调整，比如 end0
if [ -d "/sys/class/net/$NET_IF" ]; then
    ip link set "$NET_IF" up 2>/dev/null || ifconfig "$NET_IF" up 2>/dev/null || true
    if command -v udhcpc >/dev/null 2>&1; then
        udhcpc -i "$NET_IF" -q -t 3 -T 3 2>/dev/null || true
    fi
fi

# ====== 7. （可选）启动其它服务 ======
# 如果你有更多服务脚本，可以放在 /etc/init.d/S*，用 run-parts 启动。
# run-parts /etc/init.d/S*

# ====== 结束 ======
exit 0
```

### 添加fstab文件

fstab 在 Linux 开机以后自动配置哪些需要自动挂载的分区。

在  $(root_fs)/etc/ 目录下，创建 fstab文件，执行如下命令完成：

```shell
cd $(root_fs)/etc/
vim fstab
```

fstab 文件内容如下所示：

```shell
# <file system> <mount point>  <type>  <options> 		<dump> <pass>
proc            /proc          proc    defaults  		0      0
sysfs           /sys           sysfs   defaults  		0      0
devpts          /dev/pts       devpts  gid=5,mode=620   0      0
tmpfs           /run           tmpfs   mode=0755        0      0
tmpfs           /tmp           tmpfs   mode=1777        0      0

# NFS 自动挂载 —— 开发板启动后自动挂载虚拟机目录
192.168.31.142:/home/lizhaojun/nfs /mnt/nfs nfs defaults,_netdev,nolock,vers=4 0 0
```

文件最后两行：笔者做了nfs文件的自动挂载操作。读者可以自行修改适配下，也可以删除最后两行，手动挂载nfs。

### 添加 inittab 文件

inittab 的详细内容可以参考 busybox 下的文件 examples/inittab。init 程序会读取 /etc/inittab 这个文件，inittab 由若干条指令组成。每条指令的结构都是一样的，由以“:”分隔的 4 个段组成

在  $(root_fs)/etc/ 目录下，创建 inittab 文件，执行如下命令完成：

```shell
cd $(root_fs)/etc/
vim inittab 
```

inittab 文件内容如下所示：

```shell
#etc/inittab
::sysinit:/etc/init.d/rcS
console::askfirst:-/bin/sh
::restart:/sbin/init
::ctrlaltdel:/sbin/reboot
::shutdown:/bin/umount -a -r
::shutdown:/sbin/swapoff -a
```

至此，整个kernel和根文件系统的配置都已经配置完成。

## u-boot 环境变量配置

开发板上进行复位，进入u-boot进行配置环境变量：

### bootcmd

```shell
=> setenv bootcmd "tftp 80800000 zImage;tftp 83000000 imx6ull-14x14-test-emmc.dtb;bootz 80800000 - 83000000"
=> saveenv
```

### bootargs

```shell
=> setenv bootargs "console=ttymxc0,115200 root=/dev/nfs nfsroot=192.168.31.142:/home/lizhaojun/nfs/rootfs,vers=3,proto=tcp rw ip=192.168.31.180:192.168.31.142:192.168.31.1:255.255.255.0::eth0:off"
=> saveenv
```

### 配置网络

配置说明

* 笔者的开发板配置的ip为：192.168.31.180。

* 笔者的ubuntu22的ip为：192.168.31.142

* 笔者这里是将开发板链接到路由器上的，只需要保证同一个网段即可。如果与笔记本或者台式电脑直连，读者需要设置虚拟机的网络桥接网络为复制本地网络状态。

  ![image](F:\git_storage\linux-kernel_and_driver_note\images\uboot\net_config_test.png)

  只有勾上它，才能够在同一个网卡上ping通。至于为什么要这么操作，这只是笔者的一个成功案例，并不是权威，仅做参考。

* 笔者网关为：192.168.31.1。网关不是都是这样的，它是可以根据组网需求配置的，注意仔细检查自己的环境。

* 笔者子网掩码：255.255.255.0。

* 笔者这里给网络配置了MAC 地址为 b8:ae:1d:01:00:00。可以根据读者环境自行配置。

```shell
setenv ipaddr 192.168.31.180
setenv ethaddr b8:ae:1d:01:00:00
setenv gatewayip 192.168.31.1
setenv netmask 255.255.255.0
setenv serverip 192.168.31.142
saveenv
```

u-boot执行boot命令，启动内核和根文件系统。并且fstab里面自动挂载了笔者的虚拟机nfs文件。

至此整个移植操作完结。

# buildroot 下载工具

首先声明：笔者这里将不会教怎么打包一体化采用buildroot来编译所有环境，只是在使用上述环境的时候如何为现有环境添加新kernel所适配的工具，以libgpiod工具为主要讲解方式。

## 下载buildroot

下载buildroot源码到ubuntu。[buildroot官网下载](https://buildroot.org/download.html)：![image](./../../images/buildroot/external_toolchain/buildroot_download.png)

## 清除环境中的配置

执行如下make命令清除本地配置残留：

```shell
make distclean
```



## 添加芯片架构的配置信息

**注意**：下面config的<span style="color: red">红色字体</span>部分表示要做的修改。



进入到buildroot源码目录，执行 `make menuconfig` shell命令，添加芯片架构的配置信息。我们采用的imx6ull，配置如下：![image](./../../images/buildroot/external_toolchain/buildroot_target_config.png)

进入 Target options:

* Target Architecture (<span style="color: red">ARM (little endian)</span>)
* Target Architecture Variant (<span style="color: red">cortex-A7</span>>)
* Target ABI (<span style="color: red">EABIhf</span>)
* Floating point strategy (<span style="color: red">VFPv3-D16</span>)
* ARM instruction set (<span style="color: red">ARM</span>)
* Target Binary Format (<span style="color: red">ELF</span>)



## 添加本地交叉编译器

进入 Toolchain，配置交叉编译器的位置，前缀，编译器属性等:![image](./../../images/buildroot/external_toolchain/buildroot_target_config_whole.png)

* Toolchain type (External toolchain)  --->  

  \*\*\*Toolchain External Options \*\*\*

* Toolchain (<span style="color: red">Custom toolchain</span>)  --->

* Toolchain origin (<span style="color: red">Pre-installed toolchain</span>)  --->

* (<span style="color: red">/home/lizhaojun/tools/cross_compiler/gcc-10.3/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf</span>)

* (<span style="color: red">arm-none-linux-gnueabihf</span>) Toolchain prefix

* External toolchain gcc version (<span style="color: red">10.x</span>)  --->

* External toolchain kernel headers series (<span style="color: red">4.20.x</span>)  --->

* External toolchain C library (<span style="color: red">glibc</span>)  --->

* [\*] Toolchain has SSP support?

* [\*]   Toolchain has SSP strong support?

* <span style="color: red">\[ ] </span>Toolchain has RPC support?

* <span style="color: red">[\*]</span> Toolchain has C++ support?

* \[ ] Toolchain has D support?

* <span style="color: red">\[\*]</span> Toolchain has Fortran support?

* <span style="color: red">\[*]</span> Toolchain has OpenMP support?

* \[ ] Copy gdb server to the Target

  \*\*\* Host GDB Options \*\*\*

* \[ ] Build cross gdb for the host

  \*\*\* Toolchain Generic Options \*\*\*

* \[ ] Copy gconv libraries

* ()  Extra toolchain libraries to be copied to target

* ()  Target Optimizations

* ()  Target linker options

* \*\*\* Bare metal toolchain \*\*\*

* \[ ] Build bare metal toolchains

## 取消linux kernel和uboot

现在要保证buildroot的选项里面没有配置linux kernel 和 uboot。

* Kernel 	默认为空
* Bootloader     默认为空



## 添加工具

以 libgpiod 工具为例。

进入：**Main menu → **

* **Target packages（目标软件包） → **
  * **Libraries（程序库） → **
    * **Hardware handling（硬件处理）**
      * libgpiod （勾选）

找到并勾选 **libgpiod**（或 **libgpiod2**，二选一，名字随 Buildroot 版本不同而不同，但是当前编译器的头文件仅为4.20.x，无法选 **libgpiod2**）：

- 原因：安装用户态访问 `/dev/gpiochip*` 的库。
- 目的：给工具提供基础库。

## 开始构建

执行 make 命令：

```shell
make -j$(nproc)
```

完成后，与你操作相关的文件会出现在：

- 可执行工具：`<你的-buildroot>/output/target/usr/bin/gpio*`
- 运行时库：`<你的-buildroot>/output/target/usr/lib/libgpiod*.so*`（以及可能的 `libgpiodcxx*.so*`，如果你启用了 C++ 绑定）

## 移植工具

只把“新增的库与工具”合并到 NFS 根（不碰你现有 BusyBox）

笔者的 NFS 根目录是：`/home/lizhaojun/nfs/rootfs`。
 我们**只同步必要的文件与目录**，并且**不使用 `--delete`**（避免误删现有的文件）。

先在宿主机上创建目标目录（若还不存在）：

```
sudo install -d -m 0755 /home/lizhaojun/nfs/rootfs/usr/bin
sudo install -d -m 0755 /home/lizhaojun/nfs/rootfs/usr/lib
```

把工具拷过去：

```
sudo rsync -a \
  /你的-buildroot/output/target/usr/bin/gpio* \
  /home/lizhaojun/nfs/rootfs/usr/bin/
```

把库拷过去（包含所有可能的主版本号与符号链接）：

```
sudo rsync -a \
  /你的-buildroot/output/target/usr/lib/libgpiod* \
  /home/lizhaojun/nfs/rootfs/usr/lib/
```

> 说明与目的：
>
> - 只同步 `gpio*` 可执行文件与 `libgpiod*` 运行库，**不去动** 你 NFS 根里现有的 BusyBox、启动脚本和其他目录。
> - 不加 `--delete`，避免把你 NFS 根中的旧文件删掉。
> - 如果你的工具链还给出了 `libgpiodcxx*`（C++ 绑定），同样一并同步到 `/usr/lib/`。
> - 如果你启用了手册页或示例（通常不会），可按需再拷贝 `/usr/share/` 下相关内容。



## 上电开发板验证

![image](./../../images/buildroot/external_toolchain/tool_libgpiod_add.png)

log:

```txt
...
[    6.064508] VFS: Mounted root (nfs filesystem) on device 0:15.
[    6.071994] devtmpfs: mounted
[    6.076957] Freeing unused kernel image (initmem) memory: 1024K
[    6.094056] Run /sbin/init as init process

Please press Enter to activate this console. [   13.925797] platform regulator-can-3v3: deferred probe pending
[   13.931672] platform 2090000.can: deferred probe pending
[   13.937139] platform 2094000.can: deferred probe pending

~ #
~ # ls
bin      etc      linuxrc  proc     run      sys      usr
dev      lib      mnt      root     sbin     tmp
~ # which gpiodetect
/usr/bin/gpiodetect
~ #
```

可以看到已经可以找到gpiodetect工具，说明已经添加成功。

