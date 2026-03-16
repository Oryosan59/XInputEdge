#ifndef XINPUTEDGE_SERVER_H
#define XINPUTEDGE_SERVER_H

#include <stdint.h>
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* XIE Device — ボタン定義                                             */
/* ------------------------------------------------------------------ */
typedef enum {
  XIE_BTN_A          = 0x1000,
  XIE_BTN_B          = 0x2000,
  XIE_BTN_X          = 0x4000,
  XIE_BTN_Y          = 0x8000,

  XIE_BTN_LB         = 0x0100,
  XIE_BTN_RB         = 0x0200,

  XIE_BTN_START      = 0x0010,
  XIE_BTN_BACK       = 0x0020,

  XIE_BTN_DPAD_UP    = 0x0001,
  XIE_BTN_DPAD_DOWN  = 0x0002,
  XIE_BTN_DPAD_LEFT  = 0x0004,
  XIE_BTN_DPAD_RIGHT = 0x0008,
} XieButtons;

/* ------------------------------------------------------------------ */
/* XIE Client が返す入力状態                                            */
/* ------------------------------------------------------------------ */
typedef struct {
  int16_t lx;
  int16_t ly;
  int16_t rx;
  int16_t ry;
  uint8_t lt;
  uint8_t rt;
  uint16_t buttons;
} XieState;

/* ------------------------------------------------------------------ */
/* XIE Server — 不透明ハンドル (opaque handle)                         */
/* 構造体の中身は src/ 側だけで定義。将来フィールドを追加しても ABI 非破壊。*/
/* ------------------------------------------------------------------ */
typedef struct XieServer XieServer;

/* ライフサイクル */
XINPUTEDGE_API XieServer *xie_server_create(void);
XINPUTEDGE_API void       xie_server_destroy(XieServer *s);

/* 初期化 (bind_ip が NULL/空 の場合は 0.0.0.0) */
XINPUTEDGE_API int xie_server_init(XieServer *s, const char *bind_ip, int port);

/* UDP 受信 → バッファ更新（ネットワークスレッドから呼ぶ） */
XINPUTEDGE_API int xie_server_recv(XieServer *s);

/* 制御ループから最新状態を取得 */
XINPUTEDGE_API const XieState *xie_server_state(const XieServer *s);

/* （デバッグ用）最後に受信した生の XIE Packet を取得 */
XINPUTEDGE_API const void *xie_server_latest_packet(const XieServer *s);

/* サンプルロス数を取得 */
XINPUTEDGE_API uint32_t xie_server_lost(const XieServer *s);

/* 通信停止チェック（戻り値 1 = 停止中） */
XINPUTEDGE_API int xie_server_is_timeout(const XieServer *s);

/* 終了 */
XINPUTEDGE_API void xie_server_close(XieServer *s);

/* スリープ用ユーティリティ（1 kHz 制御ループなどで使用） */
XINPUTEDGE_API void xie_sleep_us(uint32_t microseconds);

#ifdef __cplusplus
}
#endif

#endif /* XINPUTEDGE_SERVER_H */
