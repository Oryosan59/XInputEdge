[![CI Pipeline](https://github.com/Oryosan59/XInputEdge/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Oryosan59/XInputEdge/actions/workflows/ci.yml)
# XInputEdge

読み: **クスィー**

Windows PC に接続された XInput 対応ゲームパッド（Xbox コントローラー等）の入力を、ネットワーク (UDP) 経由で Linux / Raspberry Pi / MCU などへ超低遅延（1000 Hz）でストリーミングするための通信ライブラリです。

「PC 上の豊かで強力な C# 環境でデバイス入力を受け取り、軽量な C コンパイラしか存在しないエッジデバイス（ラズパイやロボットなど）へネットワーク越しに直接制御命令として流し込む」という用途に特化して設計されています。

## アーキテクチャ

本プロジェクトは、役割の異なる 2 つの独立したコードベースで構成されています。

1. **[送信側 (xinputedge-sender)](./xinputedge-sender/README.md)**
   - **環境:** C# / .NET 8+ (Windows 専用)
   - **役割:** XInput API を叩いてゲームパッドの入力を取得し、XIE Packet にシリアライズして UDP で送信します。
   - **特徴:** `Stopwatch` を利用した高精度タイマースレッドにより 1 ms 周期 (1000 Hz) での送信ループを実現し、GC によるスパイクを防ぐ完全なゼロアロケーション設計です。

2. **[受信側 (xinputedge-receiver)](./xinputedge-receiver/README.md)**
   - **環境:** C99 / CMake (Linux / Raspberry Pi 等を想定)
   - **役割:** ネットワークスレッド経由で XIE Packet を受信・検証し、ユーザー（メインスレッド等）に最新の入力状態を提供します。
   - **特徴:** 通信ジッターを吸収するリングバッファ（De-jitter バッファ）を搭載しており、ネットワークの揺らぎに関わらず滑らかな制御ループ（例: PWM モータ制御ループなど）を維持できます。動的メモリ確保（malloc 等）にも依存しません。

## 通信仕様 (XIE Protocol)

UDP ベースの独自設計による 22 バイト固定長の軽量 XIE Packet を使用しています。

- **マジックナンバー:** `0x5849`（"XI"）
- **転送レート:** 最大 1000 Hz
- **エンディアン:** リトルエンディアン固定
- **異常検知・フェイルセーフ:**
  - サンプルの連番 (Sample ID) によるパケットロス検知
  - C# 側の送信ストップウォッチのタイムスタンプ同期
  - 無通信時（50 ms 以上）の自動フェイルセーフ機能（アナログスティックや出力をニュートラルへリセット）

詳細なバイナリフォーマットについては [xie_protocol.h](./xinputedge-receiver/protocol/xie_protocol.h) を参照してください。

## CI / 品質管理

全ての Push / Pull Request に対して以下が自動実行されます。

| ジョブ | 内容 |
|---|---|
| **format** | `clang-format`（C）+ `dotnet format`（C#）|
| **static-analysis** | `cppcheck` 静的解析 |
| **build-release** | Release ビルド + `ctest` ユニットテスト |
| **build-debug** | Debug ビルド + `ctest` ユニットテスト |

## 使い方（Quick Start）

すぐに試してみるための手順です。より詳細な使い方は各ディレクトリの README を参照してください。

### 1. 受信側の準備 (Raspberry Pi / Linux)

```bash
cd xinputedge-receiver
mkdir build && cd build
cmake ..
cmake --build .

# サンプルアプリケーションの起動
./examples/basic_receiver/basic_receiver
```
> サンプルアプリケーションが起動し、`192.168.4.100:5000` で UDP の受信待機を始めます（IP はコード内で変更可能）。

### 2. 送信側の準備 (Windows PC)

コントローラーを接続した Windows PC で、`xinputedge-sender/examples/BasicSender` を実行します。
> 実行前に `Program.cs` 内の送信先 IP アドレス（`client.Start("192.168.4.100", 5000);`）を、受信側の IP アドレスに合わせて変更してください。

```powershell
cd xinputedge-sender/examples/BasicSender
dotnet run
```

### 3. 通信の確認（実証データ）

正常に動作すると、**1000 回/秒（1 ms 周期）** で XIE Packet が送出されます。受信側のターミナルでは以下のようなリアルタイムなストリーミングデータが観測できます。

```text
[XIE] M:5849 V:1 TF:01 ID:42145 TS:631968402 | LX:   128 LY:   128 RX:   128 RY: -1671 LT:  0 RT:  0 BTN:0000 LOST:0
[XIE] M:5849 V:1 TF:01 ID:42149 TS:631972399 | LX:   128 LY:   128 RX:   128 RY: -1671 LT:  0 RT:  0 BTN:0000 LOST:0
[XIE] M:5849 V:1 TF:41 ID:42154 TS:631977399 | LX:   128 LY:   128 RX:   128 RY: -1671 LT:  0 RT:  0 BTN:0000 LOST:0
```
*(※ラズパイ上で毎秒 250 回の頻度でサンプリング出力した実際の検証ログです。)*

**【データの見方】**
- **メタデータ:** `M` (マジックナンバー `5849`="XI"), `V` (バージョン), `TF` (タイプ＆フラグ), `ID` (パケットの連番), `TS` (C# 側マイクロ秒タイムスタンプ)
- **スティック (`LX`, `LY`, `RX`, `RY`):** -32768 ～ 32767 の高精度なアナログ値
- **トリガー (`LT`, `RT`):** 0 ～ 255 のアナログ値
- **ボタン (`BTN`):** 押されているボタンを示す 16bit の 16 進数フラグ
- **パケットロス (`LOST`):** ローカルネットワーク上では `0` を安定して維持します。

## 通信の安定性と検証データ

### 処理レイテンシと通信ジッター
| 項目 | 計測・理論値 | 備考 |
|------|-----------|------|
| 送信スレッド周期 | ~1.0 ms | `Stopwatch` + `SpinWait` のスピンクロックによる高精度タイマー |
| ペイロードサイズ | 22 Bytes | MTU（通常 1500）に完全に収まるためフラグメンテーションが発生しない |
| UDP 受信周期 | ~1.0 ms ～ 1.5 ms | OS のネットワークスタックによる揺らぎ (ジッター) を含む |
| De-jitter 吸収遅延 | 5 サンプル (5 ms) | `XIE_DEJITTER_DELAY=5` にて ±2 ms のネットワークブレを平滑化 |

### 障害回復テスト（フェイルセーフ動作）
1. **パケットロスと検知**: `sample_id` (0～65535 のローテーション) の連番チェックにより、パケットロス発生時に `xie_server_lost()` のカウンターが正確に増加します。安定したローカルネットワークでは**ロス率 0%** を維持します。
2. **通信タイムアウト時の挙動**: 無通信状態が **50 ms** 経過すると即座に `connection_lost` ステートに移行し、入力が全て `0` の安全状態 (Safe State) へ移行。制御対象（ロボット等）の暴走を防止します。
3. **ゼロアロケーション**: ネットワークスレッドと制御ループ (1 kHz) を完全に分離。動的メモリ確保を行わないためメモリリークや GC スパイクを排除しています。

## 具体的な組み込み手順

### 【送信側】C# (.NET) プロジェクトへの組み込み

1. **ファイルのコピー**
   `xinputedge-sender` フォルダから以下の 2 ファイルをあなたの C# プロジェクトにコピーします。
   - `protocol/XieProtocol.cs`（プロトコル定義）
   - `XieClient.cs`（クライアント本体）

2. **初期化と送信の開始**

   ```csharp
   using XInputEdge;

   int playerIndex = XieClient.FindFirstConnected();
   if (playerIndex >= 0) {
       using var client = new XieClient(playerIndex);
       client.Start("192.168.4.100", 5000);
       // client.Start() はブロックしません。非同期でパケットが送信され続けます。
   }
   ```

3. **終了処理（重要）**
   アプリ終了時に必ず `client.Stop();` または `Dispose();` を呼んでください。

---

### 【受信側】C 言語プロジェクトへの組み込み

1. **ファイルのコピー**
   `xinputedge-receiver` フォルダから以下をあなたの C プロジェクトにコピーします。
   - `include/xinputedge/`（統一ヘッダ群）
   - `protocol/xie_protocol.h`（プロトコル定義）
   - `src/*.c` および `src/*.h`（実装ファイル一式）

2. **受信スレッドの作成と制御ループの実装**

   ```c
   /* 統一エントリポイント — これ 1 行で全 API にアクセスできます */
   #include <xinputedge/xinputedge.h>
   #include <pthread.h>

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

       while (1) {
           if (xie_server_is_timeout(server)) {
               /* 【ここでモーターを緊急停止させる処理を書く】 */
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

> CMake の `add_subdirectory` で組み込む場合、`target_link_libraries(your_target PRIVATE xinputedge)` だけで include パスは自動伝播します。

## ライセンス
MIT License
