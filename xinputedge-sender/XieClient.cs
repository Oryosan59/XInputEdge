using System;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Threading;
using System.Security;

namespace XInputEdge
{
    // 内部実装（ユーザー非公開）
    [SuppressUnmanagedCodeSecurity]
    internal static class XInputNative
    {
        [DllImport("XInput1_4.dll", ExactSpelling = true)]
        internal static extern uint XInputGetState(uint dwUserIndex, out XINPUT_STATE pState);

        [DllImport("kernel32.dll")]
        internal static extern IntPtr GetCurrentThread();

        [DllImport("kernel32.dll")]
        internal static extern IntPtr SetThreadAffinityMask(IntPtr hThread, IntPtr dwThreadAffinityMask);

        [StructLayout(LayoutKind.Sequential)]
        internal struct XINPUT_GAMEPAD
        {
            public ushort wButtons;
            public byte bLeftTrigger;
            public byte bRightTrigger;
            public short sThumbLX;
            public short sThumbLY;
            public short sThumbRX;
            public short sThumbRY;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct XINPUT_STATE
        {
            public uint dwPacketNumber;
            public XINPUT_GAMEPAD Gamepad;
        }

        internal const uint ERROR_SUCCESS = 0;
        internal const uint ERROR_DEVICE_NOT_CONNECTED = 1167;
    }

    public class XieClient : IDisposable
    {
        public event Action? OnConnected;
        public event Action? OnDisconnected;

        public IntPtr ThreadAffinityMask { get; set; } = IntPtr.Zero;

        // 非アトミック更新の排除（外部から安全にポーリング可能）
        private long _droppedPackets = 0;
        public long DroppedPackets => Interlocked.Read(ref _droppedPackets);

        private readonly int _userIndex;
        private readonly int _targetHz;
        private readonly long _intervalTicks;

        private volatile bool _running = false;
        private bool _wasConnected = false;
        private ushort _sampleId = 0;
        private byte _heartbeat = 0;

        private Socket? _socket;
        private byte[] _sendBuffer;
        private Thread? _sendThread;

        private static readonly long _startTick = Stopwatch.GetTimestamp();

        // ターゲット周波数（Hz）を外部指定可能に
        public XieClient(int userIndex = 0, int targetHz = 1000)
        {
            _userIndex = userIndex;
            _targetHz = targetHz;
            _intervalTicks = Stopwatch.Frequency / _targetHz;
            _sendBuffer = new byte[XieProtocol.XIE_PACKET_SIZE];
        }

        public static int FindFirstConnected()
        {
            for (int i = 0; i < 4; i++)
            {
                var result = XInputNative.XInputGetState((uint)i, out _);
                if (result == XInputNative.ERROR_SUCCESS) return i;
            }
            return -1;
        }

        // uint に戻す（71分でのラップアラウンドは仕様とする）
        private static uint GetMonotonicUs()
        {
            long elapsedTicks = Stopwatch.GetTimestamp() - _startTick;
            return (uint)((elapsedTicks * 1_000_000) / Stopwatch.Frequency);
        }

        public void Start(string ip, int port)
        {
            if (_running) return;

            var endpoint = new IPEndPoint(IPAddress.Parse(ip), port);
            _socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
            _socket.Connect(endpoint);
            _socket.Blocking = false;

            _running = true;
            _sendThread = new Thread(SendLoop)
            {
                IsBackground = true,
                Priority = ThreadPriority.Highest,
                Name = $"XieStreamLoop_{_userIndex}"
            };
            _sendThread.Start();
        }

        public void Stop()
        {
            _running = false;

            // タイムアウトなしで安全に終了を担保する
            _sendThread?.Join();

            _socket?.Close();
            _socket = null;
        }

