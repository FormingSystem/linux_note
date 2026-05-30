此处为简写版本，方面背框架代码。详细内容参考：[P02-字符设备-LED点灯+dts.md](../../../driver/P01 - 字符驱动框架/P02-字符设备-LED点灯+dts.md)。



### i.MX6ULL 基于设备树的 LED 驱动开发示例


#### 一、设备树配置
需修改 `imx6ul-14x14-test.dtsi` 文件，完成引脚复用配置和 LED 节点定义。

1. **引脚复用配置（添加到 `&iomuxc` 节点）** 
   定义 LED 引脚（GPIO1_IO03）的复用功能和电气属性：
   
   ```dts
   &iomuxc {
       pinctrl-names = "default";
   
       // LED 引脚配置组：复用为 GPIO 功能
       pinctrl_dt_led: dtledgrp {
           fsl,pins = <
               // 引脚复用宏：物理引脚 → GPIO1_IO03 功能
               // 电气属性 0x10B0：配置驱动能力和上拉/下拉（参考芯片手册）
               MX6UL_PAD_GPIO1_IO03__GPIO1_IO03    0x10B0
           >;
       };
   };
   ```
   
2. **LED 设备节点（添加到根节点 `/` 下）** 
   定义 LED 设备的硬件信息，关联 GPIO 控制器和引脚配置：
   
   ```dts
   / {
       dt_led: led@0 {
           compatible = "nxp,imx6ull-dt-led"; // 与驱动匹配的标识
           status = "okay";                  // 启用节点
           gpios = <&gpio1 3 GPIO_ACTIVE_LOW>; // 关联 GPIO1_IO03，低电平点亮
           pinctrl-names = "default";        // 引脚配置名称
           pinctrl-0 = <&pinctrl_dt_led>;    // 关联上述引脚配置组
       };
   };
   ```
   
3. **冲突处理** 
   若引脚被其他设备（如 `tsc`）占用，注释冲突配置：
   ```dts
   // arch/arm/boot/dts/imx6ul-14x14-test.dtsi：
   &tsc {
   	pinctrl-names = "default";
   	pinctrl-0 = <&pinctrl_tsc>;
   	// xnur-gpio = <&gpio1 3 GPIO_ACTIVE_LOW>;		// 注释掉冲突的引脚。重新加载即可
   	measure-delay-time = <0xffff>;
   	pre-charge-time = <0xfff>;
   	status = "disabled";
   };
   ```


#### 二、驱动代码实现（`dt_led.c`）
通过平台驱动框架匹配设备树节点，使用 GPIO 子系统 API 控制 LED。

