#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 数值地址模型：这些整数不作为宿主机指针解引用。 */
struct image {
    uint64_t words[4];
};

struct relative_record {
    uint64_t location;
    uint64_t addend;
};

static int apply_relative(struct image *image, uint64_t link_base,
                          uint64_t run_base, struct relative_record record)
{
    uint64_t offset;

    /* 教学模型先检查写入范围；不是声称上游汇编做了这些检查。 */
    if (record.location < link_base)
        return -1;
    offset = record.location - link_base;
    if (offset % sizeof(uint64_t) ||
        offset > sizeof(*image) - sizeof(uint64_t) ||
        run_base < link_base ||
        record.addend > UINT64_MAX - (run_base - link_base))
        return -1;
    image->words[offset / sizeof(uint64_t)] =
        record.addend + (run_base - link_base);
    return 0;
}

int main(int argc, char **argv)
{
    const uint64_t link_base = 0x1000;
    const uint64_t initial_base = 0x2000;
    const uint64_t final_base = 0x5000;
    struct relative_record record = {0x1008, 0x1018};
    struct image initial = {{42, 0x1018, 0x90000000, 7}};
    struct image final;
    uint64_t copied_pointer;

    if (argc > 2 || (argc == 2 && strcmp(argv[1], "bad-record")))
        return 2;
    if (argc == 2) {
        record.location = link_base + sizeof(initial);
        if (apply_relative(&initial, link_base, initial_base, record) != -1)
            return 3;
        if (initial.words[1] != 0x1018)
            return 3;
        puts("record rejected before write");
        return 1;
    }
    if (apply_relative(&initial, link_base, initial_base, record))
        return 3;
    printf("initial: slot=%" PRIx64 " value=%" PRIx64 "\n",
           initial_base + 8, initial.words[1]);

    /* 复制保留旧指针值；第二次修正仍从记录中的原始 addend 计算。 */
    final = initial;
    copied_pointer = final.words[1];
    if (apply_relative(&final, link_base, final_base, record))
        return 3;
    printf("final: slot=%" PRIx64 " value=%" PRIx64 "\n",
           final_base + 8, final.words[1]);
    printf("wrong double adjustment=%" PRIx64 "\n",
           copied_pointer + (final_base - link_base));
    printf("integer=%" PRIu64 " external=%" PRIx64 " payload=%" PRIu64 "\n",
           final.words[0], final.words[2], final.words[3]);
    return final.words[1] == 0x5018 && final.words[0] == 42 &&
           final.words[2] == 0x90000000 && final.words[3] == 7 ? 0 : 3;
}
