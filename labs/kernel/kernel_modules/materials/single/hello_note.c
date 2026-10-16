// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

static int __init hello_note_init(void)
{
    /* 本例没有申请设备或其他需回滚的资源。 */
    pr_info("loaded\n");
    return 0;
}

static void __exit hello_note_exit(void)
{
    /* 真实模块应在这里结束工作并释放其拥有的资源。 */
    pr_info("unloaded\n");
}

module_init(hello_note_init);
module_exit(hello_note_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Minimal module lifecycle example");
