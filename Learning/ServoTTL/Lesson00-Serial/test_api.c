#define _POSIX_C_SOURCE 200809L
#include "serial.h"
#include <errno.h>
#include <pty.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static unsigned failures;
static void check(int ok, const char *name)
{
    printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) { ++failures; }
}
static double seconds(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) < 0) { return -1; }
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
int main(void)
{
    int master, slave;
    char name[128];
    if (openpty(&master, &slave, name, NULL, NULL) < 0) { perror("openpty"); return 1; }
    int fd = serial_open(name, 115200); /* PTY 测试参数，不是硬件参数。 */
    if (fd < 0) { perror("serial_open"); return 1; }
    static unsigned char large[1024 * 1024];
    double start = seconds();
    int rc = serial_write_all(fd, large, sizeof large, 150);
    int error = errno;
    double elapsed = seconds() - start;
    check(rc == -1 && error == ETIMEDOUT && elapsed >= 0.12 && elapsed < 1.5,
          "backpressure uses a bounded total write deadline");
    (void)close(master);
    errno = 0;
    rc = serial_write_all(fd, large, 1, 150);
    check(rc == -1 && errno != ETIMEDOUT, "write detects disconnected peer");
    errno = 0;
    check(serial_read_some(-1, large, 1, 100) == -1 && errno == EINVAL,
          "read rejects invalid descriptor");
    errno = 0;
    check(serial_write_all(fd, NULL, 1, 100) == -1 && errno == EINVAL,
          "write rejects null buffer");
    (void)close(fd);
    (void)close(slave);
    printf("4 checks, %u failures\n", failures);
    return failures ? 1 : 0;
}
