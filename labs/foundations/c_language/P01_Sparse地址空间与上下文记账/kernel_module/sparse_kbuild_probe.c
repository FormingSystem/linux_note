// SPDX-License-Identifier: GPL-2.0-only

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>

static bool probe_user_pointer(const int __user *source)
{
#ifdef BUILD_BAD_ADDRESS
	/* 故意错误：把用户地址空间指针降格为普通内核指针。 */
	const int *plain = source;

	return plain != NULL;
#else
	/* 正确基线只比较指针，不执行真实用户内存访问。 */
	return source != NULL;
#endif
}

static int __init sparse_kbuild_probe_init(void)
{
	/* 模块不会在实验中加载；调用只保证函数进入翻译单元。 */
	(void)probe_user_pointer(NULL);
	return 0;
}

static void __exit sparse_kbuild_probe_exit(void)
{
}

module_init(sparse_kbuild_probe_init);
module_exit(sparse_kbuild_probe_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Sparse Kbuild integration probe");