        private void SendLoop()
        {
            if (ThreadAffinityMask != IntPtr.Zero)
            {
                XInputNative.SetThreadAffinityMask(XInputNative.GetCurrentThread(), ThreadAffinityMask);
            }

            var sw = Stopwatch.StartNew();
            long baseTick = 0;
            long loopCount = 0;

            // Sleep(0)を許容する閾値（例: インターバルの半分）
            // 1000Hzなら0.5ms。この時間より余裕があればCPUを手放す
            long yieldThreshold = _intervalTicks / 2;

            while (_running)
            {
                if (!_wasConnected)
                {
                    // 未接続ステート: 緩やかなポーリングでCPU負荷を下げる
                    var result = XInputNative.XInputGetState((uint)_userIndex, out var state);

                    if (result != XInputNative.ERROR_SUCCESS)
                    {
                        Thread.Sleep(500);
                        continue;
                    }

                    // 接続を検知 -> ウェイトモードへ移行
                    _wasConnected = true;
                    OnConnected?.Invoke();

                    baseTick = sw.ElapsedTicks;
                    loopCount = 0;
                    _sampleId = 0; // 再接続時にリセットするかは仕様次第（今回はリセットする）

                    SendPacket(ref state.Gamepad);
                    loopCount++;
                }
                else
                {
                    long target = baseTick + loopCount * _intervalTicks;

                    // ハイブリッド・ウェイト（CPU負荷軽減と精度の両立）
                    while (true)
                    {
                        long now = sw.ElapsedTicks;
                        if (now >= target) break;

                        if (target - now > yieldThreshold)
                        {
                            // 余裕がある時はOSに処理を譲る（CPU使用率が激減する）
                            Thread.Sleep(0);
                        }
                        else
                        {
                            // 終端の微調整のみマイルドなスピンで行う
                            Thread.SpinWait(10);
                        }
                    }

                    var result = XInputNative.XInputGetState((uint)_userIndex, out var state);

                    if (result != XInputNative.ERROR_SUCCESS)
                    {
                        // 切断を検知 -> 未接続ステートへ
                        _wasConnected = false;
                        OnDisconnected?.Invoke();
                        continue;
                    }

                    SendPacket(ref state.Gamepad);
                    loopCount++;
                }
            }
        }

        private unsafe void SendPacket(ref XInputNative.XINPUT_GAMEPAD gamepad)
        {
            // Stop直後のRace Condition対策。ローカルに退避してnullチェック
            var sock = _socket;
            if (sock == null) return;

            _heartbeat ^= XieProtocol.XIE_FLAG_HEARTBEAT;

            fixed (byte* ptr = _sendBuffer)
            {
                XiePacket* p = (XiePacket*)ptr;
                p->magic = XieProtocol.XIE_MAGIC;
                p->version = XieProtocol.XIE_VERSION;
                p->typeAndFlags = XieProtocol.MakeTypeFlags(XieProtocol.XIE_TYPE_GAMEPAD, _heartbeat);
                p->sampleId = _sampleId++;
                p->timestampUs = GetMonotonicUs(); // 元のuintキャストでOK
                p->lx = gamepad.sThumbLX;
                p->ly = gamepad.sThumbLY;
                p->rx = gamepad.sThumbRX;
                p->ry = gamepad.sThumbRY;
                p->lt = gamepad.bLeftTrigger;
                p->rt = gamepad.bRightTrigger;
                p->buttons = gamepad.wButtons;
            }

            try
            {
                sock.Send(_sendBuffer, 0, XieProtocol.XIE_PACKET_SIZE, SocketFlags.None);
            }
            catch (SocketException ex)
            {
                if (ex.SocketErrorCode == SocketError.WouldBlock)
                {
                    // Interlockedによるアトミック加算
                    Interlocked.Increment(ref _droppedPackets);
                }
                else
                {
                    Console.WriteLine($"[XIE ERROR] {ex.GetType().Name} - {ex.Message}");
                    Thread.Sleep(100);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[XIE ERROR] {ex.GetType().Name} - {ex.Message}");
                Thread.Sleep(100);
            }
        }

        public void Dispose()
        {
            Stop();
        }
    }
}
