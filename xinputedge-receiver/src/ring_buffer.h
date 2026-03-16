#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "../protocol/xie_protocol.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 約 64ms 履歴（1kHz 時）
#define XIE_RING_BUFFER_SIZE 64

// ジッター吸収のための遅延（推奨値5。状況に応じて変更可能）
#define XIE_DEJITTER_DELAY 5

typedef struct {
  XiePacket buffer[XIE_RING_BUFFER_SIZE];
  uint16_t newest_sample_id;
  int has_data;
} XieRingBuffer;

// バッファの初期化
void xie_ring_buffer_init(XieRingBuffer *rb);

// 新しいパケットをバッファに書き込む
void xie_ring_buffer_write(XieRingBuffer *rb, const XiePacket *packet);

// 最新から遅延させた対象パケットを読み出す（de-jitter処理）
// 戻り値: データが取得できた場合は1、取得できなかった場合は0
int xie_ring_buffer_read(const XieRingBuffer *rb, XiePacket *out_packet);

#ifdef __cplusplus
}
#endif

#endif // RING_BUFFER_H
