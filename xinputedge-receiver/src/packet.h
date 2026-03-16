#ifndef PACKET_H
#define PACKET_H

#include "../protocol/xie_protocol.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 受信パケットの検証（サイズ、マジックナンバー、バージョン、タイプ）
// 戻り値: XIE_OK(0) 正常 / XIE_DROP(-2) 等検証失敗
int xie_packet_validate(const XiePacket *packet, size_t received_size);

#ifdef __cplusplus
}
#endif

#endif // PACKET_H
