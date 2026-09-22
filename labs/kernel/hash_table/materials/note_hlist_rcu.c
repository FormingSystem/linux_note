// SPDX-License-Identifier: GPL-2.0
/* 一个已选定的哈希桶：仅演示发布、旧路径和异步回收，不导出设备。 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/spinlock.h>
#include <linux/string.h>

struct user_session {
	int id;
	char name[32];
	struct hlist_node node;
	struct rcu_head rcu;
};

static HLIST_HEAD(session_head);
static DEFINE_SPINLOCK(session_lock);
static int fail_at;
module_param(fail_at, int, 0444);
MODULE_PARM_DESC(fail_at, "在第 1 到 3 个对象准备前模拟失败，0 表示不注入");

/* 返回地址只属于调用者已建立的读侧区间，没有取得独立引用。 */
static struct user_session *find_user_rcu(int id)
{
	struct user_session *item;

	RCU_LOCKDEP_WARN(!rcu_read_lock_held(), "find_user_rcu needs RCU");
	hlist_for_each_entry_rcu(item, &session_head, node)
		if (item->id == id)
			return item;
	return NULL;
}

static int add_user(int id, const char *name)
{
	struct user_session *candidate, *item;
	int error = 0;

	candidate = kzalloc(sizeof(*candidate), GFP_KERNEL);
	if (!candidate)
		return -ENOMEM;
	candidate->id = id;
	if (strscpy(candidate->name, name, sizeof(candidate->name)) < 0) {
		kfree(candidate);
		return -E2BIG;
	}
	INIT_HLIST_NODE(&candidate->node);
	spin_lock(&session_lock);
	hlist_for_each_entry(item, &session_head, node)
		if (item->id == id) {
			error = -EEXIST;
			goto unlock;
		}
	hlist_add_head_rcu(&candidate->node, &session_head);
unlock:
	spin_unlock(&session_lock);
	if (error)
		kfree(candidate); /* 未发布候选可直接释放。 */
	return error;
}

static void reclaim_user(struct rcu_head *head)
{
	struct user_session *item = container_of(head, struct user_session, rcu);

	pr_info("note_hlist_rcu: reclaim id=%d\n", item->id);
	kfree(item);
}

static int remove_user_async(int id)
{
	struct user_session *item, *removed = NULL;

	spin_lock(&session_lock);
	hlist_for_each_entry(item, &session_head, node)
		if (item->id == id) {
			hlist_del_rcu(&item->node);
			removed = item;
			break; /* 普通遍历删除后立即离开，不再执行推进表达式。 */
		}
	spin_unlock(&session_lock);
	if (!removed)
		return -ENOENT;
	call_rcu(&removed->rcu, reclaim_user);
	return 0;
}

static void drain_sessions(void)
{
	struct user_session *item;
	struct hlist_node *next;

	/* 本模块没有外部生产入口，调用本函数时不会再提交新对象。 */
	spin_lock(&session_lock);
	hlist_for_each_entry_safe(item, next, &session_head, node) {
		hlist_del_rcu(&item->node);
		call_rcu(&item->rcu, reclaim_user);
	}
	spin_unlock(&session_lock);
	rcu_barrier(); /* 等回调真正执行，之后模块回调代码才可消失。 */
}

static int __init note_hlist_rcu_init(void)
{
	const int ids[] = { 10, 18, 26 };
	const char *const names[] = { "user10", "user18", "user26" };
	struct user_session *old, *following;
	struct hlist_node *next;
	unsigned int index;
	int error = -ENOMEM;

	if (fail_at < 0 || fail_at > 3)
		return -EINVAL;
	for (index = 0; index < ARRAY_SIZE(ids); ++index) {
		if ((unsigned int)fail_at == index + 1)
			goto fail;
		error = add_user(ids[index], names[index]);
		if (error)
			goto fail;
		error = -ENOMEM; /* 为下一次注入点保留确定的失败码。 */
	}
	rcu_read_lock();
	old = find_user_rcu(18);
	if (!old) {
		error = -ENOENT;
		goto unlock_fail;
	}
	error = remove_user_async(18);
	if (error)
		goto unlock_fail;
	next = rcu_dereference_raw(hlist_next_rcu(&old->node));
	following = hlist_entry_safe(next, struct user_session, node);
	if (!following || following->id != 10 || find_user_rcu(18)) {
		error = -EINVAL;
		goto unlock_fail;
	}
	pr_info("note_hlist_rcu: old=%s next=%d lookup18=absent\n",
		old->name, following->id);
	rcu_read_unlock();
	return 0;
unlock_fail:
	rcu_read_unlock();
fail:
	drain_sessions();
	return error;
}

static void __exit note_hlist_rcu_exit(void)
{
	drain_sessions();
}

module_init(note_hlist_rcu_init);
module_exit(note_hlist_rcu_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("hlist 旧路径与回调寿命教学模块");
