/*
 * test_io.c
 * I/O レイヤー統合テスト — UDP ローカルループバック (127.0.0.1)
 *
 * テスト戦略:
 *   - XieServer を 127.0.0.1 の空きポートで初期化する
 *   - 別の UDP ソケット（送信側）でモックパケットを送り込む
 *   - xie_server_recv() / xie_server_state() の戻り値を検証する
 *
 * 外部ライブラリ不要・stdlib + POSIX のみで動作
 */

#include "../protocol/xie_protocol.h"
#include "../src/packet.h"
#include "../src/udp.h"
#include "../../xinputedge-receiver/include/xie_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* テストユーティリティ                                                  */
/* ------------------------------------------------------------------ */
static int g_tests  = 0;
static int g_failed = 0;

#define TEST(name) \
  do { \
    printf("  %-60s", name); \
    g_tests++; \
  } while (0)

#define ASSERT(cond) \
  do { \
    if (!(cond)) { \
      printf("FAIL  (%s:%d)\n", __FILE__, __LINE__); \
      g_failed++; \
    } else { \
      printf("OK\n"); \
    } \
  } while (0)

/* ------------------------------------------------------------------ */
/* ヘルパー: 正常な XiePacket を生成する                                 */
/* ------------------------------------------------------------------ */
static XiePacket make_valid_packet(uint16_t sample_id) {
  XiePacket p;
  memset(&p, 0, sizeof(p));
  p.magic        = XIE_MAGIC;
  p.version      = XIE_VERSION;
  p.typeAndFlags = XIE_MAKE_TYPE_FLAGS(XIE_TYPE_GAMEPAD, 0);
  p.sample_id    = sample_id;
  p.lx           = 1000;
  p.ly           = -2000;
  p.rx           = 500;
  p.ry           = -500;
  p.lt           = 128;
  p.rt           = 64;
  p.buttons      = XIE_BTN_A;
  return p;
}

/* ------------------------------------------------------------------ */
/* ヘルパー: UDP ソケットを作り、dest_port 宛にパケットを送信する        */
/*   戻り値: 送信バイト数（失敗時 -1）                                   */
/* ------------------------------------------------------------------ */
static int send_mock_packet(int dest_port, const XiePacket *pkt) {
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0)
    return -1;

  struct sockaddr_in dest;
  memset(&dest, 0, sizeof(dest));
  dest.sin_family      = AF_INET;
  dest.sin_port        = htons((uint16_t)dest_port);
  dest.sin_addr.s_addr = inet_addr("127.0.0.1");

  ssize_t ret = sendto(sock, pkt, sizeof(*pkt), 0,
                       (struct sockaddr *)&dest, sizeof(dest));
  close(sock);
  return (int)ret;
}

/* ------------------------------------------------------------------ */
/* ヘルパー: OS に空きポートを割り当てさせ、番号を返す                   */
/* ------------------------------------------------------------------ */
static int allocate_free_port(void) {
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0)
    return -1;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_port        = 0; /* 0 = OS が選択 */
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return -1;
  }

  socklen_t len = sizeof(addr);
  if (getsockname(sock, (struct sockaddr *)&addr, &len) < 0) {
    close(sock);
    return -1;
  }

  int port = ntohs(addr.sin_port);
  close(sock); /* XieServer 側が同ポートを bind するために解放 */
  return port;
}

/* ================================================================== */
/* テストケース                                                         */
/* ================================================================== */

/*
 * 正常受信テスト
 *   有効なパケットを送ると xie_server_recv() == XIE_OK となり、
 *   xie_server_state() の値がパケット内容と一致すること
 */
static void test_normal_receive(void) {
  TEST("io: 正常パケット受信 → XIE_OK & state 一致");

  int port = allocate_free_port();
  if (port < 0) {
    printf("SKIP (port allocation failed)\n");
    return;
  }

  XieServer *srv = xie_server_create();
  if (!srv) {
    printf("SKIP (server create failed)\n");
    return;
  }

  int init = xie_server_init(srv, "127.0.0.1", port);
  if (init != XIE_OK) {
    printf("SKIP (server init failed)\n");
    xie_server_destroy(srv);
    return;
  }

  /* sample_id = 1 のパケットを送信 */
  XiePacket pkt = make_valid_packet(1);
  int sent = send_mock_packet(port, &pkt);

  int recv_ret = (sent > 0) ? xie_server_recv(srv) : XIE_ERROR;

  /* 2 回目を送って state を確定させる（ring_buffer が 1 周するタイミングに依存しない） */
  if (recv_ret == XIE_OK) {
    XiePacket pkt2 = make_valid_packet(2);
    send_mock_packet(port, &pkt2);
    xie_server_recv(srv);
  }

  const XieState *st = xie_server_state(srv);

  /* 検証 */
  int ok = (recv_ret == XIE_OK) && (st != NULL) &&
           (st->lx == pkt.lx) && (st->ly == pkt.ly) &&
           (st->lt == pkt.lt) && (st->rt == pkt.rt) &&
           (st->buttons == pkt.buttons);

  ASSERT(ok);

  xie_server_destroy(srv);
}

