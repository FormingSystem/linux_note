#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    char dir[] = "/tmp/vfs_mmap_XXXXXX";
    char path[sizeof(dir) + 16];
    unsigned char *shared = MAP_FAILED, *private = MAP_FAILED;
    int fd = -1, created = 0, result = EXIT_FAILURE, status;
    long page_size = sysconf(_SC_PAGESIZE);
    size_t length;
    unsigned char byte;
    pid_t child, waited;
    if (page_size <= 0 || page_size > 1024 * 1024) {
        fprintf(stderr, "无法取得适合本实验的页大小\n");
        return EXIT_FAILURE;
    }
    length = (size_t)page_size;
    if (!mkdtemp(dir)) { perror("mkdtemp"); return EXIT_FAILURE; }
    snprintf(path, sizeof(path), "%s/data", dir);
    fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) { perror("open"); goto done; }
    created = 1;
    if (ftruncate(fd, (off_t)length) < 0) { perror("ftruncate"); goto done; }
    shared = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared == MAP_FAILED) { perror("mmap shared"); goto done; }
    private = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (private == MAP_FAILED) { perror("mmap private"); goto done; }
    /* 映射持有自己的后备对象引用；原描述符关闭后仍然可以访问。 */
    if (close(fd) < 0) { fd = -1; perror("close"); goto done; }
    fd = -1;
    shared[0] = 'S';
    if (msync(shared, length, MS_SYNC) < 0) { perror("msync"); goto done; }
    fd = open(path, O_RDWR);
    if (fd < 0) { perror("reopen"); goto done; }
    if (pread(fd, &byte, 1, 0) != 1 || byte != 'S') {
        fprintf(stderr, "共享写入检查失败\n"); goto done;
    }
    /* 不依赖私有映射在首次写入前是否看见外部更新。 */
    private[0] = 'P';
    if (private[0] != 'P' || shared[0] != 'S' ||
        pread(fd, &byte, 1, 0) != 1 || byte != 'S') {
        fprintf(stderr, "私有写入检查失败\n"); goto done;
    }
    puts("关闭原fd后共享写入可见；私有写入没有改动文件");
    if (ftruncate(fd, 0) < 0) { perror("truncate to zero"); goto done; }
    child = fork();
    if (child < 0) { perror("fork"); goto done; }
    if (child == 0) {
        struct rlimit core_limit = {0, 0};
        struct sigaction action = {0};
        sigset_t unblock;
        /* 子进程只探测已截断的共享页，不在父进程中恢复故障指令。 */
        if (setrlimit(RLIMIT_CORE, &core_limit) < 0) _exit(2);
        action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask);
        sigemptyset(&unblock);
        sigaddset(&unblock, SIGBUS);
        if (sigaction(SIGBUS, &action, NULL) < 0 ||
            sigprocmask(SIG_UNBLOCK, &unblock, NULL) < 0) _exit(2);
        byte = *(volatile unsigned char *)shared;
        (void)byte;
        _exit(3);
    }
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    if (waited < 0) { perror("waitpid"); goto done; }
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGBUS) {
        fprintf(stderr, "没有观察到预期SIGBUS，wait状态=%d\n", status); goto done;
    }
    puts("截断到零后，仍存在的映射访问收到SIGBUS");
    result = EXIT_SUCCESS;
done:
    /* 只清理由本次实验创建的映射、文件和私有目录。 */
    if (private != MAP_FAILED && munmap(private, length) < 0) {
        perror("munmap private"); result = EXIT_FAILURE;
    }
    if (shared != MAP_FAILED && munmap(shared, length) < 0) {
        perror("munmap shared"); result = EXIT_FAILURE;
    }
    if (fd >= 0 && close(fd) < 0) { perror("close"); result = EXIT_FAILURE; }
    if (created && unlink(path) < 0) { perror("unlink"); result = EXIT_FAILURE; }
    if (rmdir(dir) < 0) { perror("rmdir"); result = EXIT_FAILURE; }
    return result;
}
