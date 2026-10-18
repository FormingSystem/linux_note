/* SPL 教学实现；正常情况下不会与 proper.c 链入同一产物。 */
const char *board_stage(void)
{
    return "spl";
}
