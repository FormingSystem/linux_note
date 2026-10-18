#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

struct image_result {
    size_t size;
    size_t entry_offset;
    const char *source;
};

struct candidate {
    const char *name;
    const unsigned char *bytes;
    size_t size;
};

static bool try_load(const struct candidate *candidate,
                     unsigned char *ram, size_t capacity,
                     struct image_result *result)
{
    printf("try %s\n", candidate->name);
    if (candidate->size < 8 || candidate->size > capacity) {
        puts("range rejected before copy");
        return false;
    }
    memcpy(ram, candidate->bytes, candidate->size);
    /* 失败检查发生在复制之后，RAM 已可能改变，不能把失败当成回滚。 */
    if (memcmp(ram, "DEMO", 4) != 0) {
        puts("header rejected after copy");
        return false;
    }
    const size_t entry = ram[4]; /* 教学格式：第五字节是数组内入口偏移。 */
    if (entry < 8 || entry >= candidate->size) {
        puts("entry rejected after copy");
        return false;
    }
    *result = (struct image_result){candidate->size, entry, candidate->name};
    return true;
}

int main(int argc, char **argv)
{
    bool ram_ready = true, fail_all = false;
    if (argc == 2 && strcmp(argv[1], "no-ram") == 0)
        ram_ready = false;
    else if (argc == 2 && strcmp(argv[1], "fail") == 0)
        fail_all = true;
    else if (argc != 1)
        return 2;

    unsigned char ram[64] = {0};
    const unsigned char good[16] = {'D', 'E', 'M', 'O', 8};
    const unsigned char bad[16] = {'B', 'A', 'D', '!', 8};
    struct candidate devices[] = {
        {"primary", bad, sizeof(bad)},
        {"backup", fail_all ? bad : good, sizeof(good)}
    };
    struct image_result result = {0};
    if (!ram_ready) {
        puts("RAM unavailable: no copy, no handoff");
        return 3;
    }
    for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); ++i) {
        if (try_load(&devices[i], ram, sizeof(ram), &result)) {
            printf("handoff model: source=%s size=%zu entry=%zu\n",
                   result.source, result.size, result.entry_offset);
            return 0; /* 只报告交接条件，不把数组当机器码执行。 */
        }
    }
    printf("all failed: RAM starts with %c, no handoff\n", ram[0]);
    return 4;
}
