#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "serial.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static speed_t baud_value(unsigned baud)
{
    switch (baud) {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    case 921600: return B921600;
    case 1000000: return B1000000;
    default: return (speed_t)0;
    }
}

int serial_baud_supported(unsigned baud) { return baud_value(baud) != 0; }

int serial_open(const char *path, unsigned baud)
{
    speed_t speed = baud_value(baud);
    if (path == NULL || speed == 0) { errno = EINVAL; return -1; }
    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { return -1; }
    struct termios config;
    if (tcgetattr(fd, &config) < 0) { goto fail; }
    cfmakeraw(&config); /* 禁止回显、行缓冲及特殊字符转换。 */
    config.c_cflag &= (tcflag_t)~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    config.c_cflag |= CS8 | CLOCAL | CREAD;
    config.c_iflag &= (tcflag_t)~(IXON | IXOFF | IXANY);
    config.c_cc[VMIN] = 0;
    config.c_cc[VTIME] = 0; /* 超时统一交给 poll，不混用两套计时。 */
    if (cfsetispeed(&config, speed) < 0 || cfsetospeed(&config, speed) < 0 ||
        tcsetattr(fd, TCSANOW, &config) < 0) { goto fail; }
    return fd;
fail: {
    int saved = errno;
    (void)close(fd);
    errno = saved;
    return -1;
    }
}

static int64_t now_ms(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) < 0) { return -1; }
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* 1=就绪，0=超时，-1=错误。EINTR 后沿用原期限，不重新计时。 */
static int wait_ready(int fd, short events, int64_t deadline)
{
    for (;;) {
        int64_t now = now_ms();
        if (now < 0) { return -1; }
        if (now >= deadline) { return 0; }
        struct pollfd p = {.fd = fd, .events = events, .revents = 0};
        int rc = poll(&p, 1, (int)(deadline - now));
        if (rc < 0) { if (errno == EINTR) { continue; } return -1; }
        if (rc == 0) { return 0; }
        if (p.revents & POLLNVAL) { errno = EBADF; return -1; }
        if (p.revents & events) { return 1; }
        if (p.revents & (POLLERR | POLLHUP)) { errno = EIO; return -1; }
    }
}

int serial_write_all(int fd, const void *data, size_t size, int timeout_ms)
{
    if (fd < 0 || data == NULL || timeout_ms <= 0) { errno = EINVAL; return -1; }
    int64_t start = now_ms();
    if (start < 0) { return -1; }
    const unsigned char *bytes = data;
    size_t sent = 0;
    while (sent < size) {
        int ready = wait_ready(fd, POLLOUT, start + timeout_ms);
        if (ready <= 0) { if (ready == 0) { errno = ETIMEDOUT; } return -1; }
        ssize_t n = write(fd, bytes + sent, size - sent);
        if (n > 0) { sent += (size_t)n; continue; }
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) { continue; }
        if (n == 0) { errno = EIO; }
        return -1;
    }
    return 0;
}

ssize_t serial_read_some(int fd, void *data, size_t capacity, int timeout_ms)
{
    if (fd < 0 || data == NULL || capacity == 0 || timeout_ms <= 0) { errno = EINVAL; return -1; }
    int64_t start = now_ms();
    if (start < 0) { return -1; }
    for (;;) {
        int ready = wait_ready(fd, POLLIN, start + timeout_ms);
        if (ready <= 0) { return ready; }
        ssize_t n = read(fd, data, capacity);
        if (n > 0) { return n; }
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) { continue; }
        if (n == 0) { errno = EIO; } /* 就绪后 EOF 视为断开。 */
        return -1;
    }
}
