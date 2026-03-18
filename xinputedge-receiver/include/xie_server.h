#ifndef XIE_SERVER_H
#define XIE_SERVER_H

#include <stdint.h>

#include "xinputedge/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* XIE Device — ボタン定義                                             */
/* ------------------------------------------------------------------ */
typedef enum {
  XIE_BTN_A = 0x1000,
  XIE_BTN_B = 0x2000,
  XIE_BTN_X = 0x4000,
  XIE_BTN_Y = 0x8000,

  XIE_BTN_LB = 0x0100,
  XIE_BTN_RB = 0x0200,

  XIE_BTN_START = 0x0010,
  XIE_BTN_BACK = 0x0020,

  XIE_BTN_DPAD_UP = 0x0001,
  XIE_BTN_DPAD_DOWN = 0x0002,
  XIE_BTN_DPAD_LEFT = 0x0004,
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

XINPUTEDGE_API XieServer *xie_server_create(void);
XINPUTEDGE_API void xie_server_destroy(XieServer *s);

XINPUTEDGE_API int xie_server_init(XieServer *s, const char *bind_ip, int port);

XINPUTEDGE_API int xie_server_get_port(const XieServer *s);

XINPUTEDGE_API int xie_server_recv(XieServer *s);

XINPUTEDGE_API const XieState *xie_server_state(const XieServer *s);

XINPUTEDGE_API const void *xie_server_latest_packet(const XieServer *s);

XINPUTEDGE_API uint32_t xie_server_lost(const XieServer *s);

XINPUTEDGE_API int xie_server_is_timeout(const XieServer *s);

XINPUTEDGE_API void xie_server_close(XieServer *s);

XINPUTEDGE_API void xie_sleep_us(uint32_t microseconds);

#ifdef __cplusplus
}
#endif

#endif /* XIE_SERVER_H */
