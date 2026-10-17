// SPDX-License-Identifier: GPL-2.0-only
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>

struct demo_state {
	struct input_dev *input;
	bool high;
};

static struct demo_state demo;

static void demo_poll(struct input_dev *input)
{
	struct demo_state *state = input_get_drvdata(input);

	/* 只模拟一个轴，不访问任何物理控制器。 */
	state->high = !state->high;
	input_report_abs(input, ABS_X, state->high ? 100 : 0);
	input_sync(input);
}

static int demo_open(struct input_dev *input)
{
	struct demo_state *state = input_get_drvdata(input);

	/* core在open成功之后才启动poller。 */
	state->high = false;
	pr_info("input_lifecycle_demo: open\n");
	return 0;
}

static void demo_close(struct input_dev *input)
{
	/* 普通最后关闭中，core已同步停止poller，再调用这里。 */
	pr_info("input_lifecycle_demo: close\n");
}

static int __init demo_init(void)
{
	struct input_dev *input;
	int error;

	input = input_allocate_device();
	if (!input)
		return -ENOMEM;

	demo.input = input;
	input_set_drvdata(input, &demo);
	input->name = "input-lifecycle-demo";
	input->id.bustype = BUS_VIRTUAL;
	input->open = demo_open;
	input->close = demo_close;
	input_set_abs_params(input, ABS_X, 0, 100, 0, 0);

	/* 设置轮询可能分配失败，尚未注册时走free路径。 */
	error = input_setup_polling(input, demo_poll);
	if (error)
		goto err_free;
	input_set_poll_interval(input, 1000);
	input_set_min_poll_interval(input, 1000);
	input_set_max_poll_interval(input, 1000);

	/* 发布后可能立即被打开，因此上面必须先准备完毕。 */
	error = input_register_device(input);
	if (error)
		goto err_free;
	return 0;

err_free:
	input_free_device(input);
	demo.input = NULL;
	return error;
}

static void __exit demo_exit(void)
{
	/* 注销收束handler使用关系及poller；之后不再解引用input。 */
	input_unregister_device(demo.input);
	demo.input = NULL;
}

module_init(demo_init);
module_exit(demo_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Input lifecycle teaching device with a virtual axis");
