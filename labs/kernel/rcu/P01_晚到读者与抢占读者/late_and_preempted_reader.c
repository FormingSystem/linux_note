// SPDX-License-Identifier: GPL-2.0-only
/*
 * RCU 生命周期教学实验：
 * 1. 已启动但被挡在读侧外的晚到读者只会取得新对象；
 * 2. 已取得旧指针的读者被抢占后，GP 仍等待其最外层 unlock。
 */

#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/timekeeping.h>

#define LAB_PREFIX "rcu_lifetime_lab: "

struct demo_obj {
	int generation;
	int value;
};

static struct demo_obj __rcu *demo_current;

static int target_cpu = -1;
module_param(target_cpu, int, 0444);
MODULE_PARM_DESC(target_cpu,
	"CPU shared by the preempted reader and FIFO disturber (-1: auto)");

static unsigned int hold_ms = 300;
module_param(hold_ms, uint, 0444);
MODULE_PARM_DESC(hold_ms, "Release timer delay in milliseconds, not a promised GP duration");

static DECLARE_COMPLETION(late_reader_ready);
static DECLARE_COMPLETION(start_late_reader);
static DECLARE_COMPLETION(late_reader_done);

static DECLARE_COMPLETION(old_reader_has_pointer);
static DECLARE_COMPLETION(old_reader_done);
static DECLARE_COMPLETION(start_disturber);
static DECLARE_COMPLETION(disturber_running);
static DECLARE_COMPLETION(disturber_done);
static atomic_t release_preempt_reader = ATOMIC_INIT(0);
static struct timer_list release_timer;
/* 时间戳只作事件边界观察；完整读者退出由completion交接。 */
static u64 old_unlock_enter_ns;
static u64 old_unlock_exit_ns;
static u64 release_ns;

static struct task_struct *late_reader_task;
static struct task_struct *old_reader_task;
static struct task_struct *disturber_task;

static struct demo_obj *alloc_demo_obj(int generation, int value)
{
	struct demo_obj *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return NULL;
	obj->generation = generation;
	obj->value = value;
	return obj;
}

static int late_reader_fn(void *unused)
{
	struct demo_obj *obj;
	int generation;
	int value;

	complete(&late_reader_ready);
	wait_for_completion(&start_late_reader);

	rcu_read_lock();
	obj = rcu_dereference(demo_current);
	generation = obj ? obj->generation : -1;
	value = obj ? obj->value : -1;
	rcu_read_unlock();

	pr_info(LAB_PREFIX "late reader saw generation=%d value=%d\n",
		generation, value);
	complete(&late_reader_done);
	while (!kthread_should_stop())
		msleep(20);
	return 0;
}

static int old_reader_fn(void *unused)
{
	struct demo_obj *obj;
	unsigned long checksum = 0;
	int generation;

	rcu_read_lock();
	obj = rcu_dereference(demo_current);
	if (!obj) {
		rcu_read_unlock();
		pr_err(LAB_PREFIX "old reader found NULL\n");
		complete(&old_reader_has_pointer);
		complete(&old_reader_done);
		while (!kthread_should_stop())
			msleep(20);
		return -ENOENT;
	}
	generation = obj->generation;
	pr_info(LAB_PREFIX "old reader acquired generation=%d value=%d on cpu=%d\n",
		generation, obj->value, task_cpu(current));
	complete(&old_reader_has_pointer);

	while (!atomic_read(&release_preempt_reader) && !kthread_should_stop()) {
		checksum ^= READ_ONCE(obj->value);
		cpu_relax();
	}

	WRITE_ONCE(old_unlock_enter_ns, ktime_get_ns());
	rcu_read_unlock();
	WRITE_ONCE(old_unlock_exit_ns, ktime_get_ns());
	pr_info(LAB_PREFIX "old reader unlocked generation=%d checksum=%lu on cpu=%d\n",
		generation, checksum, task_cpu(current));
	complete(&old_reader_done);
	while (!kthread_should_stop())
		msleep(20);
	return 0;
}

static int disturber_fn(void *unused)
{
	wait_for_completion(&start_disturber);

	/* GPL-exported helper；让本任务在目标 CPU 上抢占普通读者。 */
	sched_set_fifo(current);
	pr_info(LAB_PREFIX "FIFO disturber running on cpu=%d\n",
		task_cpu(current));
	complete(&disturber_running);

	while (!atomic_read(&release_preempt_reader) && !kthread_should_stop())
		cpu_relax();

	complete(&disturber_done);
	sched_set_normal(current, 0);
	while (!kthread_should_stop())
		msleep(20);
	return 0;
}

static void release_timer_fn(struct timer_list *timer)
{
	WRITE_ONCE(release_ns, ktime_get_ns());
	atomic_set(&release_preempt_reader, 1);
}

static int choose_target_cpu(void)
{
	int cpu;
	int caller_cpu;

	caller_cpu = get_cpu();
	put_cpu();
	if (target_cpu >= 0)
		return target_cpu < nr_cpu_ids && cpu_online(target_cpu) &&
		       target_cpu != caller_cpu ? target_cpu : -EINVAL;

	for_each_online_cpu(cpu) {
		if (cpu != caller_cpu)
			return cpu;
	}
	return -ENODEV;
}

static int run_late_reader_phase(void)
{
	struct demo_obj *new_obj;
	struct demo_obj *old_obj;

	late_reader_task = kthread_run(late_reader_fn, NULL, "rcu_lifetime_late");
	if (IS_ERR(late_reader_task))
		return PTR_ERR(late_reader_task);

	wait_for_completion(&late_reader_ready);
	new_obj = alloc_demo_obj(2, 200);
	if (!new_obj)
		return -ENOMEM;

	old_obj = rcu_replace_pointer(demo_current, new_obj, true);
	synchronize_rcu();
	kfree(old_obj);

	/* gen1 已释放以后，晚到读者才第一次读取正式入口。 */
	complete(&start_late_reader);
	wait_for_completion(&late_reader_done);
	return 0;
}

