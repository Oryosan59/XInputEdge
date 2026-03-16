# xinputedge-receiver (C99)

XIE Protocol を用いて、送信側 (Windows PC) から送られてくる XInput ゲームパッドの入力データを受信するための C99 静的ライブラリです。

## 特徴

- **ノンブロッキング設計**: UDP の待ち受けを独立した「ネットワークスレッド」で行い、ユーザーのメイン制御ループ（1 kHz など）を一切ブロックさせません。
- **De-jitter バッファ**: ネットワーク揺らぎをリングバッファで吸収し、モーター等のハードウェア制御に必要な滑らかで連続した入力状態を提供します。
- **軽量・静的確保**: 実装はすべて静的またはスタック上で完結（初期化ヘルパー `xie_server_create` のみ `malloc` を使用）。
- **フェイルセーフ**: 無通信状態が 50 ms 以上続くと入力状態をニュートラルへ自動リセット。
- **ABI 安定設計**: 不透明ハンドル (opaque handle)パターンにより、将来の内部変更が利用側に影響しません。

## ディレクトリ構成

```
xinputedge-receiver/
├── include/
│   ├── xinputedge/
│   │   ├── xinputedge.h    <-- 統一エントリポイント（利用側はこれだけで OK）
│   │   ├── server.h        <-- XieServer / XieState 公開 API
│   │   └── export.h        <-- XINPUTEDGE_API エクスポートマクロ
│   └── xie_server.h        <-- 後方互換ヘッダ（非推奨・将来削除予定）
├── protocol/
│   └── xie_protocol.h      <-- XIE Protocol 定数・XiePacket 構造体
├── src/                    <-- 内部実装（PRIVATE）
│   ├── server.c
│   ├── ring_buffer.c / .h
│   ├── udp.c / .h
│   ├── packet.c / .h
│   └── time_util.c / .h
├── tests/
│   ├── test_ring_buffer.c  <-- ring_buffer ユニットテスト（5 ケース）
│   └── test_packet.c       <-- packet ユニットテスト（6 ケース）
├── examples/
│   └── basic_receiver/
└── CMakeLists.txt
```

## ビルド (Linux / Raspberry Pi)

静的ライブラリ `libxinputedge.a` としてビルドされます。

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
```

### ユニットテストの実行

```bash
cd build && ctest --output-on-failure
```

### サンプルの実行

#### `basic_receiver` — 受信ログの確認

```bash
./build/examples/basic_receiver/basic_receiver
```

#### `cli_robot_sim` — CLI ロボットシミュレータ ⭐

ANSI アート製のロボットをコントローラーで動かすエンドツーエンドデモ。
送信側 (Windows) と受信側 (Linux) を接続した状態でのライブラリ動作確認に最適です。

```bash
./build/examples/cli_robot_sim/cli_robot_sim
```

```
  XInputEdge CLI Robot Simulator
  Port :5000  |  LOST:0  |  ● CONNECTED

  +--------------------------------------------------+
  |                                                  |
  |                        →                         |
  |                                                  |
  +--------------------------------------------------+

  pos: (25, 10)  yaw:  90 deg
  LX [··········|██████·····] +0.60
  LY [··········|···········] +0.00
  RX [·····█████|···········] -0.48
  LT:  0  RT:  0
```

**操作方法：**

| 入力 | 動作 |
|---|---|
| 左スティック | ロボット移動（上下左右） |
| 右スティック X | 旋回（yaw 変化） |
| A ボタン | アクション（★ フラッシュ）|
| START ボタン | 終了 |

通信が切れると自動的に **`TIMEOUT / SAFE STATE`** 表示に切り替わり、フェイルセーフが発動していることを視覚的に確認できます。

### インストール

```bash
sudo cmake --install build
# 以下に配置されます:
#   /usr/local/lib/libxinputedge.a
#   /usr/local/include/xinputedge/xinputedge.h
#   /usr/local/include/protocol/xie_protocol.h
```

## ラズパイ等への手動導入方法（git/cmake 不要）

```bash
gcc -O2 -I./include -I./protocol -I./src \
    ./src/*.c ./examples/basic_receiver/main.c \
    -o basic_receiver -pthread
```

## 使用方法 (C 言語)

### ヘッダのインクルード

```c
/* 推奨: 統一エントリポイント。これ 1 行で全 API にアクセスできます */
#include <xinputedge/xinputedge.h>

/* 後方互換（既存コードはそのまま動作します） */
/* #include "xie_server.h" */
```

### 基本的な使い方

```c
#include <xinputedge/xinputedge.h>
#include <pthread.h>

/* ネットワーク受信専用スレッド */
void *network_thread(void *arg) {
    XieServer *server = (XieServer *)arg;
    while (1) {
        xie_server_recv(server);  /* 内部でブロック/タイムアウト処理あり */
    }
    return NULL;
}

int main(void) {
    XieServer *server = xie_server_create();
    xie_server_init(server, "0.0.0.0", 5000); /* 5000 番ポートで受信待機 */

    pthread_t th;
    pthread_create(&th, NULL, network_thread, server);

    /* メインの制御ループ（例: 1000 Hz） */
    while (1) {
        if (xie_server_is_timeout(server)) {
            /* 通信断（フェイルセーフ）。モーター等を緊急停止させてください */
        } else {
            const XieState *state = xie_server_state(server);
            /* state->lx, state->ly : 左スティック (-32768 ~ 32767) */
            /* state->rx, state->ry : 右スティック (-32768 ~ 32767) */
            /* state->lt, state->rt : 左右トリガー  (0 ~ 255)        */
            /* state->buttons       : ボタン状態    (XieButtons 参照) */
        }
        xie_sleep_us(1000); /* 1 ms 待機 */
    }

    xie_server_destroy(server);
    return 0;
}
```

### 高度な API

```c
/* パケットのロス数累計を取得 */
uint32_t lost = xie_server_lost(server);

/* （デバッグ用）受信した最新の XIE Packet の生データにアクセス */
const XiePacket *pkt = (const XiePacket *)xie_server_latest_packet(server);
```

### ボタン判定の例

```c
const XieState *state = xie_server_state(server);

if (state->buttons & XIE_BTN_A) { /* A ボタンが押されている */ }
if (state->buttons & XIE_BTN_LB) { /* LB が押されている     */ }
```

## CMake プロジェクトへの組み込み

`add_subdirectory` でそのまま使えます。

```cmake
add_subdirectory(xinputedge-receiver)
target_link_libraries(your_target PRIVATE xinputedge)
# PUBLIC include は自動的に伝播します（target_include_directories 不要）
```

## API リファレンス

| 関数 | 説明 |
|---|---|
| `xie_server_create()` | サーバーハンドルを生成（`malloc`）|
| `xie_server_destroy(s)` | ハンドルを解放（close + free）|
| `xie_server_init(s, ip, port)` | UDP ソケットを初期化。`XIE_OK` / `XIE_ERROR` を返す |
| `xie_server_recv(s)` | 非同期受信（ネットワークスレッドから呼ぶ）|
| `xie_server_state(s)` | 最新の入力状態 `XieState*` を返す |
| `xie_server_lost(s)` | ロストパケット数を返す |
| `xie_server_is_timeout(s)` | 通信停止中なら `1`、通信中なら `0` |
| `xie_server_close(s)` | ソケットを閉じる |
| `xie_sleep_us(us)` | マイクロ秒スリープ（制御ループ用）|
