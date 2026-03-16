using System;
using System.Threading;
using XInputEdge;

namespace BasicSender
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("[XIE] BasicSender 起動");

            int index = XieClient.FindFirstConnected();
            if (index < 0)
            {
                Console.WriteLine("XInput コントローラーが見つかりません。");
                Console.WriteLine("終了するには Enter キーを押してください。");
                Console.ReadLine();
                return;
            }

            Console.WriteLine($"コントローラー検出 (Index: {index})");

            using var client = new XieClient(index);

            client.OnConnected    += () => Console.WriteLine("[Event] コントローラー接続");
            client.OnDisconnected += () => Console.WriteLine("[Event] コントローラー切断");

            string ip   = "192.168.4.100";
            int    port = 5000;
            Console.WriteLine($"{ip}:{port} への送信を開始します");

            client.Start(ip, port);

            Console.WriteLine("送信中... 終了するには Ctrl+C を押してください。");

            while (true)
            {
                Thread.Sleep(1000);
            }
        }
    }
}
