#ifndef LESSON00_SERIAL_H
#define LESSON00_SERIAL_H
#include <stddef.h>
#include <sys/types.h>

int serial_baud_supported(unsigned baud);
/* Linux raw 8N1，无软/硬流控。成功返回非阻塞 fd；失败 -1 并设置 errno。 */
int serial_open(const char *path, unsigned baud);
/* 在总期限内全部交给内核，成功 0；失败 -1，可能已经发送部分数据。 */
int serial_write_all(int fd, const void *data, size_t size, int timeout_ms);
/* 返回本次读取的字节数；0 表示超时；-1 表示错误或断开。不组帧。 */
ssize_t serial_read_some(int fd, void *data, size_t capacity, int timeout_ms);
#endif
