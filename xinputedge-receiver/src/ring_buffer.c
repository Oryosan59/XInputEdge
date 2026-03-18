#include "ring_buffer.h"
#include <string.h>

void xie_ring_buffer_init(XieRingBuffer *rb) {
  if (!rb)
    return;
  for (int i = 0; i < XIE_RING_BUFFER_SIZE; i++) {
    memset(&rb->slots[i].packet, 0, sizeof(XiePacket));

    // sequenceは偶数=安定状態 / 奇数=書き込み中（Seqlock）
    // 初期状態は「完成済み」として0（偶数）にする
    atomic_init(&rb->slots[i].sequence, 0);
  }

  // newest_sample_id:
  // 単なる「最新ヒント」。厳密な整合性はslot側のSeqlockで担保される
  atomic_init(&rb->newest_sample_id, 0);

  // 初回未受信ガード（API的な明示性のため）
  atomic_init(&rb->has_data, 0);
}

void xie_ring_buffer_write(XieRingBuffer *rb, const XiePacket *packet) {
  if (!rb || !packet)
    return;

  // sample_idはuint16_tのためオーバーフローするが問題なし（リング前提設計）
  int index = packet->sample_id % XIE_RING_BUFFER_SIZE;
  XieRingSlot *slot = &rb->slots[index];

  // 奇数化（書き込み中フラグ）
  // RelaxedでOK（順序は不要、「状態」だけ示す）
  atomic_fetch_add_explicit(&slot->sequence, 1, memory_order_relaxed);

  // 非アトミックコピー（22byte）
  // 途中読み取りはあり得るがSeqlockで検出される
  slot->packet = *packet;

  // 偶数化（書き込み完了）
  // Releaseによりpacketの書き込み完了を可視化
  atomic_fetch_add_explicit(&slot->sequence, 1, memory_order_release);
  // --- publish ---

  // newestは「最後に更新」することで
  // 読み手がnewestを見た時、slotはほぼ確定状態になる
  atomic_store_explicit(&rb->newest_sample_id, packet->sample_id, memory_order_release);

  // 初回受信フラグ
  atomic_store_explicit(&rb->has_data, 1, memory_order_release);
}

int xie_ring_buffer_read(const XieRingBuffer *rb, XiePacket *out_packet) {
  if (!rb || !out_packet)
    return 0;

  // データ未到着ガード（なくても動くがAPIとして明確化）
  if (!atomic_load_explicit(&rb->has_data, memory_order_acquire))
    return 0;

  // newestは「ヒント」であり厳密保証ではない
  // 最終整合性はSeqlockで確認する
  uint16_t newest = atomic_load_explicit(&rb->newest_sample_id, memory_order_acquire);

  // De-jitter遅延（wrap前提）
  uint16_t target = newest - XIE_DEJITTER_DELAY;

  // NOTE:
  // XIE_RING_BUFFER_SIZEは2の冪が望ましい
  // → その場合 index = target & (SIZE-1) に最適化可能
  // ※ただし本実装では22byteのコピーなので性能差は軽微
  int index = target % XIE_RING_BUFFER_SIZE;

  const XieRingSlot *slot = &rb->slots[index];

  // Seqlock 読み込みループ
  uint32_t seq1, seq2;
  int retries = 0;

  // リアルタイム性重視のため短いリトライ
  const int max_retries = 3;

  do {
    // Acquire:
    // 以降のpacket読み取りとの順序を保証
    seq1 = atomic_load_explicit(&slot->sequence, memory_order_acquire);

    // 奇数＝書き込み中 → 破損の可能性があるためリトライ
    if (seq1 & 1) {
      retries++;

      // NOTE:
      // 高競合時は _mm_pause() / yield 命令を入れると
      // HT環境でのCPU効率が改善する可能性あり
      continue;
    }

    // データをコピー (非アトミックな22バイト読み込み)
    *out_packet = slot->packet;

    // もう一度Seqを読む (Acquire: この前のデータ読み込みに対して順序付け)
    seq2 = atomic_load_explicit(&slot->sequence, memory_order_acquire);

    // 前後で一致 → 読み取り中に変更なし（整合性OK）
    if (seq1 == seq2) {
      return 1;
    }

    retries++;
  } while (retries < max_retries);

  // ここに来ることは稀だが（書き込みが読み込みを一瞬で追い越したか、書き込み衝突）
  // データは破損している可能性があるため破棄・失敗扱いとする
  return 0;
}
