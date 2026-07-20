---
id: knowledge.kernel_subsystems.vfs.debugging
title: "VFS 调试与源码追踪"
kind: subsystem
status: evolving
domains: [linux, kernel, filesystem, debugging]
---

# 第25章\_VFS\_调试与源码追踪

## 25.1\_按状态链定位，不从错误字符串猜

```mermaid
flowchart LR
    TYPE["文件系统已注册？"] --> MNT["superblock/mount 已建立？"]
    MNT --> PATH["路径在哪个 namespace 解析？"]
    PATH --> OPEN["open 在哪阶段失败？"]
    OPEN --> FD["fd 指向哪个 file？"]
    FD --> IO["I/O 进入哪组 fops/aops？"]
    IO --> WB["数据在缓存、writeback 还是后端？"]
    WB --> LIFE["哪个引用阻止 close/umount？"]
```

## 25.2\_用户空间观察入口

- `/proc/filesystems`：已注册类型；
- `/proc/self/mountinfo`：当前 namespace 的 mount ID、父子、根和传播状态；
- `/proc/<pid>/fd` 与 `fdinfo`：fd 指向对象及部分 file 状态；
- `stat/statx`：inode、类型、设备号和时间等；
- `strace`：系统调用参数、返回值和 errno；
- `findmnt`、`namei`、`lsof`：挂载、路径分量和打开对象辅助观察。

## 25.3\_内核跟踪

根据问题选择 syscall、vfs、filemap、writeback、block、fsnotify 等 tracepoint，或用 ftrace/function graph 追踪具体调用链。动态调试适合文件系统自身日志；BPF 可关联进程、file/inode 和延迟，但探针读取仍要遵守内核对象有效性。

## 25.4\_典型故障路线

- `ENODEV/unknown filesystem`：先查类型注册与模块加载；
- `ENOENT/ELOOP/EXDEV`：沿起点、每个 dentry、符号链接与 mount 边界；
- open 成功但 I/O 异常：核对 `file->f_op`、位置、页缓存或 direct 路径；
- `fsync` 报错：追踪 writeback 错误和文件系统提交协议；
- `umount: busy`：查 cwd/root、打开 file、VMA、子 mount 和内核 path 引用；
- close 后资源不释放：确认是否仍有 dup/fork fd、VMA、异步 I/O 或缓存对象。

## 25.5\_源码阅读地图

| 机制 | Linux 6.12 主要文件 |
| --- | --- |
| 类型与挂载 | `fs/filesystems.c`、`fs/fs_context.c`、`fs/super.c`、`fs/namespace.c` |
| 名称与路径 | `fs/dcache.c`、`fs/namei.c` |
| open/fd/file | `fs/open.c`、`fs/file.c`、`fs/file_table.c` |
| I/O 与缓存 | `fs/read_write.c`、`mm/filemap.c` |
| 写回与同步 | `fs/fs-writeback.c`、`mm/page-writeback.c`、`fs/sync.c` |
| 通知 | `fs/notify/` |

完成本章后，应能从任一用户文件操作同时回答：路径从哪里开始、命中了哪些 VFS 对象、状态由谁维护、回调进入哪个实现、异步完成如何通知、最后由哪个引用决定释放。
