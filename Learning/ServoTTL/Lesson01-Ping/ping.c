#include "ping.h"
void build_ping_id1(uint8_t out[PING_FRAME_SIZE])
{
    out[0] = 0xFF;
    out[1] = 0xFF;
    out[2] = 0x01; /* ID */
    out[3] = 0x02; /* 无参数，Length = 0 + 2 */
    out[4] = 0x01; /* PING 指令 */
    /* 帧头不参与校验；显式保留取反后的低八位。 */
    unsigned sum = (unsigned)out[2] + out[3] + out[4];
    out[5] = (uint8_t)((~sum) & 0xFFu);
}
bool ping_frame_checksum_valid(const uint8_t *frame, size_t size)
{
    if (frame == NULL || size != PING_FRAME_SIZE) {
        return false;
    }
    if (frame[0] != 0xFF || frame[1] != 0xFF || frame[3] != 0x02) {
        return false;
    }
    /* 含校验字节的总和，其低八位应为 FF。 */
    unsigned sum = (unsigned)frame[2] + frame[3] + frame[4] + frame[5];
    return (sum & 0xFFu) == 0xFFu;
}
