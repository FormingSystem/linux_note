#include <stdbool.h>
#include <stdio.h>

/* 宿主教学状态，不是gpio_desc，也不访问真实硬件。 */
struct line_state {
    bool provider_ready;
    unsigned int owner;
    bool active_low;
    bool configured;
    bool hw_output;
    bool hw_raw;
};

enum model_result { MODEL_OK, MODEL_WAIT, MODEL_BUSY, MODEL_CONFIG_ERROR };

static void show_state(const char *stage, const struct line_state *line)
{
    const char *owner = line->owner == 1 ? "A" :
                        line->owner == 2 ? "B" : "none";
    printf("%s owner=%s configured=%d output=%d raw=%d\n", stage, owner,
           line->configured, line->hw_output, line->hw_raw);
}

static enum model_result request_line(struct line_state *line,
                                       unsigned int owner, bool active_low,
                                       bool initial, bool fail_after_write)
{
    /* S2：此模型只支持已经确定的一根线，ready代表提供者是否可用。 */
    if (!line->provider_ready)
        return MODEL_WAIT;
    if (line->owner != 0)
        return MODEL_BUSY;
    /* S3：串行模型的占用登记；此普通赋值不能充当并发请求协议。 */
    line->owner = owner;
    line->active_low = active_low;
    /* S4：先形成物理值，再启用模拟输出。 */
    line->hw_raw = initial != active_low;
    line->hw_output = true;
    if (fail_after_write) {
        /* 仅回滚本次软件占用，刻意保留已发生的模拟硬件变化。 */
        line->owner = 0;
        line->active_low = false;
        line->configured = false;
        return MODEL_CONFIG_ERROR;
    }
    line->configured = true;
    return MODEL_OK;
}

static bool set_line(struct line_state *line, unsigned int owner, bool logical)
{
    if (owner == 0 || line->owner != owner || !line->configured)
        return false;
    line->hw_raw = logical != line->active_low;
    return true;
}

static bool release_line(struct line_state *line, unsigned int owner)
{
    if (owner == 0 || line->owner != owner)
        return false;
    /* 本模型无IRQ/工作队列；真实使用者须先停止这些在途访问。 */
    line->owner = 0;
    line->active_low = false;
    line->configured = false;
    return true;
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    struct line_state line = { .hw_raw = true };

    CHECK(request_line(&line, 1, true, true, false) == MODEL_WAIT);
    CHECK(line.owner == 0 && !line.hw_output && line.hw_raw);
    show_state("S0 waiting", &line);

    line.provider_ready = true; /* S1：模拟登记完成。 */
    CHECK(request_line(&line, 1, true, true, false) == MODEL_OK);
    CHECK(line.owner == 1 && line.configured && !line.hw_raw);
    show_state("S4 reset asserted", &line);

    CHECK(request_line(&line, 2, false, true, false) == MODEL_BUSY);
    CHECK(line.owner == 1 && !line.hw_raw);
    CHECK(!set_line(&line, 2, true));
    show_state("S3 B rejected", &line);

    CHECK(set_line(&line, 1, false));
    CHECK(line.hw_raw);
    show_state("S5 reset deasserted", &line);
    CHECK(!release_line(&line, 2));
    CHECK(release_line(&line, 1));
    CHECK(line.owner == 0 && line.hw_output && line.hw_raw);
    CHECK(!set_line(&line, 1, true));
    show_state("S7 released", &line);

    CHECK(request_line(&line, 2, true, true, true) == MODEL_CONFIG_ERROR);
    CHECK(line.owner == 0 && !line.configured && line.hw_output && !line.hw_raw);
    show_state("S4 failure after write", &line);
    CHECK(request_line(&line, 2, false, true, false) == MODEL_OK);
    CHECK(line.owner == 2 && line.hw_raw);
    CHECK(release_line(&line, 2));
    puts("all model checks passed");
    return 0;
}
