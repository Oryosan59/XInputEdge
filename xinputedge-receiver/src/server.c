#include "packet.h"
#include "ring_buffer.h"
#include "time_util.h"
#include "udp.h"
#include "xie_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct XieServer {
  UdpReceiver udp;
  XieRingBuffer ring_buffer;
  XieState current_state;
  XiePacket latest_packet; // 生パケットのバックアップ
  uint32_t lost_count;
  uint16_t last_sample_id;
  uint8_t last_heartbeat;
  uint32_t last_recv_us;
  int connection_lost;
};

XieServer *xie_server_create(void) {
  XieServer *s = (XieServer *)malloc(sizeof(XieServer));
  if (s) {
    memset(s, 0, sizeof(XieServer));
    s->connection_lost = 1;
  }
  return s;
}

void xie_server_destroy(XieServer *s) {
  if (s) {
    xie_server_close(s);
    free(s);
  }
}

int xie_server_init(XieServer *s, const char *bind_ip, int port) {
  if (!s)
    return XIE_ERROR;

  memset(s, 0, sizeof(XieServer));
  xie_ring_buffer_init(&s->ring_buffer);
  s->connection_lost = 1;

  // ソケットの初期化、タイムアウトは5msを指定
  int ret = xie_udp_init(&s->udp, bind_ip, port, XIE_RECV_TIMEOUT_US);
  return ret == 0 ? XIE_OK : XIE_ERROR;
}

static void apply_safe_state(XieServer *s) {
  s->current_state.lx = 0;
  s->current_state.ly = 0;
  s->current_state.rx = 0;
  s->current_state.ry = 0;
  s->current_state.lt = 0;
  s->current_state.rt = 0;
  s->current_state.buttons = 0;
}

static void resync(XieServer *s, const XiePacket *packet) {
  s->last_sample_id = packet->sample_id;
  s->last_heartbeat = XIE_GET_FLAGS(packet->typeAndFlags);
  xie_ring_buffer_init(&s->ring_buffer);
}

int xie_server_recv(XieServer *s) {
  if (!s)
    return XIE_ERROR;

  XiePacket packet;
  int ret = xie_udp_recv(&s->udp, &packet, sizeof(packet));

  if (ret > 0) {
    if (xie_packet_validate(&packet, (size_t)ret) == XIE_OK) {
      s->last_recv_us = xie_time_us();
      s->connection_lost = 0;

      uint16_t expected = s->last_sample_id + 1;
      uint16_t delta = (uint16_t)(packet.sample_id - expected);

      if (delta > 0 && delta < 1000) {
        s->lost_count += delta;
      } else if (delta >= 1000) {
        resync(s, &packet);
      }

      s->last_sample_id = packet.sample_id;
      s->last_heartbeat = XIE_GET_FLAGS(packet.typeAndFlags);

      xie_ring_buffer_write(&s->ring_buffer, &packet);
      return XIE_OK;
    }
    return XIE_DROP;
  } else if (ret == XIE_TIMEOUT) {
    // 50ms経過で通信切断（タイムアウト）と判定する
    if (!s->connection_lost && (xie_time_us() - s->last_recv_us > 50000)) {
      s->connection_lost = 1;
    }
    return XIE_TIMEOUT;
  }

  return XIE_ERROR;
}

const XieState *xie_server_state(const XieServer *s) {
  if (!s)
    return NULL;

  XieServer *mutable_s = (XieServer *)s;

  if (s->connection_lost) {
    apply_safe_state(mutable_s);
  } else {
    XiePacket pkt;
    if (xie_ring_buffer_read(&s->ring_buffer, &pkt)) {
      mutable_s->current_state.lx = pkt.lx;
      mutable_s->current_state.ly = pkt.ly;
      mutable_s->current_state.rx = pkt.rx;
      mutable_s->current_state.ry = pkt.ry;
      mutable_s->current_state.lt = pkt.lt;
      mutable_s->current_state.rt = pkt.rt;
      mutable_s->current_state.buttons = pkt.buttons;
      // 生パケット記録（デバッグ表示用）
      mutable_s->latest_packet = pkt;
    }
  }

  return &s->current_state;
}

const void *xie_server_latest_packet(const XieServer *s) {
  if (!s)
    return NULL;
  return &s->latest_packet;
}

uint32_t xie_server_lost(const XieServer *s) {
  if (!s)
    return 0;
  return s->lost_count;
}

int xie_server_is_timeout(const XieServer *s) {
  if (!s)
    return 1;

  // 直近の受信時刻からの乖離を見てタイムアウトを強制判定
  if (!s->connection_lost && (xie_time_us() - s->last_recv_us > 50000)) {
    ((XieServer *)s)->connection_lost = 1;
  }
  return s->connection_lost;
}

void xie_server_close(XieServer *s) {
  if (!s)
    return;
  xie_udp_close(&s->udp);
}
