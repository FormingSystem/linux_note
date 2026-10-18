#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifndef ARENA_BYTES
#define ARENA_BYTES 0x400u
#endif

struct arena_state {
    size_t base;
    size_t limit;
    size_t cursor;
};

static bool take_bytes(struct arena_state *arena, size_t bytes, size_t *offset)
{
    /* 教学模型采用 8 字节粒度，先检查剩余容量，失败不推进游标。 */
    if (!bytes || arena->cursor > arena->limit ||
        bytes > arena->limit - arena->cursor)
        return false;
    size_t end = arena->cursor + bytes;
    size_t padding = (8u - end % 8u) % 8u;
    if (padding > arena->limit - end)
        return false;
    *offset = arena->base + arena->cursor;
    arena->cursor = end + padding;
    return true;
}

static int run_layout(size_t arena_bytes)
{
    unsigned char memory[0x1000];
    const size_t top = sizeof(memory);
    const size_t gd_bytes = 0x128; /* 假设值，不是实际 U-Boot 的 sizeof(gd_t)。 */
    if (arena_bytes > top || gd_bytes > top - arena_bytes)
        return 1;
    const size_t gd_base = (top - arena_bytes - gd_bytes) & ~(size_t)15;
    const size_t gd_rounded = (gd_bytes + 15u) & ~(size_t)15;
    const size_t arena_base = gd_base + gd_rounded;
    if (gd_base < 0x100 || arena_base > top || arena_bytes > top - arena_base)
        return 1; /* 本模型至少为下方栈区留下 256 字节。 */

    memset(memory, 0xa5, sizeof(memory));
    /* 只清 GD 的假设区间，模拟早期清 GD 与整个 BSS 清零的区别。 */
    memset(memory + gd_base, 0, gd_bytes);
    printf("gd=0x%zx arena=[0x%zx,0x%zx) stack_top=0x%zx\n",
           gd_base, arena_base, arena_base + arena_bytes, gd_base);
    printf("gd_first=%02x other_byte=%02x\n",
           (unsigned)memory[gd_base], (unsigned)memory[0]);

    struct arena_state arena = {arena_base, arena_bytes, 0};
    size_t first = 0, second = 0, unused = 0;
    if (!take_bytes(&arena, 13, &first) || !take_bytes(&arena, 13, &second))
        return 2;
    printf("first=0x%zx second=0x%zx used=%zu\n", first, second, arena.cursor);
    const size_t before = arena.cursor;
    const bool accepted = take_bytes(&arena, arena_bytes, &unused);
    assert(!accepted && arena.cursor == before);
    printf("oversize=rejected cursor_unchanged=%s\n",
           arena.cursor == before ? "yes" : "no");
    return 0;
}

int main(void)
{
    return run_layout(ARENA_BYTES);
}
