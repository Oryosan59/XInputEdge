#include "../../protocol/xie_protocol.h"
#include "xie_server.h"
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
void *network_thread(void *arg) {
  XieServer *server = (XieServer *)arg;
  while (1) {
    xie_server_recv(server);
  }
  return NULL;
}

int main() {
  XieServer *server = xie_server_create();
  if (xie_server_init(server, "192.168.4.100", 5000) != 0) {
    printf("XIE Server の初期化に失敗しました (192.168.4.100:5000)\n");
    return -1;
  }

  printf("XIE Server 起動 (192.168.4.100:5000) ...\n");

  pthread_t th;
  pthread_create(&th, NULL, network_thread, server);

  // 制御ループ (1000Hz)
  while (1) {
    if (xie_server_is_timeout(server)) {
      static int to_cnt = 0;
      if (to_cnt++ % 1000 == 0) {
        printf("通信タイムアウト (受信なし)\n");
      }
    } else {
      const XieState *s = xie_server_state(server);
      const XiePacket *pkt =
          (const XiePacket *)xie_server_latest_packet(server);
      if (s && pkt) {
        // パケットの全データを4ms間隔（250Hz）で出力する
        static int display_cnt = 0;
        if (display_cnt++ % 4 == 0) {
          printf("[XIE] M:%04X V:%d TF:%02X ID:%5u TS:%8u | "
                 "LX:%6d LY:%6d RX:%6d RY:%6d LT:%3d RT:%3d BTN:%04X LOST:%d\n",
                 pkt->magic, pkt->version, pkt->typeAndFlags, pkt->sample_id,
                 pkt->timestamp_us, s->lx, s->ly, s->rx, s->ry, s->lt, s->rt,
                 s->buttons, xie_server_lost(server));
        }
      }
    }
    xie_sleep_us(1000);
  }

  xie_server_destroy(server);
  return 0;
}
