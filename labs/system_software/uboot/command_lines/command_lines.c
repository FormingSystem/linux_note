#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 教学模型仅接受字面命令，不实现引号、转义和环境变量展开。 */
static int execute_token(char *token)
{
    while (*token == ' ' || *token == '\t')
        ++token;
    printf("execute: [%s]\n", token);
    return strcmp(token, "fail") == 0 || *token == '\0' ? 1 : 0;
}

static int execute_line(const char *input)
{
    char copy[128];
    char *token;
    int failed = 0;

    if (strlen(input) >= sizeof(copy))
        return -1;
    strcpy(copy, input); /* 后续切分只修改这一行的工作副本。 */
    token = copy;
    for (;;) {
        char *separator = strchr(token, ';');
        if (separator != NULL)
            *separator = '\0';
        if (execute_token(token) != 0)
            failed = 1; /* 记住失败，但继续本行的下一条命令。 */
        if (separator == NULL)
            break;
        token = separator + 1;
    }
    return failed ? -1 : 1; /* 模型中所有成功命令均可重复。 */
}

static int execute_list(const char *input)
{
    char *copy = malloc(strlen(input) + 1);
    char *line;
    int result = 0;

    if (copy == NULL)
        return 1;
    strcpy(copy, input);
    line = copy;
    for (;;) {
        char *separator = strchr(line, '\n');
        if (separator != NULL)
            *separator = '\0';
        if (*line != '\0' && execute_line(line) < 0) {
            result = 1;
            break; /* 外层在失败行结束后停止，不撤销已执行命令。 */
        }
        if (separator == NULL)
            break;
        line = separator + 1;
    }
    free(copy);
    return result;
}

int main(int argc, char **argv)
{
    const char *input;
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: %s semicolon|newline|mixed|ok\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "semicolon") == 0)
        input = "first;fail;last";
    else if (strcmp(argv[1], "newline") == 0)
        input = "first\nfail\nlast";
    else if (strcmp(argv[1], "mixed") == 0)
        input = "first;fail;last\nafter";
    else if (strcmp(argv[1], "ok") == 0)
        input = "first\n\nlast";
    else
        return 2;
    result = execute_list(input);
    printf("list result=%d\n", result);
    return result;
}
