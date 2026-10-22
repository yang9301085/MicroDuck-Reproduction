#include "ping.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
static unsigned checks;
static void check(bool condition, const char *name)
{
    ++checks;
    printf("%s: %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) { ++failures; }
}

int main(void)
{
    /* 独立预期值来自手册第 3、4 页，不由待测函数生成。 */
    const uint8_t request[] = {0xFF, 0xFF, 0x01, 0x02, 0x01, 0xFB};
    const uint8_t response[] = {0xFF, 0xFF, 0x01, 0x02, 0x00, 0xFC};
    const uint8_t bad_request[] = {0xFF, 0xFF, 0x01, 0x02, 0x01, 0xFA};
    const uint8_t bad_response[] = {0xFF, 0xFF, 0x01, 0x02, 0x00, 0xFD};
    const uint8_t bad_header[] = {0xFE, 0xFF, 0x01, 0x02, 0x00, 0xFC};
    const uint8_t bad_length[] = {0xFF, 0xFF, 0x01, 0x03, 0x00, 0xFB};
    uint8_t actual[PING_FRAME_SIZE] = {0};
    build_ping_id1(actual);
    check(memcmp(actual, request, sizeof request) == 0, "build ID 1 PING");
    check(ping_frame_checksum_valid(request, sizeof request), "valid request");
    check(ping_frame_checksum_valid(response, sizeof response), "valid response");
    check(!ping_frame_checksum_valid(bad_request, sizeof bad_request), "reject bad request checksum");
    check(!ping_frame_checksum_valid(bad_response, sizeof bad_response), "reject bad response checksum");
    check(!ping_frame_checksum_valid(bad_header, sizeof bad_header), "reject bad header");
    check(!ping_frame_checksum_valid(bad_length, sizeof bad_length), "reject wrong Length even with valid sum");
    check(!ping_frame_checksum_valid(response, sizeof response - 1), "reject truncated frame");
    check(!ping_frame_checksum_valid(NULL, 0), "reject null input");
    printf("%u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
