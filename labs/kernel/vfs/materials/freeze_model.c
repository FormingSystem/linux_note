#include <stdio.h>
#include <stdlib.h>

/* 单线程教学模型：计数表示已进入的操作，closed表示不再接纳新操作。 */
struct freeze_model {
    unsigned active[3];
    int closed[3];
    unsigned phase; /* 0正常，1至3逐级收口，4冻结完成。 */
};

static int enter(struct freeze_model *model, unsigned level)
{
    if (level >= 3 || model->closed[level]) return 0;
    ++model->active[level];
    return 1;
}

static int leave(struct freeze_model *model, unsigned level)
{
    if (level >= 3 || model->active[level] == 0) return 0;
    --model->active[level];
    return 1;
}

static int begin_freeze(struct freeze_model *model)
{
    if (model->phase != 0) return 0;
    model->phase = 1;
    model->closed[0] = 1;
    return 1;
}

/* 只在当前层已进入者全部退出时推进；阶段2显式模拟同步成功与否。 */
static int advance(struct freeze_model *model, int sync_ok)
{
    if (model->phase < 1 || model->phase > 3) return 0;
    if (model->active[model->phase - 1] != 0) return 0;
    if (model->phase == 2 && !sync_ok) {
        for (unsigned index = 0; index < 3; ++index) model->closed[index] = 0;
        model->phase = 0;
        return -1;
    }
    ++model->phase;
    if (model->phase < 4) model->closed[model->phase - 1] = 1;
    return 1;
}

static void require(int condition, const char *message)
{
    if (!condition) { fprintf(stderr, "模型前提不符：%s\n", message); exit(EXIT_FAILURE); }
}

int main(void)
{
    struct freeze_model model = {0};
    require(enter(&model, 0), "普通写者先进入");
    require(begin_freeze(&model), "开始冻结");
    require(!enter(&model, 0) && advance(&model, 1) == 0, "新写者被挡住，但旧写者尚未退出");
    require(enter(&model, 1) && enter(&model, 2), "此时写缺页和内部操作仍可进入");
    puts("Z1：挡住新普通写者，不能因此宣称旧写者已排空");
    require(leave(&model, 0) && advance(&model, 1) == 1, "普通写者退出后推进第二层");
    require(!enter(&model, 1) && advance(&model, 1) == 0, "新写缺页被挡住，旧写缺页仍需退出");
    require(leave(&model, 1) && advance(&model, 1) == 1, "写缺页退出并完成模拟同步");
    puts("Z2：模拟同步时内部层仍开放，之后才关闭第三层");
    require(!enter(&model, 2) && advance(&model, 1) == 0, "第三层仍有先前进入的操作");
    require(leave(&model, 2) && advance(&model, 1) == 1, "内部操作退出后完成模拟冻结");
    require(model.phase == 4, "冻结完成");
    puts("Z4：三层都已关闭且原有操作退出；模型没有执行磁盘同步");

    /* 另起一次模型周期，观察同步失败需要恢复已关闭的准入。 */
    model = (struct freeze_model){0};
    require(begin_freeze(&model) && advance(&model, 1) == 1, "第二次到达同步前");
    require(advance(&model, 0) == -1 && model.phase == 0, "同步失败回到正常准入");
    require(enter(&model, 0) && enter(&model, 1), "失败回退后前两层重新允许进入");
    puts("失败分支：回退准入状态，不把失败记录成冻结成功");
    return EXIT_SUCCESS;
}
