#ifndef LESSON01_PING_H
#define LESSON01_PING_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { PING_FRAME_SIZE = 6 };
/* out 必须指向至少六字节的可写缓冲区。 */
void build_ping_id1(uint8_t out[PING_FRAME_SIZE]);
/* 仅检查无参数六字节帧的帧头、长度和校验和，不判断业务含义。 */
bool ping_frame_checksum_valid(const uint8_t *frame, size_t size);
#endif
