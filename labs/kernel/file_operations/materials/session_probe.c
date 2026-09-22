/* 通过 fdinfo 比较 dup 共享的打开对象与独立 open 的对象。 */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int flushes(int fd, unsigned int *value)
{
    char path[64], line[128];
    FILE *info;
    int found = 0;
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", fd);
    info = fopen(path, "r");
    if (!info) { perror("fopen fdinfo"); return 0; }
    while (fgets(line, sizeof(line), info)) {
        if (sscanf(line, "note_flushes: %u", value) == 1) {
            found = 1;
            break;
        }
    }
    if (ferror(info)) { perror("read fdinfo"); found = 0; }
    if (fclose(info) != 0) { perror("fclose fdinfo"); found = 0; }
    if (!found)
        fputs("未取得 note_flushes 计数\n", stderr);
    return found;
}

static int close_one(int *fd)
{
    int owned = *fd;
    *fd = -1; /* Linux close 错误后不重复关闭可能已被复用的数字。 */
    if (owned >= 0 && close(owned) < 0) {
        perror("close");
        return 0;
    }
    return 1;
}

int main(void)
{
    int first = -1, alias = -1, separate = -1, result = 1;
    unsigned int shared_count, separate_count;
    first = open("/dev/note_session", O_RDONLY);
    if (first < 0) { perror("open first"); goto out; }
    alias = dup(first);
    if (alias < 0) { perror("dup"); goto out; }
    separate = open("/dev/note_session", O_RDONLY);
    if (separate < 0) { perror("open separate"); goto out; }
    if (!flushes(alias, &shared_count) || !flushes(separate, &separate_count))
        goto out;
    printf("before: %u %u\n", shared_count, separate_count);
    if (shared_count != 0 || separate_count != 0)
        goto out;
    if (!close_one(&first) || !flushes(alias, &shared_count) ||
        !flushes(separate, &separate_count))
        goto out;
    printf("after: %u %u\n", shared_count, separate_count);
    result = shared_count == 1 && separate_count == 0 ? 0 : 1;
out:
    if (!close_one(&first)) result = 1;
    if (!close_one(&alias)) result = 1;
    if (!close_one(&separate)) result = 1;
    return result;
}
