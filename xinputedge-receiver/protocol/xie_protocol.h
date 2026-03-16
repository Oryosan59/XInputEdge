#ifndef XIE_PROTOCOL_H
#define XIE_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 基本仕様
 * エンディアン: Little Endian
 * パケットサイズ: 22 bytes
 */

// XIE Packet 識別子とバージョン
#define XIE_MAGIC 0x5849 // "XI"
#define XIE_VERSION 1
#define XIE_TYPE_GAMEPAD 1
#define XIE_PACKET_SIZE 22

// typeAndFlags アクセスマクロ
#define XIE_GET_TYPE(tf) ((tf) & 0x0F)
#define XIE_GET_FLAGS(tf) (((tf) >> 4) & 0x0F)
#define XIE_MAKE_TYPE_FLAGS(t, f) (((uint8_t)(f) << 4) | ((uint8_t)(t) & 0x0F))

// return code 定義
#define XIE_OK 0       // 正常
#define XIE_TIMEOUT -3 // 受信タイムアウト（EAGAIN / EWOULDBLOCK）
#define XIE_ERROR -1   // ソケットエラー
#define XIE_DROP -2    // パケット検証失敗（magic/version/size 不一致）

// flags ビット割り当て (4bit)
#define XIE_FLAG_INPUT_DROP 0x1 // bit0: 送信側で入力取得失敗 / 処理遅延
#define XIE_FLAG_OVERFLOW 0x2   // bit1: 送信側内部キュー溢れ
#define XIE_FLAG_HEARTBEAT 0x4  // bit2: 毎パケットでトグル (通信停止検出)
#define XIE_FLAG_RESERVED 0x8   // bit3: 予約

// 受信タイムアウト値
#define XIE_RECV_TIMEOUT_US 5000 // 5ms

#pragma pack(push, 1)

// XIE Packet 構造体
typedef struct {
  uint16_t magic;        // 0x5849 ("XI") - パケット識別子
  uint8_t version;       // プロトコルバージョン (現在: 1)
  uint8_t typeAndFlags;  // 上位4bit = flags, 下位4bit = type
  uint16_t sample_id;    // サンプルカウンタ (0 -> 65535 -> 0)
  uint32_t timestamp_us; // 送信側モノトニック時刻 (マイクロ秒)
  int16_t lx;            // 左スティック X  (-32768 ~ 32767)
  int16_t ly;            // 左スティック Y  (-32768 ~ 32767)
  int16_t rx;            // 右スティック X  (-32768 ~ 32767)
  int16_t ry;            // 右スティック Y  (-32768 ~ 32767)
  uint8_t lt;            // 左トリガー (0 ~ 255)
  uint8_t rt;            // 右トリガー (0 ~ 255)
  uint16_t buttons;      // ボタン状態 (ビットフラグ)
} XiePacket;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // XIE_PROTOCOL_H
