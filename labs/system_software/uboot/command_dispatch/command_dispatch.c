#include <stdio.h>
#include <string.h>

struct command;
typedef int (*command_fn)(struct command *, int, char *const []);
typedef int (*repeat_fn)(struct command *, int, char *const [], int *);

/* 教学记录：保留两层回调，省略真实 U-Boot 的 flags 和补全接口。 */
struct command {
    const char *name;
    int maxargs;
    command_fn execute;
    repeat_fn execute_with_repeat;
    const char *usage;
};

static int repeat_yes(struct command *entry, int argc, char *const argv[],
                      int *repeatable)
{
    *repeatable = 1;
    return entry->execute(entry, argc, argv);
}

static int repeat_no(struct command *entry, int argc, char *const argv[],
                     int *repeatable)
{
    *repeatable = 0;
    return entry->execute(entry, argc, argv);
}

static int do_show(struct command *entry, int argc, char *const argv[])
{
    (void)entry;
    if (argc != 2)
        return -1;
    printf("show: %s\n", argv[1]);
    return 0;
}

static int do_shutdown(struct command *entry, int argc, char *const argv[])
{
    (void)entry;
    (void)argc;
    (void)argv;
    puts("shutdown model only: no hardware action");
    return 0;
}

static struct command commands[] = {
    {"show", 2, do_show, repeat_yes, "show text"},
    {"shutdown", 1, do_shutdown, repeat_no, "shutdown"},
};

static struct command *lookup(const char *name)
{
    struct command *candidate = NULL;
    size_t length = strlen(name);
    unsigned int matches = 0;

    if (!length)
        return NULL;
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        if (strncmp(name, commands[i].name, length))
            continue;
        if (length == strlen(commands[i].name))
            return &commands[i];
        candidate = &commands[i];
        ++matches;
    }
    return matches == 1 ? candidate : NULL;
}

int main(int argc, char **argv)
{
    struct command *entry;
    int repeatable = 0;
    int result;

    if (argc < 2) {
        puts("usage: command_dispatch command [argument]");
        return 2;
    }
    --argc;
    ++argv; /* 当前 argv[0] 变成教学命令名。 */
    entry = lookup(argv[0]);
    if (!entry) {
        puts("unknown or ambiguous command");
        return 1;
    }
    if (argc > entry->maxargs)
        result = -1;
    else
        result = entry->execute_with_repeat(entry, argc, argv, &repeatable);
    if (result == -1) {
        printf("usage: %s\n", entry->usage);
        return 1;
    }
    printf("result=%d repeatable=%d\n", result, repeatable);
    return result ? 1 : 0;
}
