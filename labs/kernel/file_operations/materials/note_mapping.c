// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

static void *mapping_page;

static int mapping_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;

    if (vma->vm_pgoff != 0 || size != PAGE_SIZE)
        return -EINVAL;
    if (!(vma->vm_flags & VM_SHARED))
        return -EINVAL;
    if (vma->vm_flags & (VM_WRITE | VM_EXEC))
        return -EPERM;

    /* 不允许后续 mprotect 把这份只读数据改成可写或可执行。 */
    vm_flags_clear(vma, VM_MAYWRITE | VM_MAYEXEC);
    return remap_vmalloc_range(vma, mapping_page, 0);
}

static const struct file_operations mapping_fops = {
    .owner = THIS_MODULE,
    .mmap = mapping_mmap,
};

static struct miscdevice mapping_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "note_mapping",
    .fops = &mapping_fops,
    .mode = 0400,
};

static int __init mapping_init(void)
{
    static const char message[] = "hello mapped page\n";
    int ret;

    /* 专用、清零、允许映射给用户的页，不暴露 slab 内的邻接对象。 */
    mapping_page = vmalloc_user(PAGE_SIZE);
    if (!mapping_page)
        return -ENOMEM;
    memcpy(mapping_page, message, sizeof(message) - 1);

    ret = misc_register(&mapping_device);
    if (ret) {
        vfree(mapping_page);
        mapping_page = NULL;
    }
    return ret;
}

static void __exit mapping_exit(void)
{
    /* 仅演示普通模块卸载，已有映射保留的 file 引用会阻止它。 */
    misc_deregister(&mapping_device);
    vfree(mapping_page);
    mapping_page = NULL;
}

module_init(mapping_init);
module_exit(mapping_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("专用只读页与映射持有的文件引用");
