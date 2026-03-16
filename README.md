[![CI Pipeline](https://github.com/Oryosan59/XInputEdge/actions/workflows/ci.yml/badge.svg)](https://github.com/Oryosan59/XInputEdge/actions/workflows/ci.yml)
![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![Platform](https://img.shields.io/badge/Sender-Windows%20%7C%20C%23%20%2F%20.NET%208-informational)
![Platform](https://img.shields.io/badge/Receiver-Linux%20%7C%20C99%20%2F%20CMake-success)
![Protocol](https://img.shields.io/badge/Protocol-UDP%20%40%201000%20Hz-orange)

<br>

<div align="center">

```
██╗  ██╗██╗███╗   ██╗██████╗ ██╗   ██╗████████╗███████╗██████╗  ██████╗ ███████╗
╚██╗██╔╝██║████╗  ██║██╔══██╗██║   ██║╚══██╔══╝██╔════╝██╔══██╗██╔════╝ ██╔════╝
 ╚███╔╝ ██║██╔██╗ ██║██████╔╝██║   ██║   ██║   █████╗  ██║  ██║██║  ███╗█████╗  
 ██╔██╗ ██║██║╚██╗██║██╔═══╝ ██║   ██║   ██║   ██╔══╝  ██║  ██║██║   ██║██╔══╝  
██╔╝ ██╗██║██║ ╚████║██║     ╚██████╔╝   ██║   ███████╗██████╔╝╚██████╔╝███████╗
╚═╝  ╚═╝╚═╝╚═╝  ╚═══╝╚═╝      ╚═════╝    ╚═╝   ╚══════╝╚═════╝  ╚═════╝ ╚══════╝
```

**XIE** ─ 読み: **クスィー**

*XInput → UDP → Edge.  1 ms. Zero allocation. No compromise.*

</div>

---

## 概要

Windows PC に接続された XInput 対応ゲームパッドの入力を、**UDP 経由で 1000 Hz** にてエッジデバイスへストリーミングする通信ライブラリ。

> **設計思想:** PC 上の豊かな C# 環境でデバイス入力を受け取り、C コンパイラしかないエッジデバイス（Raspberry Pi・ロボット等）へ、ネットワーク越しに直接・超低遅延で制御命令として流し込む。

```
┌─────────────────────────────────┐         ┌──────────────────────────────────┐
│         Windows PC              │  UDP    │     Raspberry Pi / Linux MCU     │
│  ┌───────────────────────────┐  │ ──────► │  ┌────────────────────────────┐  │
│  │  XInput API               │  │ 22 B   │  │  xie_server_recv()         │  │
│  │    │                      │  │ 1000Hz │  │    │                        │  │
│  │    ▼                      │  │        │  │    ▼                        │  │
│  │  XieClient (C#)           │  │        │  │  De-jitter Ring Buffer      │  │
│  │  Stopwatch + SpinWait     │  │        │  │    │                        │  │
│  │  Zero-alloc send loop     │  │        │  │    ▼                        │  │
│  └───────────────────────────┘  │        │  │  xie_server_state()         │  │
│   1 ms tick / GC-spike free     │        │  │  → Motor / PWM control      │  │
└─────────────────────────────────┘        └──────────────────────────────────┘
```

---

## アーキテクチャ

本プロジェクトは **2 つの独立したコードベース**で構成されています。

### `xinputedge-sender` — C# / .NET 8+ (Windows)

| 特徴 | 詳細 |
|---|---|
| **入力取得** | XInput API によるゲームパッド読み取り |
| **送信周期** | `Stopwatch` + `SpinWait` スピンクロック → **1 ms (1000 Hz)** |
| **メモリ** | 完全ゼロアロケーション設計。GC スパイク排除 |
| **出力** | 22 バイト固定長 XIE Packet を UDP で送出 |

→ 詳細: [xinputedge-sender/README.md](./xinputedge-sender/README.md)

### `xinputedge-receiver` — C99 / CMake (Linux / Raspberry Pi)

| 特徴 | 詳細 |
|---|---|
| **受信** | 専用ネットワークスレッドで XIE Packet を受信・検証 |
| **ジッター吸収** | De-jitter リングバッファ (±2 ms のネットワーク揺らぎを平滑化) |
| **メモリ** | `malloc` 非使用。静的確保のみ |
| **フェイルセーフ** | 50 ms 無通信で Safe State へ自動移行 |

→ 詳細: [xinputedge-receiver/README.md](./xinputedge-receiver/README.md)

---

## XIE Protocol

UDP ベースの独自設計による **22 バイト固定長**パケット。

```
Offset  Size  Field          Description
──────  ────  ─────────────  ────────────────────────────────────────────
  0      2    magic          0x5849 ("XI") — フレーム同期用マジックナンバー
  2      1    version        プロトコルバージョン (現在: 1)
  3      1    type_flags     パケット種別 & フラグ
  4      2    sample_id      連番 (0–65535 ローテーション) — ロス検知用
  6      4    timestamp_us   C# 側マイクロ秒タイムスタンプ
 10      2    lx             左スティック X  (-32768 ~ 32767)
 12      2    ly             左スティック Y  (-32768 ~ 32767)
 14      2    rx             右スティック X  (-32768 ~ 32767)
 16      2    ry             右スティック Y  (-32768 ~ 32767)
 18      1    lt             左トリガー      (0 ~ 255)
 19      1    rt             右トリガー      (0 ~ 255)
 20      2    buttons        ボタン状態      (16-bit フラグ)
──────  ────  ─────────────  ────────────────────────────────────────────
 22           Total
```

- **エンディアン:** リトルエンディアン固定
- **MTU:** 22 B < 1500 B → フラグメンテーション発生なし
- **フェイルセーフ:** 50 ms 無通信で `connection_lost` ステートへ自動移行

詳細フォーマット → [`xie_protocol.h`](./xinputedge-receiver/protocol/xie_protocol.h)

---

## パフォーマンス

| 計測項目 | 値 | 備考 |
|---|---|---|
| 送信スレッド周期 | **~1.0 ms** | SpinWait スピンクロック |
| ペイロードサイズ | **22 Bytes** | MTU 内に完全収容 |
| UDP 受信周期 | **~1.0 – 1.5 ms** | OS ネットワークスタックによる揺らぎ込み |
| De-jitter 吸収遅延 | **5 ms** | `XIE_DEJITTER_DELAY=5` で ±2 ms を平滑化 |
| ローカルネットワーク パケットロス率 | **0%** | 安定した LAN 環境での実測値 |

---

## 実証ログ

ラズパイ上で 250 Hz サンプリングした実際の受信ログ:

```
[XIE] M:5849 V:1 TF:01 ID:42145 TS:631968402 | LX:   128 LY:   128 RX:   128 RY: -1671 LT:  0 RT:  0 BTN:0000 LOST:0
[XIE] M:5849 V:1 TF:01 ID:42149 TS:631972399 | LX:   128 LY:   128 RX:   128 RY: -1671 LT:  0 RT:  0 BTN:0000 LOST:0
[XIE] M:5849 V:1 TF:41 ID:42154 TS:631977399 | LX:   128 LY:   128 RX:   128 RY: -1671 LT:  0 RT:  0 BTN:0000 LOST:0
```

| フィールド | 説明 |
|---|---|
| `M` | マジックナンバー (`5849` = "XI") |
| `V` | プロトコルバージョン |
| `TF` | タイプ & フラグ |
| `ID` | パケット連番 |
| `TS` | C# 側マイクロ秒タイムスタンプ |
| `LX/LY/RX/RY` | アナログスティック値 (-32768 ~ 32767) |
| `LT/RT` | トリガー値 (0 ~ 255) |
| `BTN` | ボタン状態 (16-bit hex) |
| `LOST` | パケットロスカウンタ |

---

## Quick Start

### Step 1 — 受信側 (Raspberry Pi / Linux)

```bash
git clone https://github.com/Oryosan59/XInputEdge.git
cd XInputEdge/xinputedge-receiver

mkdir build && cd build
cmake ..
cmake --build .

# サンプル起動 → 192.168.4.100:5000 で UDP 受信待機
./examples/basic_receiver/basic_receiver
```

### Step 2 — 送信側 (Windows PC)

`Program.cs` 内の送信先 IP を受信側に合わせて変更してから実行:

```powershell
cd xinputedge-sender/examples/BasicSender
dotnet run
```

---

## 組み込み手順

### 【送信側】C# プロジェクト

**1. ファイルをコピー**

```
your-project/
├── XieClient.cs            ← xinputedge-sender/XieClient.cs
└── protocol/
    └── XieProtocol.cs      ← xinputedge-sender/protocol/XieProtocol.cs
```

**2. 実装**

```csharp
using XInputEdge;

// 接続中の最初のコントローラーを自動検出
int playerIndex = XieClient.FindFirstConnected();

if (playerIndex >= 0)
{
    using var client = new XieClient(playerIndex);

    // 非ブロッキング。呼び出し後すぐに 1000 Hz 送信が始まる
    client.Start("192.168.4.100", 5000);

    Console.ReadLine(); // アプリ稼働中はここで待機

    // using により自動的に Stop() & Dispose() が呼ばれる
}
```

> ⚠️ アプリ終了時に必ず `client.Stop()` または `Dispose()` を呼ぶこと。

---

### 【受信側】C プロジェクト

**1. ファイルをコピー**

```
your-project/
├── include/xinputedge/     ← xinputedge-receiver/include/xinputedge/
├── protocol/
│   └── xie_protocol.h      ← xinputedge-receiver/protocol/xie_protocol.h
└── src/                    ← xinputedge-receiver/src/*.c, *.h
```

**2. 実装**

```c
#include <xinputedge/xinputedge.h>  /* 統一エントリポイント — これ 1 行で全 API */
#include <pthread.h>

/* ネットワークスレッド — 制御ループと完全分離 */
void *network_thread(void *arg) {
    XieServer *server = (XieServer *)arg;
    while (1) { xie_server_recv(server); }
    return NULL;
}

int main(void) {
    XieServer *server = xie_server_create();
    xie_server_init(server, "0.0.0.0", 5000);

    pthread_t th;
    pthread_create(&th, NULL, network_thread, server);

    /* 1 kHz 制御ループ */
    while (1) {
        if (xie_server_is_timeout(server)) {
            /* ── フェイルセーフ: モーター緊急停止 ── */
        } else {
            const XieState *s = xie_server_state(server);
            /* s->lx, s->ly  : 左スティック  (-32768 ~ 32767) */
            /* s->rx, s->ry  : 右スティック  (-32768 ~ 32767) */
            /* s->lt, s->rt  : トリガー      (0 ~ 255)        */
            /* s->buttons    : ボタン状態    (XieButtons 参照) */
        }
        xie_sleep_us(1000); /* 1 ms */
    }

    xie_server_destroy(server);
    return 0;
}
```

**3. CMake**

```cmake
add_subdirectory(xinputedge-receiver)
target_link_libraries(your_target PRIVATE xinputedge)
# include パスは自動伝播されます
```

---

## CI / 品質管理

すべての Push / Pull Request に対して自動実行:

| ジョブ | 内容 |
|---|---|
| `format` | `clang-format` (C) + `dotnet format` (C#) |
| `static-analysis` | `cppcheck` による静的解析 |
| `build-release` | Release ビルド + `ctest` ユニットテスト |
| `build-debug` | Debug ビルド + `ctest` ユニットテスト |

---

## フェイルセーフ動作

| 条件 | 動作 |
|---|---|
| パケットロス検出 | `sample_id` 連番ギャップを検知 → `xie_server_lost()` カウンタ増加 |
| **50 ms 無通信** | 即座に `connection_lost` ステートへ移行。全入力を `0`（Safe State）にリセット |
| GC スパイク | ゼロアロケーション設計により発生しない |

---

## ライセンス

MIT License — © Oryosan59