/*
 * xinputedge.h — XInputEdge ライブラリ 統一エントリポイント
 *
 * 利用側はこのヘッダ 1 本をインクルードするだけで
 * 全公開 API・プロトコル定数・型にアクセスできます。
 *
 *   #include <xinputedge/xinputedge.h>
 */
#ifndef XINPUTEDGE_H
#define XINPUTEDGE_H

#include "export.h"  /* XINPUTEDGE_API マクロ */
#include "server.h"  /* XieServer / XieState / XieButtons / xie_server_* */

/*
 * XIE Protocol 定数・型を公開 API の一部として再エクスポート。
 * 利用側が xie_protocol.h のパスを意識せずに済む。
 */
#include "../../protocol/xie_protocol.h"

#endif /* XINPUTEDGE_H */
