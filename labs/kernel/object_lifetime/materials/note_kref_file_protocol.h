#ifndef NOTE_KREF_FILE_PROTOCOL_H
#define NOTE_KREF_FILE_PROTOCOL_H
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif
/* 两个命令均无指针参数：一次短业务操作，或关闭所有后续业务接纳。 */
#define NOTE_FILE_STEP _IO('K', 0x10)
#define NOTE_FILE_CLOSE _IO('K', 0x11)
#endif