```c
// dt_led.c

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <linux/kernel.h>    /* pr_info/pr_warn/pr_err */
#include <linux/string.h>    /* strim, strncpy, strlen */
#include <linux/err.h>       /* IS_ERR, PTR_ERR */
#include <linux/kstrtox.h>   /* kstrtobool */

// 驱动常量定义
#define DRIVER_NAME  "imx6ull-dt-led"
#define DEVICE_NAME  "dt_led"
#define BUFFER_SIZE  16

// LED设备私有数据结构
#include <linux/fs.h>        /* struct file, file_operations */
#include <linux/minmax.h>    /* min() */
#include <linux/init.h>      /* module init/exit helpers */
struct dt_led_dev {
    struct class *led_class;    // 设备类（用于/sys/class）
    struct device *led_device;  // 设备节点（用于/dev）
    struct cdev led_cdev;       // 字符设备核心结构
    dev_t dev_num;              // 设备号
    char buffer[BUFFER_SIZE];   // 用户输入缓冲区
    struct mutex led_mutex;     // 互斥锁（保证多线程安全）
    struct gpio_desc *gpiod;    // GPIO描述符（从设备树获取）
};
static struct dt_led_dev led_dev;

// ---------------- 硬件抽象：GPIO电平控制 ----------------
// 逻辑控制：on=true → 点亮（低电平）；on=false → 熄灭（高电平）
static inline void led_set_state(bool on) {
    // gpiod_set_value：内核GPIO子系统API，设置GPIO电平
    // 因设备树配置GPIO_ACTIVE_LOW，on=true对应输出0（低电平）
    gpiod_set_value(led_dev.gpiod, on ? 0 : 1);
}

// ---------------- 业务逻辑：解析用户输入 ----------------
// 解析用户输入字符串，返回LED动作（点亮/熄灭/保持）
enum led_action {
    LED_ACT_KEEP = 0,  // 保持现状
    LED_ACT_ON,        // 点亮
    LED_ACT_OFF        // 熄灭
};

static enum led_action parse_user_input(const char *input, size_t len) {
    char trimmed[BUFFER_SIZE];
    strncpy(trimmed, input, len);
    trimmed[len] = '\0';
    strim(trimmed); // 去除前后空格/换行
    len = strlen(trimmed);
    // 1. 单字符解析：'0'→点亮，'1'→熄灭
    if (len == 1) {
        if (trimmed[0] == '0') return LED_ACT_ON;
        if (trimmed[0] == '1') return LED_ACT_OFF;
    }
    
    // 2. 字符串解析：on/true/yes→点亮；off/false/no→熄灭（大小写不敏感）
    bool on;
    if (!kstrtobool(trimmed, &on)) {
        return on ? LED_ACT_ON : LED_ACT_OFF;
    }
    
    // 3. 无法识别的输入→保持现状
    return LED_ACT_KEEP;
}

// ---------------- 文件操作接口：与用户空间交互 ----------------
static int led_open(struct inode *inode, struct file *file) {
    struct dt_led_dev *dev = container_of(inode->i_cdev, struct dt_led_dev, led_cdev);
    file->private_data = dev;
    pr_info("[%s] device opened\n", DRIVER_NAME);
    return 0;
}

static int led_release(struct inode *inode, struct file *file) {
    pr_info("[%s] device closed\n", DRIVER_NAME);
    return 0;
}

// 写入接口：用户通过echo写入控制命令（如echo 0 > /dev/dt_led）
static ssize_t led_write(struct file *file, const char __user *ubuf, 
                         size_t count, loff_t *ppos) {
    struct dt_led_dev *dev = file->private_data;
    size_t copy_len;
    enum led_action act;

    if (count == 0) return 0;
    
    // 互斥锁保护：避免多线程同时写入
    mutex_lock(&dev->led_mutex);
    
    // 1. 拷贝用户输入到内核缓冲区
    copy_len = min(count, (size_t)(BUFFER_SIZE - 1));
    if (copy_from_user(dev->buffer, ubuf, copy_len)) {
        mutex_unlock(&dev->led_mutex);
        return -EFAULT; // 拷贝失败（用户空间地址非法）
    }
    
    // 2. 解析输入并执行动作
    act = parse_user_input(dev->buffer, copy_len);
    switch (act) {
        case LED_ACT_ON:
            led_set_state(true);
            pr_info("[%s] LED turned ON (input: %.*s)\n", 
                    DRIVER_NAME, (int)copy_len, dev->buffer);
            break;
        case LED_ACT_OFF:
            led_set_state(false);
            pr_info("[%s] LED turned OFF (input: %.*s)\n", 
                    DRIVER_NAME, (int)copy_len, dev->buffer);
            break;
        case LED_ACT_KEEP:
            pr_warn("[%s] unknown input: %.*s (keep state)\n", 
                    DRIVER_NAME, (int)copy_len, dev->buffer);
            break;
    }
    
    mutex_unlock(&dev->led_mutex);
    return copy_len; // 返回实际写入字节数
}

// 文件操作结构体：关联用户空间接口
static const struct file_operations led_fops = {
    .owner   = THIS_MODULE,
    .open    = led_open,
    .release = led_release,
    .write   = led_write,
};

// ---------------- 平台驱动接口：与设备树匹配 ----------------
// probe函数：设备树节点与驱动匹配成功后执行（初始化核心逻辑）
static int dt_led_probe(struct platform_device *pdev) {
    struct device *dev = &pdev->dev;
    int ret;

    // 1. 初始化私有数据
    memset(&led_dev, 0, sizeof(struct dt_led_dev));
    mutex_init(&led_dev.led_mutex);
    
    // 2. 从设备树获取GPIO资源（核心步骤）
    // devm_gpiod_get：自动管理GPIO资源，无需手动释放
    // 第二个参数NULL：匹配设备树"gpios"属性（无名称时）
    // GPIOD_OUT_LOW：默认输出低电平（熄灭，因GPIO_ACTIVE_LOW实际为高电平）
    led_dev.gpiod = devm_gpiod_get(dev, NULL, GPIOD_OUT_LOW);
    if (IS_ERR(led_dev.gpiod)) {
        dev_err(dev, "failed to get GPIO from device tree\n");
        return PTR_ERR(led_dev.gpiod);
    }
    
    // 3. 申请设备号（动态分配，避免静态冲突）
    ret = alloc_chrdev_region(&led_dev.dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        dev_err(dev, "failed to allocate device number\n");
        return ret;
    }
    
    // 4. 初始化并注册字符设备
    cdev_init(&led_dev.led_cdev, &led_fops);
    led_dev.led_cdev.owner = THIS_MODULE;
    ret = cdev_add(&led_dev.led_cdev, led_dev.dev_num, 1);
    if (ret < 0) {
        dev_err(dev, "failed to add cdev\n");
        goto err_unregister_chrdev;
    }
    
    // 5. 创建设备类（在/sys/class下生成dt_led_class）
    led_dev.led_class = class_create(THIS_MODULE, "dt_led_class");
    if (IS_ERR(led_dev.led_class)) {
        dev_err(dev, "failed to create class\n");
        ret = PTR_ERR(led_dev.led_class);
        goto err_cdev_del;
    }
    
    // 6. 创建设备节点（在/dev下生成dt_led）
    led_dev.led_device = device_create(led_dev.led_class, NULL, 
                                       led_dev.dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(led_dev.led_device)) {
        dev_err(dev, "failed to create device node\n");
        ret = PTR_ERR(led_dev.led_device);
        goto err_class_destroy;
    }
    
    dev_info(dev, "LED driver probe success! Try: echo 0 > /dev/%s (ON), echo 1 > /dev/%s (OFF)\n",
             DEVICE_NAME, DEVICE_NAME);
    return 0;
    
    // 错误处理：反向释放资源
err_class_destroy:
    class_destroy(led_dev.led_class);
err_cdev_del:
    cdev_del(&led_dev.led_cdev);
err_unregister_chrdev:
    unregister_chrdev_region(led_dev.dev_num, 1);
    return ret;
}

// remove函数：驱动卸载时执行（释放资源）
static int dt_led_remove(struct platform_device *pdev) {
    // 释放字符设备相关资源
    device_destroy(led_dev.led_class, led_dev.dev_num);
    class_destroy(led_dev.led_class);
    cdev_del(&led_dev.led_cdev);
    unregister_chrdev_region(led_dev.dev_num, 1);

    // GPIO资源由devm_xxx自动释放，无需手动调用gpiod_put()
    dev_info(&pdev->dev, "LED driver removed\n");
    return 0;
}

// 设备树匹配表：驱动与设备树节点的匹配规则
static const struct of_device_id dt_led_of_match[] = {
    { .compatible = "nxp,imx6ull-dt-led" }, // 与设备树led节点的compatible一致
    { /* 终止符：必须添加 */ }
};
MODULE_DEVICE_TABLE(of, dt_led_of_match); // 向内核注册匹配表

// 平台驱动结构体：关联probe/remove与匹配表
static struct platform_driver dt_led_driver = {
    .probe  = dt_led_probe,
    .remove = dt_led_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = of_match_ptr(dt_led_of_match), // 关联设备树匹配表
    },
};

// 模块加载/卸载接口
module_platform_driver(dt_led_driver);

// 模块信息
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("i.MX6ULL LED Driver Based on Device Tree (Linux 6.1)");
MODULE_ALIAS("imx6ull-dt-led");
```


#### 三、编译与验证
1. **Makefile（交叉编译配置）** 
   
   ```makefile
   # 编译目标：dt_led.c -> dt_led.ko
   obj-m := dt_led.o
   
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
   
2. **验证步骤** 
   
   - 编译设备树：`make imx6ul-14x14-test.dtb` 生成 `imx6ul-14x14-test.dtb` 并烧录到开发板。 
   - 编译驱动：`make` 生成 `dt_led.ko`，拷贝到开发板。 
   - 加载驱动：`insmod dt_led.ko`。 
   - 控制 LED： 
     ```bash
     echo 0 > /dev/dt_led  # 点亮 LED
     echo 1 > /dev/dt_led  # 熄灭 LED
     ```


#### 核心逻辑总结
1. 设备树：通过 `gpios` 和 `pinctrl` 属性定义 LED 硬件信息（引脚、复用、电平极性），与驱动解耦。 
2. 驱动：通过 `of_device_id` 匹配设备树节点，使用 `devm_gpiod_get` 获取 GPIO 资源，通过 `gpiod_set_value` 控制电平。 
3. 优势：更换 LED 引脚时无需修改驱动，仅需调整设备树配置，提升兼容性和开发效率。