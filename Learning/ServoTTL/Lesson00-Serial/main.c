#include "serial.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { BUFFER_SIZE = 4096 };

static int number(const char *text, unsigned max, unsigned *out)
{
    if (*text == '\0') { return -1; }
    for (const char *p = text; *p; ++p) {
        if (!isdigit((unsigned char)*p)) { return -1; }
    }
    char *end;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno || *end || value == 0 || value > max) { return -1; }
    *out = (unsigned)value;
    return 0;
}

static int hex_digit(unsigned char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

/* 每个字节必须恰好两个十六进制字符，字节之间以空白分隔。 */
static int parse_hex(const char *text, unsigned char *out, size_t *size)
{
    *size = 0;
    while (*text) {
        while (isspace((unsigned char)*text)) { ++text; }
        if (!*text) { break; }
        int hi = hex_digit((unsigned char)*text++);
        if (!*text) { return -1; }
        int lo = hex_digit((unsigned char)*text++);
        if (hi < 0 || lo < 0 || (*text && !isspace((unsigned char)*text)) || *size == BUFFER_SIZE) { return -1; }
        out[(*size)++] = (unsigned char)(hi * 16 + lo);
    }
    return *size ? 0 : -1;
}

static void log_hex(const char *label, const unsigned char *data, size_t size)
{
    printf("%s:", label);
    for (size_t i = 0; i < size; ++i) { printf(" %02X", (unsigned)data[i]); }
    putchar('\n');
}

int main(int argc, char **argv)
{
    if (argc != 5 && argc != 6) {
        fprintf(stderr, "Usage: %s DEVICE BAUD TIMEOUT_MS MAX_RX [\"HH HH ...\"]\n", argv[0]);
        return 1;
    }
    unsigned baud, timeout, max_rx;
    unsigned char tx[BUFFER_SIZE], rx[BUFFER_SIZE];
    size_t tx_size = 0;
    if (number(argv[2], 1000000, &baud) < 0 || !serial_baud_supported(baud) ||
        number(argv[3], 60000, &timeout) < 0 || number(argv[4], BUFFER_SIZE, &max_rx) < 0 ||
        (argc == 6 && parse_hex(argv[5], tx, &tx_size) < 0)) {
        fprintf(stderr, "arguments: invalid baud, timeout (1..60000), size (1..4096), or hex bytes\n");
        return 1;
    }
    int fd = serial_open(argv[1], baud);
    if (fd < 0) { perror("open/configure"); return 1; }
    puts("READY");
    fflush(stdout);
    int result = 0;
    if (tx_size) {
        if (serial_write_all(fd, tx, tx_size, (int)timeout) < 0) {
            perror("send (partial transmission possible)"); result = 1; goto done;
        }
        log_hex("TX", tx, tx_size);
    }
    ssize_t n = serial_read_some(fd, rx, max_rx, (int)timeout);
    if (n < 0) { perror("receive"); result = 1; }
    else if (n == 0) { puts("RX timeout"); result = 2; }
    else { log_hex("RX", rx, (size_t)n); }
done:
    /* Linux close 失败不重试，避免误关已被复用的文件描述符。 */
    if (close(fd) < 0) { perror("close"); result = 1; }
    return result;
}
