#define MUST_USE_RESULT \
    __attribute__((__warn_unused_result__))

static int parse_record(const char *text) MUST_USE_RESULT;

static int parse_record(const char *text)
{
    /* 返回1表示输入中至少存在一个字符。 */
    return text != 0 && text[0] != '\0';
}

static int import_record(const char *text)
{
    /* 正确路径消费了返回值。 */
    if (!parse_record(text))
        return -1;

    return 0;
}

#ifdef BAD_UNUSED_RESULT
static void broken_import(const char *text)
{
    /* 故意忽略返回值，GCC可在构建阶段给出警告。 */
    parse_record(text);
}
#endif
