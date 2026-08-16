// SPDX-License-Identifier: GPL-2.0-only
/*
 * Lockdep 锁序实验：
 * 1. 先完整执行 config_lock -> state_lock；
 * 2. 再完整执行 state_lock -> config_lock；
 * 3. 让 Lockdep 组合两条组件链，而不制造真实 ABBA 卡死。
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>

#define LAB_PREFIX "lockdep_cycle_lab: "

static DEFINE_MUTEX(config_lock);
static DEFINE_MUTEX(state_lock);

static int __init lockdep_cycle_demo_init(void)
{
	if (!IS_ENABLED(CONFIG_PROVE_LOCKING)) {
		pr_err(LAB_PREFIX "CONFIG_PROVE_LOCKING is disabled\n");
		return -EOPNOTSUPP;
	}

	pr_info(LAB_PREFIX "S0 record config_lock -> state_lock\n");
	mutex_lock(&config_lock);
	mutex_lock(&state_lock);
	mutex_unlock(&state_lock);
	mutex_unlock(&config_lock);

	pr_info(LAB_PREFIX "S1 propose state_lock -> config_lock\n");
	mutex_lock(&state_lock);
	mutex_lock(&config_lock);
	mutex_unlock(&config_lock);
	mutex_unlock(&state_lock);

	pr_info(LAB_PREFIX "S2 functional mutex path completed\n");
	return 0;
}

static void __exit lockdep_cycle_demo_exit(void)
{
	pr_info(LAB_PREFIX "unloaded\n");
}

module_init(lockdep_cycle_demo_init);
module_exit(lockdep_cycle_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("FormingSystem");
MODULE_DESCRIPTION("Lockdep lock-order inversion lab without a real deadlock");
