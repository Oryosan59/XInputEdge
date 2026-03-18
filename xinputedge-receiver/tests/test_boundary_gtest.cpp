#include <gtest/gtest.h>

#include "../src/ring_buffer.h"
#include "../src/packet.h"

// ------------------------------------------------------------------
// Tests for Ring Buffer
// ------------------------------------------------------------------

class RingBufferTest : public ::testing::Test {
protected:
    XieRingBuffer rb;

    void SetUp() override {
        xie_ring_buffer_init(&rb);
    }
    
    // Helper to make dummy packets quickly
    XiePacket makePacket(uint16_t sample_id) {
        XiePacket p = {};
        p.magic = XIE_MAGIC;
        p.version = XIE_VERSION;
        p.typeAndFlags = XIE_MAKE_TYPE_FLAGS(XIE_TYPE_GAMEPAD, 0);
        p.sample_id = sample_id;
        p.lx = (int16_t)(sample_id * 10);
        return p;
    }
};

TEST_F(RingBufferTest, InitEmpty) {
    EXPECT_EQ((int)rb.has_data, 0);
    XiePacket out;
    EXPECT_EQ(xie_ring_buffer_read(&rb, &out), 0);
}

// Wrap-around 境界のテスト (最重要)
TEST_F(RingBufferTest, WrapAroundBoundary) {
    const int num_packets = XIE_RING_BUFFER_SIZE * 3 + 10; // Wrap around 3 times securely
    
    for (int i = 0; i < num_packets; ++i) {
        XiePacket p = makePacket(i);
        xie_ring_buffer_write(&rb, &p);
    }
    
    EXPECT_NE((int)rb.has_data, 0);
    
    XiePacket out;
    int read_success = xie_ring_buffer_read(&rb, &out);
    EXPECT_EQ(read_success, 1);
    
    // 最新のパケットは num_packets - 1 のIDを持っている
    // De-jitter delay により、得られる値は (num_packets - 1) - XIE_DEJITTER_DELAY
    uint16_t expected_id = (num_packets - 1) - XIE_DEJITTER_DELAY;
    EXPECT_EQ(out.sample_id, expected_id);
    EXPECT_EQ(out.lx, expected_id * 10);
}

// 状態不変性検証のテスト: NULL 呼び出し等でバッファが破壊されないか
TEST_F(RingBufferTest, StateInvariance) {
    // Write 1 packet
    XiePacket p = makePacket(42);
    xie_ring_buffer_write(&rb, &p);
    EXPECT_NE((int)rb.has_data, 0);

    // Call with NULL, state shouldn't break
    xie_ring_buffer_write(&rb, nullptr);
    xie_ring_buffer_write(nullptr, &p);
    
    XiePacket out;
    // Should still be able to read normally (though dejitter might need more packets to match ID 42 exactly, 
    // actually it will return whatever is at newest - delay index, which is index - 5, so it will be 0-initialized slots)
    // Wait, the test here is just ensuring it doesn't crash or lose the 'has_data' state.
    EXPECT_NE((int)rb.has_data, 0);
    
    // Read with NULL
    int read_ret = xie_ring_buffer_read(nullptr, &out);
    EXPECT_EQ(read_ret, 0);
}


// ------------------------------------------------------------------
// Tests for Packet Validation
// ------------------------------------------------------------------

class PacketValidationTest : public ::testing::Test {
protected:
    XiePacket valid_packet;

    void SetUp() override {
        // Initialize a standard valid packet
        valid_packet = {};
        valid_packet.magic = XIE_MAGIC;
        valid_packet.version = XIE_VERSION;
        valid_packet.typeAndFlags = XIE_MAKE_TYPE_FLAGS(XIE_TYPE_GAMEPAD, 0);
    }
};

// 正常系の検証
TEST_F(PacketValidationTest, ValidPacket) {
    EXPECT_EQ(XIE_OK, xie_packet_validate(&valid_packet, XIE_PACKET_SIZE));
}

// Length破壊、境界サイズ超過、部分書き込み、ゼロ長
TEST_F(PacketValidationTest, BoundarySize) {
    // Exact size
    EXPECT_EQ(XIE_OK, xie_packet_validate(&valid_packet, XIE_PACKET_SIZE));
    
    // 1 byte short (部分書き込み / partial write)
    EXPECT_EQ(XIE_DROP, xie_packet_validate(&valid_packet, XIE_PACKET_SIZE - 1));
    
    // 1 byte over (複数連結や予期しない余剰データ / back-to-back parsing issues prep)
    EXPECT_EQ(XIE_DROP, xie_packet_validate(&valid_packet, XIE_PACKET_SIZE + 1));
    
    // Zero length
    EXPECT_EQ(XIE_DROP, xie_packet_validate(&valid_packet, 0));
}

// ヘッダ異常 (CRCの代わりとなる基本的なシグネチャ・プロトコル不備チェック)
TEST_F(PacketValidationTest, InvalidHeader) {
    // Magic corruption
    XiePacket bad_magic = valid_packet;
    bad_magic.magic = 0x0000;
    EXPECT_EQ(XIE_DROP, xie_packet_validate(&bad_magic, XIE_PACKET_SIZE));
    
    // Version corruption
    XiePacket bad_version = valid_packet;
    bad_version.version = 99;
    EXPECT_EQ(XIE_DROP, xie_packet_validate(&bad_version, XIE_PACKET_SIZE));
    
    // Type corruption
    XiePacket bad_type = valid_packet;
    bad_type.typeAndFlags = XIE_MAKE_TYPE_FLAGS(99, 0);
    EXPECT_EQ(XIE_DROP, xie_packet_validate(&bad_type, XIE_PACKET_SIZE));
}

// NULL Pointer Input
TEST_F(PacketValidationTest, NullPointerSafety) {
    EXPECT_EQ(XIE_DROP, xie_packet_validate(nullptr, XIE_PACKET_SIZE));
}
