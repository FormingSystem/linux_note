// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>

struct service_cfg {
	struct rcu_head rcu;
	u32 generation;
	u32 timeout_ms;
	bool enabled;
};

static struct service_cfg __rcu *active_cfg;
static DEFINE_MUTEX(cfg_update_lock);

static int service_fast_path(u32 *timeout_ms)
{
	struct service_cfg *cfg;
	u32 timeout;
	bool enabled;

	rcu_read_lock();
	cfg = rcu_dereference(active_cfg);
	if (!cfg) {
		rcu_read_unlock();
		return -ENOENT;
	}

	/* cfg 是共享对象本身的地址，不是副本。 */
	timeout = READ_ONCE(cfg->timeout_ms);
	enabled = READ_ONCE(cfg->enabled);
	rcu_read_unlock();

	/* 这里只使用已经复制到栈上的标量，不再解引用 cfg。 */
	*timeout_ms = enabled ? timeout : 0;
	return 0;
}

static int service_cfg_replace(u32 timeout_ms, bool enabled)
{
	struct service_cfg *new_cfg;
	struct service_cfg *old_cfg;

	new_cfg = kzalloc(sizeof(*new_cfg), GFP_KERNEL);
	if (!new_cfg)
		return -ENOMEM;

	/* 发布以前完成 reader 可见字段初始化。 */
	new_cfg->timeout_ms = timeout_ms;
	new_cfg->enabled = enabled;

	mutex_lock(&cfg_update_lock);
	old_cfg = rcu_dereference_protected(
		active_cfg, lockdep_is_held(&cfg_update_lock));
	new_cfg->generation = old_cfg ? old_cfg->generation + 1 : 1;
	rcu_assign_pointer(active_cfg, new_cfg);
	mutex_unlock(&cfg_update_lock);

	/* 这里允许睡眠，而且已经不持有更新锁。 */
	synchronize_rcu();
	kfree(old_cfg);
	return 0;
}

static void service_cfg_free_rcu(struct rcu_head *head)
{
	struct service_cfg *cfg;

	cfg = container_of(head, struct service_cfg, rcu);
	kfree(cfg);
}

static int service_cfg_replace_async(struct service_cfg *new_cfg)
{
	struct service_cfg *old_cfg;

	mutex_lock(&cfg_update_lock);
	old_cfg = rcu_dereference_protected(
		active_cfg, lockdep_is_held(&cfg_update_lock));
	new_cfg->generation = old_cfg ? old_cfg->generation + 1 : 1;
	rcu_assign_pointer(active_cfg, new_cfg);
	mutex_unlock(&cfg_update_lock);

	if (old_cfg)
		call_rcu(&old_cfg->rcu, service_cfg_free_rcu);
	return 0;
}

static void service_cfg_shutdown(void)
{
	struct service_cfg *old_cfg;

	/* 调用者此前必须停止所有查询入口和配置更新来源。 */
	mutex_lock(&cfg_update_lock);
	old_cfg = rcu_replace_pointer(
		active_cfg, NULL,
		lockdep_is_held(&cfg_update_lock));
	mutex_unlock(&cfg_update_lock);

	/* 保护刚从入口摘除、尚未交给回调的最后一个对象。 */
	synchronize_rcu();
	kfree(old_cfg);

	/* 等待更早的异步更新所登记的回调函数体执行完毕。 */
	rcu_barrier();
}

/* 只比较读区内复制出的值，不保存共享对象地址。 */
static int service_expect(u32 expected)
{
	u32 timeout_ms = 0;
	int ret = service_fast_path(&timeout_ms);

	if (ret)
		return ret;
	pr_info("service_config: observed=%u expected=%u\n", timeout_ms, expected);
	return timeout_ms == expected ? 0 : -EINVAL;
}

static int __init service_cfg_init(void)
{
	struct service_cfg *candidate;
	u32 untouched = 7;
	int ret;

	/* 尚未发布时应返回缺失，且不修改输出。 */
	ret = service_fast_path(&untouched);
	if (ret != -ENOENT || untouched != 7)
		return -EINVAL;

	ret = service_cfg_replace(100, true);
	if (ret)
		goto fail;
	ret = service_expect(100);
	if (ret)
		goto fail;

	/* 候选在发布前始终私有；本函数成功交接后不再使用它。 */
	candidate = kzalloc(sizeof(*candidate), GFP_KERNEL);
	if (!candidate) {
		ret = -ENOMEM;
		goto fail;
	}
	candidate->timeout_ms = 0xffffffffU;
	candidate->enabled = true;
	service_cfg_replace_async(candidate);
	ret = service_expect(0xffffffffU);
	if (ret)
		goto fail;

	ret = service_cfg_replace(300, false);
	if (ret)
		goto fail;
	ret = service_expect(0);
	if (ret)
		goto fail;

	pr_info("service_config: lifecycle checks passed\n");
	return 0;

fail:
	/* 初始化失败不会进入正常module_exit，必须在此排空。 */
	service_cfg_shutdown();
	return ret;
}

static void __exit service_cfg_exit(void)
{
	service_cfg_shutdown();
	pr_info("service_config: shutdown complete\n");
}

module_init(service_cfg_init);
module_exit(service_cfg_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RCU service configuration lifecycle example");
