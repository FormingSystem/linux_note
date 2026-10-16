#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/init.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/printk.h>
#include "note_api.h"

int note_value_get(void)
{
    return 42;
}
/* 这是模块间接口，不是普通 C 声明的替代品。 */
EXPORT_SYMBOL_GPL(note_value_get);

static int __init note_provider_init(void)
{
    pr_info("ready\n");
    return 0;
}

static void __exit note_provider_exit(void)
{
    pr_info("unloaded\n");
}

module_init(note_provider_init);
module_exit(note_provider_exit);
MODULE_LICENSE("GPL");
