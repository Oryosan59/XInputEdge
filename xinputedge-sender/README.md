# xinputedge-sender (C# / .NET)

Windows PC 上で、XInput 対応コントローラー（Xbox コントローラー等）の入力を非同期かつ高速にリモートサーバー（xinputedge-receiver 等）へ UDP 転送するための C# ライブラリです。

## 特徴

- **外部依存ゼロ**: `XInput1_4.dll` のネイティブ P/Invoke 呼び出しのみで動作し、追加の NuGet パッケージは不要です。
- **1000 Hz 送信ループ**: `Stopwatch` による高精度スレッド（`ThreadPriority.Highest`）で 1 ms 周期を実現。接続中はハイブリッドウェイト（`Sleep(0)` + `SpinWait`）で CPU 負荷を軽減し、切断時は自動エコモード（`Sleep(500)`）へ移行してリソース消費を最小化します。
- **ゼロアロケーション**: バッファを完全使い回しする実装により、長時間稼働でも GC スパイクが発生しません。
- **コントローラー監視**: 接続・切断を自動検出してイベントで通知します。

## ファイル構成

```
xinputedge-sender/
├── XieClient.cs            <-- 送信スレッドをカプセル化した XIE Client 本体
├── protocol/
│   └── XieProtocol.cs      <-- XIE Protocol 定数・XIE Packet 構造体
└── examples/
    └── BasicSender/        <-- 動作確認用サンプルアプリ
```

自身のプロジェクト（コンソールアプリ・WPF・Unity 等）には上記 2 ファイルをコピーするだけで利用できます。

## 実行要件

- Windows OS（XInput 利用のため必須）
- .NET 8.0 以降

## 使用方法 (C#)

### 基本的な使い方

```csharp
using XInputEdge;

class Program
{
    static void Main()
    {
        // 接続されているコントローラーを検索（XInput は最大 4 台）
        int index = XieClient.FindFirstConnected();
        if (index < 0) {
            Console.WriteLine("コントローラーが見つかりません。");
            return;
        }

        using var client = new XieClient(index);

        // 接続・切断イベントのサブスクライブ（任意）
        client.OnConnected    += () => Console.WriteLine("コントローラーが接続されました");
        client.OnDisconnected += () => Console.WriteLine("コントローラーが切断されました");

        // 送信先 IP・ポートを指定して 1000 Hz 送信ループを開始
        // この呼び出しはブロックしません。バックグラウンドスレッドで送信が始まります。
        client.Start("192.168.4.100", 5000);

        Console.WriteLine("Enter を押すと終了します...");
        Console.ReadLine();

        // 終了時にスレッドとソケットを解放（using による自動解放でも OK）
        client.Stop();
    }
}
```

### サンプルの実行

```powershell
cd examples/BasicSender
# Program.cs の送信先 IP を受信側に合わせてから実行
dotnet run
```

## API リファレンス

### `XieClient`

| メンバー | 説明 |
|---|---|
| `XieClient(int userIndex)` | コントローラーのインデックス（0〜3）を指定して初期化 |
| `FindFirstConnected()` | 接続されている最初のコントローラーのインデックスを返す（なければ `-1`）|
| `Start(string ip, int port)` | UDP 送信ループを開始（ノンブロッキング）|
| `Stop()` | 送信ループを停止してリソースを解放 |
| `OnConnected` | コントローラーが接続されたときに発火するイベント |
| `OnDisconnected` | コントローラーが切断されたときに発火するイベント |
| `Dispose()` | `Stop()` と同等（`using` 構文での利用を推奨）|

### `XieProtocol` 定数

| 定数 | 値 | 説明 |
|---|---|---|
| `XIE_MAGIC` | `0x5849` ("XI") | XIE Packet 識別子 |
| `XIE_VERSION` | `1` | プロトコルバージョン |
| `XIE_PACKET_SIZE` | `22` | パケットサイズ（バイト） |
| `XIE_FLAG_INPUT_DROP` | `0x1` | 入力取得失敗フラグ |
| `XIE_FLAG_OVERFLOW` | `0x2` | キュー溢れフラグ |
| `XIE_FLAG_HEARTBEAT` | `0x4` | 通信死活検出用トグルフラグ |

### `XiePacket` 構造体（内部用）

22 バイト固定長・リトルエンディアン。`StructLayout(Pack = 1)` により C 側の `XiePacket` と完全にバイナリ互換です。

| フィールド | 型 | 説明 |
|---|---|---|
| `magic` | `ushort` | `0x5849` |
| `version` | `byte` | `1` |
| `typeAndFlags` | `byte` | 上位 4bit = flags, 下位 4bit = type |
| `sampleId` | `ushort` | パケット連番（0→65535→0）|
| `timestampUs` | `uint` | 単調増加タイムスタンプ（μs）|
| `lx` / `ly` | `short` | 左スティック（-32768〜32767）|
| `rx` / `ry` | `short` | 右スティック（-32768〜32767）|
| `lt` / `rt` | `byte` | 左右トリガー（0〜255）|
| `buttons` | `ushort` | ボタン状態ビットフラグ |
