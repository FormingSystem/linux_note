---
id: research.source_reading.devres.impl.clock
title: "托管时钟获取与启停实现"
kind: source
status: evolving
domains: [linux, source_reading]
---

# 第1章\_托管时钟获取与启停实现

上游位置`drivers/clk/clk-devres.c`，NXP Linux 6.12.20固定dfaf2136提交；[完整原文](../../../../linux/drivers/clk/clk-devres.c)。先读[模块导读](../../../navigation/P04_句柄启停与注册契约导读.md#4.1_时钟把退出动作放进同一记录)。以下Doxygen与中文阅读注释是仓库补充，源码只抽取完整结构及函数，未修改控制流。

## 1.1\_获取与初始化分支

state中的clk保存获取结果，exit保存可选的退出动作；GFP_KERNEL是记录分配标志。普通get传入的init/exit均为空，enabled包装则传入准备使能与关闭反准备两个动作。NULL句柄是否有效由所选get契约决定，IS_ERR检查的只是错误指针。

这里沿用前篇的错误指针表示：ENOMEM表示内存不足，ERR_PTR把负错误码编码为指针，PTR_ERR取回错误码。下面每条失败出口都要同时检查“交还错误的形式”和“此时已经取得哪些资源”，两者不能互相替代。PM（Power Management，电源管理）中的反复启停另有运行协议，本条记录只负责约定的最终清理。

```c
/**
 * @brief 成功获取并完成可选初始化后才登记，失败按已经取得的责任回滚。
 * @note 调用者必须使用匹配的init/exit协议，不能重复消费托管启停责任。
 */
struct devm_clk_state {
	struct clk *clk;
	void (*exit)(struct clk *clk);
};

static struct clk *__devm_clk_get(struct device *dev, const char *id,
				  struct clk *(*get)(struct device *dev, const char *id),
				  int (*init)(struct clk *clk),
				  void (*exit)(struct clk *clk))
{
	/* 仓库阅读注释：记录、句柄、初始化依次成功后才进入设备账本。 */
	struct devm_clk_state *state;
	struct clk *clk;
	int ret;

	state = devres_alloc(devm_clk_release, sizeof(*state), GFP_KERNEL);
	if (!state)
		return ERR_PTR(-ENOMEM);

	clk = get(dev, id);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto err_clk_get;
	}

	if (init) {
		ret = init(clk);
		if (ret)
			goto err_clk_init;
	}

	state->clk = clk;
	state->exit = exit;

	devres_add(dev, state);

	return clk;

err_clk_init:

	clk_put(clk);
err_clk_get:

	devres_free(state);
	return ERR_PTR(ret);
}
```

记录失败时没有句柄可归还；get错误时只释放记录；init失败时先put句柄再释放记录。成功后回调使用的clk和exit都在记录内，不能等到解绑时再根据当前函数名猜测应清理什么。公开enabled获取由它配对clk_prepare_enable与clk_disable_unprepare，普通获取则只登记put。

## 1.2\_退出动作先于句柄归还

```c
/**
 * @brief 若记录带有exit先退出运行阶段，然后归还句柄。
 * @note 只履行这一条托管记录的责任，不代替全部PM周期。
 */
static void devm_clk_release(struct device *dev, void *res)
{
	/* 仓库阅读注释：先结束被登记的状态责任，再结束句柄责任。 */
	struct devm_clk_state *state = res;

	if (state->exit)
		state->exit(state->clk);

	clk_put(state->clk);
}
```

普通记录的exit为空，不执行disable；enabled记录有exit，先disable/unprepare再put。全局设备资源链的逆序决定本记录和其他资源记录的先后，本函数决定同一记录内部的先后。六条宿主路径只验证包装分支，底层时钟提供者和devres登记为明确替身，真实硬件和PM未执行。

返回[模块导读](../../../navigation/P04_句柄启停与注册契约导读.md#4.1_时钟把退出动作放进同一记录)或[总索引](../../../navigation/P01_Linux_6.12_devres源码阅读索引.md#1.2_按问题进入实现)。
