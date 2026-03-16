#include "ring_buffer.h"
#include <string.h>

void xie_ring_buffer_init(XieRingBuffer *rb) {
  if (!rb)
    return;
  memset(rb->buffer, 0, sizeof(rb->buffer));
  rb->newest_sample_id = 0;
  rb->has_data = 0;
}

void xie_ring_buffer_write(XieRingBuffer *rb, const XiePacket *packet) {
  if (!rb || !packet)
    return;

  int index = packet->sample_id % XIE_RING_BUFFER_SIZE;
  rb->buffer[index] = *packet;
  rb->newest_sample_id = packet->sample_id;
  rb->has_data = 1;
}

int xie_ring_buffer_read(const XieRingBuffer *rb, XiePacket *out_packet) {
  if (!rb || !out_packet || !rb->has_data)
    return 0;

  // de-jitter遅延を適用したサンプルIDを計算
  uint16_t target = rb->newest_sample_id - XIE_DEJITTER_DELAY;
  int index = target % XIE_RING_BUFFER_SIZE;

  *out_packet = rb->buffer[index];

  return 1;
}
