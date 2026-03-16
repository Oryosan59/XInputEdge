using System;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Threading;

namespace XInputEdge
{
    // 内部実装（ユーザー非公開）
    internal static class XInputNative
    {
        [DllImport("XInput1_4.dll")]
        internal static extern uint XInputGetState(uint dwUserIndex, out XINPUT_STATE pState);

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

        private readonly int _userIndex;
        private volatile bool _running = false;
        private bool _wasConnected = false;
        private ushort _sampleId = 0;
        private byte _heartbeat = 0;
        private Socket? _socket;
        private EndPoint? _endpoint;
        private byte[] _sendBuffer;
        private IntPtr _bufferPtr;
        private Thread? _sendThread;

        private static readonly long _startTick = Stopwatch.GetTimestamp();

        public XieClient(int userIndex = 0)
        {
            _userIndex = userIndex;
            _sendBuffer = new byte[XieProtocol.XIE_PACKET_SIZE];
            _bufferPtr = Marshal.AllocHGlobal(XieProtocol.XIE_PACKET_SIZE);
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

        private static uint GetMonotonicUs()
        {
            long elapsed = Stopwatch.GetTimestamp() - _startTick;
            return (uint)(elapsed * 1_000_000 / Stopwatch.Frequency);
        }

        public void Start(string ip, int port)
        {
            if (_running) return;

            _endpoint = new IPEndPoint(IPAddress.Parse(ip), port);
            _socket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);

            _running = true;
            _sendThread = new Thread(SendLoop)
            {
                IsBackground = true,
                Priority = ThreadPriority.Highest,
                Name = "XieSendLoop"
            };
            _sendThread.Start();
        }

        public void Stop()
        {
            _running = false;
            _sendThread?.Join(100);

            _socket?.Close();
            _socket = null;
        }

        private void SendLoop()
        {
            var sw = Stopwatch.StartNew();
            long next = 0;
            long interval = Stopwatch.Frequency / 1000; // 1ms

            while (_running)
            {
                while (sw.ElapsedTicks < next)
                    Thread.SpinWait(10);
                next += interval;

                var result = XInputNative.XInputGetState((uint)_userIndex, out var state);

                if (result == XInputNative.ERROR_DEVICE_NOT_CONNECTED)
                {
                    if (_wasConnected) { _wasConnected = false; OnDisconnected?.Invoke(); }
                    continue;
                }

                if (!_wasConnected) { _wasConnected = true; OnConnected?.Invoke(); }

                SendPacket(ref state.Gamepad);
            }
        }

        private void SendPacket(ref XInputNative.XINPUT_GAMEPAD gamepad)
        {
            _heartbeat ^= XieProtocol.XIE_FLAG_HEARTBEAT;

            var packet = new XiePacket
            {
                magic = XieProtocol.XIE_MAGIC,
                version = XieProtocol.XIE_VERSION,
                typeAndFlags = XieProtocol.MakeTypeFlags(XieProtocol.XIE_TYPE_GAMEPAD, _heartbeat),
                sampleId = _sampleId++,
                timestampUs = GetMonotonicUs(),
                lx = gamepad.sThumbLX,
                ly = gamepad.sThumbLY,
                rx = gamepad.sThumbRX,
                ry = gamepad.sThumbRY,
                lt = gamepad.bLeftTrigger,
                rt = gamepad.bRightTrigger,
                buttons = gamepad.wButtons
            };

            Marshal.StructureToPtr(packet, _bufferPtr, false);
            Marshal.Copy(_bufferPtr, _sendBuffer, 0, XieProtocol.XIE_PACKET_SIZE);

            try
            {
                if (_running && _socket != null && _endpoint != null)
                {
                    _socket.SendTo(_sendBuffer, XieProtocol.XIE_PACKET_SIZE, SocketFlags.None, _endpoint);

                    if ((_sampleId % 100) == 0)
                    {
                        Console.WriteLine($"[XIE] Sent {_sampleId} packets to {_endpoint}");
                    }
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
            if (_bufferPtr != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(_bufferPtr);
                _bufferPtr = IntPtr.Zero;
            }
        }
    }
}