static int run_preempt_reader_phase(int cpu)
{
	struct demo_obj *new_obj;
	struct demo_obj *old_obj;
	ktime_t started;
	s64 elapsed_us;
	u64 gp_return_ns;
	bool window_open_at_gp_start;

	old_reader_task = kthread_create(old_reader_fn, NULL, "rcu_lifetime_old");
	if (IS_ERR(old_reader_task))
		return PTR_ERR(old_reader_task);
	kthread_bind(old_reader_task, cpu);

	disturber_task = kthread_create(disturber_fn, NULL, "rcu_lifetime_fifo");
	if (IS_ERR(disturber_task))
		return PTR_ERR(disturber_task);
	kthread_bind(disturber_task, cpu);

	wake_up_process(disturber_task);
	wake_up_process(old_reader_task);
	wait_for_completion(&old_reader_has_pointer);

	new_obj = alloc_demo_obj(3, 300);
	if (!new_obj)
		return -ENOMEM;
	old_obj = rcu_replace_pointer(demo_current, new_obj, true);

	mod_timer(&release_timer, jiffies + msecs_to_jiffies(hold_ms));
	complete(&start_disturber);
	wait_for_completion(&disturber_running);

	/* 若定时器已放行，本次没有建立可观察的抢占等待窗口。 */
	window_open_at_gp_start = !atomic_read(&release_preempt_reader);
	started = ktime_get();
	synchronize_rcu();
	gp_return_ns = ktime_get_ns();
	elapsed_us = ktime_us_delta(ktime_get(), started);
	/* 先保存GP实际返回时刻，再等日志生产者；不拿打印先后冒充事件先后。 */
	wait_for_completion(&old_reader_done);
	wait_for_completion(&disturber_done);
	del_timer_sync(&release_timer);
	pr_info(LAB_PREFIX "preempt GP returned after %lld us window_open_at_start=%d\n",
		elapsed_us, window_open_at_gp_start);
	pr_info(LAB_PREFIX "event_ns release=%llu unlock_enter=%llu unlock_exit=%llu gp_return=%llu\n",
		(unsigned long long)READ_ONCE(release_ns),
		(unsigned long long)READ_ONCE(old_unlock_enter_ns),
		(unsigned long long)READ_ONCE(old_unlock_exit_ns),
		(unsigned long long)gp_return_ns);
	if (!window_open_at_gp_start)
		pr_warn(LAB_PREFIX "preempt window not established; result is inconclusive\n");
	if (gp_return_ns < READ_ONCE(old_unlock_enter_ns))
		pr_err(LAB_PREFIX "GP returned before reader reached unlock; inspect trace and timing\n");

	/* 额外等待读者完成后才释放，异常观测时也不故意制造UAF。 */
	kfree(old_obj);
	return 0;
}

static void stop_started_tasks(void)
{
	atomic_set(&release_preempt_reader, 1);
	complete_all(&start_late_reader);
	complete_all(&start_disturber);
	del_timer_sync(&release_timer);

	if (late_reader_task && !IS_ERR(late_reader_task))
		kthread_stop(late_reader_task);
	if (old_reader_task && !IS_ERR(old_reader_task))
		kthread_stop(old_reader_task);
	if (disturber_task && !IS_ERR(disturber_task))
		kthread_stop(disturber_task);
}

static int __init rcu_lifetime_lab_init(void)
{
	struct demo_obj *obj;
	int cpu;
	int ret;

	if (hold_ms < 20 || hold_ms > 5000)
		return -EINVAL;
	timer_setup(&release_timer, release_timer_fn, 0);

	obj = alloc_demo_obj(1, 100);
	if (!obj)
		return -ENOMEM;
	RCU_INIT_POINTER(demo_current, obj);

	ret = run_late_reader_phase();
	if (ret)
		goto fail;

	if (!IS_ENABLED(CONFIG_PREEMPT_RCU) || IS_ENABLED(CONFIG_PREEMPT_RT)) {
		pr_warn(LAB_PREFIX "requires PREEMPT_RCU without PREEMPT_RT; preempt phase skipped\n");
		return 0;
	}

	/* 固定在线CPU集合，避免实验中途下线目标CPU改变调度前提。 */
	cpus_read_lock();
	cpu = choose_target_cpu();
	if (cpu < 0) {
		cpus_read_unlock();
		pr_warn(LAB_PREFIX "no separate online target CPU; preempt phase skipped\n");
		return 0;
	}
	pr_info(LAB_PREFIX "preempt phase target_cpu=%d hold_ms=%u\n",
		cpu, hold_ms);

	ret = run_preempt_reader_phase(cpu);
	cpus_read_unlock();
	if (ret)
		goto fail;
	return 0;

fail:
	stop_started_tasks();
	obj = rcu_replace_pointer(demo_current, NULL, true);
	synchronize_rcu();
	kfree(obj);
	return ret;
}

static void __exit rcu_lifetime_lab_exit(void)
{
	struct demo_obj *obj;

	stop_started_tasks();
	obj = rcu_replace_pointer(demo_current, NULL, true);
	synchronize_rcu();
	kfree(obj);
	pr_info(LAB_PREFIX "unloaded\n");
}

module_init(rcu_lifetime_lab_init);
module_exit(rcu_lifetime_lab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("FormingSystem");
MODULE_DESCRIPTION("RCU late-reader and preempted-reader lifetime lab");
