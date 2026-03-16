#include "packet.h"
#include <stdio.h>

int xie_packet_validate(const XiePacket *packet, size_t received_size) {
  if (!packet)
    return XIE_DROP;

  // 1. サイズ検証
  if (received_size != XIE_PACKET_SIZE) {
    static int e1 = 0;
    if (e1++ % 100 == 0)
      printf("[DEBUG] DROP: size %zu != %d\n", received_size, XIE_PACKET_SIZE);
    return XIE_DROP;
  }

  // 2. マジックナンバー検証
  if (packet->magic != XIE_MAGIC) {
    static int e2 = 0;
    if (e2++ % 100 == 0)
      printf("[DEBUG] DROP: magic 0x%04X != 0x%04X\n", packet->magic, XIE_MAGIC);
    return XIE_DROP;
  }

  // 3. バージョン検証
  if (packet->version != XIE_VERSION) {
    static int e3 = 0;
    if (e3++ % 100 == 0)
      printf("[DEBUG] DROP: version %d != %d\n", packet->version, XIE_VERSION);
    return XIE_DROP;
  }

  // 4. タイプ検証
  uint8_t type = XIE_GET_TYPE(packet->typeAndFlags);
  if (type != XIE_TYPE_GAMEPAD) {
    static int e4 = 0;
    if (e4++ % 100 == 0)
      printf("[DEBUG] DROP: type %d != %d\n", type, XIE_TYPE_GAMEPAD);
    return XIE_DROP;
  }

  return XIE_OK;
}
