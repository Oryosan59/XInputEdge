#include "ring_buffer.h"
#include <string.h>

void xie_ring_buffer_init(XieRingBuffer *rb) {
  if (!rb)
    return;
  for (int i = 0; i < XIE_RING_BUFFER_SIZE; i++) {
    memset(&rb->slots[i].packet, 0, sizeof(XiePacket));
    atomic_init(&rb->slots[i].sequence, 0); // 偶数: 初期・完了状態
  }
  atomic_init(&rb->newest_sample_id, 0);
  atomic_init(&rb->has_data, 0);
}

void xie_ring_buffer_write(XieRingBuffer *rb, const XiePacket *packet) {
  if (!rb || !packet)
    return;

  int index = packet->sample_id % XIE_RING_BUFFER_SIZE;
  XieRingSlot *slot = &rb->slots[index];

  // 1. Seqを奇数にする (Release: これより前の更新を可視化。まあただのインクリメントでOK)
  uint32_t seq = atomic_load_explicit(&slot->sequence, memory_order_relaxed);
  atomic_store_explicit(&slot->sequence, seq + 1, memory_order_release);

  // 2. データをコピー (ここでは22バイトの構造体コピー、非アトミック)
  slot->packet = *packet;

  // 3. Seqを偶数にする (Release: パケットコピー完了を可視化)
  atomic_store_explicit(&slot->sequence, seq + 2, memory_order_release);

  // 4. 最新インデックスを更新 (Release: slot側のSeq更新後にnewestが変わるように)
  atomic_store_explicit(&rb->newest_sample_id, packet->sample_id, memory_order_release);
  atomic_store_explicit(&rb->has_data, 1, memory_order_release);
}

int xie_ring_buffer_read(const XieRingBuffer *rb, XiePacket *out_packet) {
  if (!rb || !out_packet)
    return 0;

  if (!atomic_load_explicit(&rb->has_data, memory_order_acquire))
    return 0;

  // de-jitter遅延を適用したサンプルIDを計算
  uint16_t newest = atomic_load_explicit(&rb->newest_sample_id, memory_order_acquire);
  uint16_t target = newest - XIE_DEJITTER_DELAY;
  int index = target % XIE_RING_BUFFER_SIZE;

  const XieRingSlot *slot = &rb->slots[index];

  // Seqlock 読み込みループ
  uint32_t seq1, seq2;
  int retries = 0;
  const int max_retries = 3; // 諦めるまでの回数 (1kHz周期なので競合は稀)

  do {
    // 1. Seqを読む (Acquire: この後のデータ読み込みに対して順序付け)
    seq1 = atomic_load_explicit(&slot->sequence, memory_order_acquire);

    // 奇数なら現在書き込み中なのでリトライ
    if (seq1 % 2 != 0) {
      retries++;
      // 短くyieldするなどの考慮もアリ。一旦続ける
      continue;
    }

    // 2. データをコピー (非アトミックな22バイト読み込み)
    *out_packet = slot->packet;

    // 3. もう一度Seqを読む (Acquire: この前のデータ読み込みに対して順序付け)
    seq2 = atomic_load_explicit(&slot->sequence, memory_order_acquire);

    // 一致していれば読み取り途中の変更はなかった
    if (seq1 == seq2) {
      return 1;
    }

    retries++;
  } while (retries < max_retries);

  // ここに来ることは稀だが（書き込みが読み込みを一瞬で追い越したか、書き込み衝突）
  // データは破損している可能性があるため破棄・失敗扱いとする
  return 0;
}
