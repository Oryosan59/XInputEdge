using System.Runtime.InteropServices;

namespace XInputEdge
{
    public static class XieProtocol
    {
        public const ushort XIE_MAGIC = 0x5849; // "XI"
        public const byte XIE_VERSION = 1;
        public const byte XIE_TYPE_GAMEPAD = 1;
        public const int XIE_PACKET_SIZE = 22;

        // flags
        public const byte XIE_FLAG_INPUT_DROP = 0x1;
        public const byte XIE_FLAG_OVERFLOW = 0x2;
        public const byte XIE_FLAG_HEARTBEAT = 0x4;
        public const byte XIE_FLAG_RESERVED = 0x8;

        public static byte MakeTypeFlags(byte type, byte flags)
        {
            return (byte)((flags << 4) | (type & 0x0F));
        }
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct XiePacket
    {
        public ushort magic;
        public byte version;
        public byte typeAndFlags; // type(4bit) | flags(4bit)
        public ushort sampleId;
        public uint timestampUs;
        public short lx, ly, rx, ry;
        public byte lt, rt;
        public ushort buttons;
    }
}
