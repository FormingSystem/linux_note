#include <stdbool.h>
#include <stdio.h>

/* 教学内存单元：不是MMIO地址，不访问任何GPIO。 */
struct output_cell {
    bool latch;
    bool output_enabled;
};

static void show_output(const char *stage, const struct output_cell *cell)
{
    /* 这里只观察主动输出；高阻输入的实际电平另由外部电路决定。 */
    printf("%s latch=%d driven=%s\n", stage, cell->latch,
           !cell->output_enabled ? "off" : cell->latch ? "1" : "0");
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed: %s\n", #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    unsigned int data = 0;
    unsigned int snapshot_a, snapshot_b;
    struct output_cell cell = { .latch = true, .output_enabled = false };

    /* A独占line0，B独占line1；合法所有权没有阻止共享字的交错。 */
    snapshot_a = data;
    snapshot_b = data;
    data = snapshot_a | 1u;
    data = snapshot_b | 2u;
    printf("interleaved data=%u (wanted 3)\n", data);
    CHECK(data == 2u);

    /* 模拟锁保护下的完整事务序列；这里没有实现真正的并发锁。 */
    data = 0;
    snapshot_a = data;
    data = snapshot_a | 1u;
    snapshot_b = data;
    data = snapshot_b | 2u;
    printf("serialized data=%u\n", data);
    CHECK(data == 3u);

    /* 模拟两个硬件置位命令的契约；|=本身不是C并发原子操作。 */
    data = 0;
    data |= 1u;
    data |= 2u;
    printf("set-bit commands data=%u\n", data);
    CHECK(data == 3u);

    /* 假定硬件允许在输出关闭时准备锁存值，目标输出是0。 */
    show_output("wrong before", &cell);
    cell.output_enabled = true;
    show_output("wrong enable first", &cell);
    CHECK(cell.output_enabled && cell.latch);
    cell.latch = false;
    show_output("wrong finally", &cell);

    cell = (struct output_cell){ .latch = true, .output_enabled = false };
    cell.latch = false;
    show_output("right prepare first", &cell);
    CHECK(!cell.output_enabled && !cell.latch);
    cell.output_enabled = true;
    show_output("right enable", &cell);
    CHECK(cell.output_enabled && !cell.latch);

    puts("all transaction model checks passed");
    return 0;
}