/*
 * タイムアウトテスト
 *   何も送らないと xie_server_recv() == XIE_TIMEOUT が返ること、
 *   および connection_lost フラグが立つこと（is_timeout == 1）
 */
static void test_timeout(void) {
  TEST("io: 無送信時 → XIE_TIMEOUT & is_timeout == 1");

  int port = allocate_free_port();
  if (port < 0) {
    printf("SKIP (port allocation failed)\n");
    return;
  }

  XieServer *srv = xie_server_create();
  if (!srv) {
    printf("SKIP (server create failed)\n");
    return;
  }

  int init = xie_server_init(srv, "127.0.0.1", port);
  if (init != XIE_OK) {
    printf("SKIP (server init failed)\n");
    xie_server_destroy(srv);
    return;
  }

  /* recv_timeout は 5ms → タイムアウトが返るはず */
  int recv_ret  = xie_server_recv(srv);
  /* 接続済み状態を経ていないので最初から connection_lost=1 のはず */
  int is_timed  = xie_server_is_timeout(srv);

  ASSERT(recv_ret == XIE_TIMEOUT && is_timed == 1);

  xie_server_destroy(srv);
}

/*
 * 不正パケット破棄テスト
 *   magic が不正なパケットを送ると xie_server_recv() == XIE_DROP が返ること
 */
static void test_invalid_packet_dropped(void) {
  TEST("io: 不正パケット → XIE_DROP");

  int port = allocate_free_port();
  if (port < 0) {
    printf("SKIP (port allocation failed)\n");
    return;
  }

  XieServer *srv = xie_server_create();
  if (!srv) {
    printf("SKIP (server create failed)\n");
    return;
  }

  int init = xie_server_init(srv, "127.0.0.1", port);
  if (init != XIE_OK) {
    printf("SKIP (server init failed)\n");
    xie_server_destroy(srv);
    return;
  }

  /* 意図的に壊れた magic を持つパケット */
  XiePacket bad = make_valid_packet(1);
  bad.magic     = 0xDEAD;
  send_mock_packet(port, &bad);

  int recv_ret = xie_server_recv(srv);
  ASSERT(recv_ret == XIE_DROP);

  xie_server_destroy(srv);
}

/*
 * パケットロストカウントテスト
 *   sample_id に大きなギャップがあると lost_count が增加すること
 */
static void test_lost_count(void) {
  TEST("io: sample_id ギャップ → lost_count 増加");

  int port = allocate_free_port();
  if (port < 0) {
    printf("SKIP (port allocation failed)\n");
    return;
  }

  XieServer *srv = xie_server_create();
  if (!srv) {
    printf("SKIP (server create failed)\n");
    return;
  }

  int init = xie_server_init(srv, "127.0.0.1", port);
  if (init != XIE_OK) {
    printf("SKIP (server init failed)\n");
    xie_server_destroy(srv);
    return;
  }

  /* 1 枚目: sample_id = 1 */
  XiePacket first = make_valid_packet(1);
  send_mock_packet(port, &first);
  xie_server_recv(srv);

  /* 2 枚目: sample_id = 11 → lost = 9 */
  XiePacket jumped = make_valid_packet(11);
  send_mock_packet(port, &jumped);
  xie_server_recv(srv);

  uint32_t lost = xie_server_lost(srv);
  ASSERT(lost == 9);

  xie_server_destroy(srv);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void) {
  printf("=== test_io (UDP loopback) ===\n");

  test_normal_receive();
  test_timeout();
  test_invalid_packet_dropped();
  test_lost_count();

  printf("\n%d / %d tests passed\n", g_tests - g_failed, g_tests);
  return g_failed == 0 ? 0 : 1;
}
