#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    const uint64_t mib = UINT64_C(1024) * 1024;
    const uint64_t file_size = UINT64_C(6) * 1024 * mib;
    const uint64_t window_size = 256 * mib;
    uint64_t offset = 0;
    unsigned int windows = 0;

    while (offset < file_size) {
        uint64_t remaining = file_size - offset;
        uint64_t length = remaining < window_size ? remaining : window_size;

        /* 实际程序在此映射、处理并解除映射；旧指针不能传到下一轮。 */
        printf("offset=%" PRIu64 " MiB length=%" PRIu64 " MiB\n",
               offset / mib, length / mib);
        offset += length;
        ++windows;
    }
    printf("windows=%u processed=%" PRIu64 " MiB\n", windows, offset / mib);
    return 0;
}
