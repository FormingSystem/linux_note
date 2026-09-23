/* C11：比较规则与调用方式分开；不是 rbtree 实现或性能基准。 */
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

struct job_key {
    int deadline;
    unsigned int id;
};

static int compare_deadline(const struct job_key *a, const struct job_key *b)
{
    /* 先比较再相减布尔值，避免直接相减两个有符号键时溢出。 */
    return (a->deadline > b->deadline) - (a->deadline < b->deadline);
}

static int compare_job(const struct job_key *a, const struct job_key *b)
{
    int first = compare_deadline(a, b);
    if (first != 0)
        return first;
    return (a->id > b->id) - (a->id < b->id);
}

static int compare_through_callback(const struct job_key *a,
                                    const struct job_key *b,
                                    int (*compare)(const struct job_key *,
                                                   const struct job_key *))
{
    return compare(a, b);
}

int main(void)
{
    const struct job_key keys[] = {
        {INT_MIN, 0}, {40, 2}, {40, 4}, {INT_MAX, UINT_MAX}
    };
    size_t count = sizeof keys / sizeof keys[0];
    printf("same deadline: %d; full key: %d\n",
           compare_deadline(&keys[1], &keys[2]), compare_job(&keys[1], &keys[2]));
    printf("extreme signed keys: %d\n", compare_job(&keys[0], &keys[3]));
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = 0; j < count; ++j) {
            int direct = compare_job(&keys[i], &keys[j]);
            int callback = compare_through_callback(&keys[i], &keys[j], compare_job);
            /* 表中完整键严格递增，数组下标给出独立的预期符号。 */
            int expected = (i > j) - (i < j);
            assert(direct == expected && callback == expected);
        }
    }
    puts("16 pairs: direct and callback agree; no timing claim");
    return 0;
}
