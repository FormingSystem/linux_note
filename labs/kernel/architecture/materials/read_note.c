#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    FILE *input_file;
    char buffer[16] = {0};
    size_t bytes_read;

    /* 参数中只接收一个文件路径。 */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    input_file = fopen(argv[1], "rb");
    if (input_file == NULL) {
        perror("open input");
        return EXIT_FAILURE;
    }

    /* 留出一个零字节，便于本例显示短文本。 */
    bytes_read = fread(buffer, 1, sizeof(buffer) - 1, input_file);
    if (ferror(input_file)) {
        fprintf(stderr, "read input failed\n");
        fclose(input_file);
        return EXIT_FAILURE;
    }

    if (fclose(input_file) == EOF) {
        perror("close input");
        return EXIT_FAILURE;
    }

    printf("bytes=%zu\ntext=%s\n", bytes_read, buffer);
    return EXIT_SUCCESS;
}
