#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include "note_api.h"

static int __init note_consumer_init(void)
{
    /* 装载时必须已经有能够满足该符号引用的提供者。 */
    pr_info("value=%d\n", note_value_get());
    return 0;
}

static void __exit note_consumer_exit(void)
{
    pr_info("unloaded\n");
}

module_init(note_consumer_init);
module_exit(note_consumer_exit);
MODULE_LICENSE("GPL");
