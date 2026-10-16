#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include "note_value.h"

static int __init hello_note_init(void)
{
    /* 普通函数调用发生在同一模块内部。 */
    pr_info("value=%d\n", note_value_get());
    return 0;
}

static void __exit hello_note_exit(void)
{
    pr_info("unloaded\n");
}

module_init(hello_note_init);
module_exit(hello_note_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Composite module example");
