using System;
using System.Threading;
using GamepadNet;

namespace BasicSender
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("gamepad-net 基本送信テスト");

            int index = GamepadClient.FindFirstConnected();
            if (index < 0)
            {
                Console.WriteLine("XInputコントローラーが見つかりません。");
                Console.WriteLine("終了するにはEnterキーを押してください。");
                Console.ReadLine();
                return;
            }

            Console.WriteLine($"コントローラー検出 (Index: {index})");

            using var client = new GamepadClient(index);

            client.OnConnected += () => Console.WriteLine("[Event] コントローラー接続");
            client.OnDisconnected += () => Console.WriteLine("[Event] コントローラー切断");

            string ip = "192.168.4.100";
            int port = 5000;
            Console.WriteLine($"{ip}:{port} への送信を開始します");

            client.Start(ip, port);

            Console.WriteLine("送信中... 終了するには Ctrl+C を押してください。");

            // dotnet run などの環境により ReadLine() が即時リターンして終了してしまうのを防ぐため、無限ループで待機
            while (true)
            {
                Thread.Sleep(1000);
            }

            // 到達しないが記述しておく
            // Console.WriteLine("終了中...");
            // client.Stop();
        }
    }
}
