#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "../protocol/xie_protocol.h"
#include <stdint.h>

#ifdef __cplusplus
#include <atomic>
#define XIE_ATOMIC(type) std::atomic<type>
#else
#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#define XIE_ATOMIC(type) _Atomic type
#else
#error "XInputEdge requires C11 with stdatomic.h support for lock-free ring buffer."
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 約 64ms 履歴（1kHz 時）
#define XIE_RING_BUFFER_SIZE 64

// ジッター吸収のための遅延（推奨値5。状況に応じて変更可能）
#define XIE_DEJITTER_DELAY 5

typedef struct {
  XiePacket packet;
  XIE_ATOMIC(uint32_t) sequence;
} XieRingSlot;

typedef struct {
  XieRingSlot slots[XIE_RING_BUFFER_SIZE];
  XIE_ATOMIC(uint16_t) newest_sample_id;
  XIE_ATOMIC(int) has_data;
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
