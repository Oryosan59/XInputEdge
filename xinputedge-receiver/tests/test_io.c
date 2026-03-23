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
#include "../include/xie_server.h"

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
/* TestFixture Setup & Teardown                                         */
/* ------------------------------------------------------------------ */
typedef struct {
  XieServer *srv;
  int port;
} TestFixture;

static TestFixture setup_server(void) {
  TestFixture f = {0};
  f.srv = xie_server_create();
  if (f.srv) {
    if (xie_server_init(f.srv, "127.0.0.1", 0) == XIE_OK) {
      f.port = xie_server_get_port(f.srv);
    } else {
      xie_server_destroy(f.srv);
      f.srv = NULL;
    }
  }
  return f;
}

static void teardown_server(TestFixture *f) {
  if (f && f->srv) {
    xie_server_destroy(f->srv);
    f->srv = NULL;
    f->port = 0;
  }
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

  TestFixture f = setup_server();
  if (!f.srv || f.port < 0) {
    printf("SKIP (server setup failed)\n");
    return;
  }

  /* sample_id = 1 のパケットを送信 */
  XiePacket pkt = make_valid_packet(1);
  send_mock_packet(f.port, &pkt);

  int recv_ret = XIE_TIMEOUT;
  /* 初回のパケット到達を待つ */
  for (int i = 0; i < 10; i++) {
    recv_ret = xie_server_recv(f.srv);
    if (recv_ret == XIE_OK || recv_ret == XIE_DROP) {
      break; 
    }
  }

  /* de-jitter buffer を考慮し、sample_id=1 のパケットが読み取れるように
     追加でパケットを送信する（XIE_DEJITTER_DELAY 相当分） */
  if (recv_ret == XIE_OK) {
    // ring_buffer.h で XIE_DEJITTER_DELAY は 5 と定義されているため
    // sample_id = 1 を読むには最新が 6 以上（ここでは余裕を見て 2〜6 を送信）
    for (uint16_t id = 2; id <= 6; id++) {
      XiePacket p = make_valid_packet(id);
      send_mock_packet(f.port, &p);
      for (int i = 0; i < 10; i++) {
        int r = xie_server_recv(f.srv);
        if (r == XIE_OK || r == XIE_DROP) break;
      }
    }
  }

  const XieState *st = xie_server_state(f.srv);

  /* 検証とデバッグ出力 */
  int ok = 1;
  if (recv_ret != XIE_OK) {
    printf("\n    -> recv_ret failed. Expected XIE_OK(%d), got %d\n", XIE_OK, recv_ret);
    ok = 0;
  }
  if (!st) {
    printf("\n    -> state is NULL\n");
    ok = 0;
  } else if (recv_ret == XIE_OK) {
    if (st->lx != pkt.lx) { printf("\n    -> lx mismatch: exp %d, got %d\n", pkt.lx, st->lx); ok = 0; }
    if (st->ly != pkt.ly) { printf("\n    -> ly mismatch: exp %d, got %d\n", pkt.ly, st->ly); ok = 0; }
    if (st->lt != pkt.lt) { printf("\n    -> lt mismatch: exp %d, got %d\n", pkt.lt, st->lt); ok = 0; }
    if (st->rt != pkt.rt) { printf("\n    -> rt mismatch: exp %d, got %d\n", pkt.rt, st->rt); ok = 0; }
    if (st->buttons != pkt.buttons) { printf("\n    -> buttons mismatch: exp %d, got %d\n", pkt.buttons, st->buttons); ok = 0; }
  }

  ASSERT(ok);
  teardown_server(&f);
}

/*
 * タイムアウトテスト
 *   何も送らないと xie_server_recv() == XIE_TIMEOUT が返ること、
 *   および connection_lost フラグが立つこと（is_timeout == 1）
 */
static void test_timeout(void) {
  TEST("io: 無送信時 → XIE_TIMEOUT & is_timeout == 1");

  TestFixture f = setup_server();
  if (!f.srv) {
    printf("SKIP (server setup failed)\n");
    return;
  }

  /* recv_timeout は 5ms → タイムアウトが返るはず */
  int recv_ret  = xie_server_recv(f.srv);
  /* 接続済み状態を経ていないので最初から connection_lost=1 のはず */
  int is_timed  = xie_server_is_timeout(f.srv);

  ASSERT(recv_ret == XIE_TIMEOUT && is_timed == 1);
  teardown_server(&f);
}

/*
 * 不正パケット破棄テスト
 *   magic が不正なパケットを送ると xie_server_recv() == XIE_DROP が返ること
 */
static void test_invalid_packet_dropped(void) {
  TEST("io: 不正パケット → XIE_DROP");

  TestFixture f = setup_server();
  if (!f.srv) {
    printf("SKIP (server setup failed)\n");
    return;
  }

  /* 意図的に壊れた magic を持つパケット */
  XiePacket bad = make_valid_packet(1);
  bad.magic     = 0xDEAD;
  send_mock_packet(f.port, &bad);

  int recv_ret = XIE_TIMEOUT;
  for (int i = 0; i < 10; i++) {
    recv_ret = xie_server_recv(f.srv);
    if (recv_ret != XIE_TIMEOUT) break;
  }
  
  ASSERT(recv_ret == XIE_DROP);
  teardown_server(&f);
}

/*
 * パケットロストカウントテスト
 *   sample_id に大きなギャップがあると lost_count が增加すること
 */
static void test_lost_count(void) {
  TEST("io: sample_id ギャップ → lost_count 増加");

  TestFixture f = setup_server();
  if (!f.srv) {
    printf("SKIP (server setup failed)\n");
    return;
  }

  /* 1 枚目: sample_id = 1 */
  XiePacket first = make_valid_packet(1);
  send_mock_packet(f.port, &first);
  for (int i = 0; i < 10; i++) {
    if (xie_server_recv(f.srv) == XIE_OK) break;
  }

  /* 2 枚目: sample_id = 11 → lost = 9 */
  XiePacket jumped = make_valid_packet(11);
  send_mock_packet(f.port, &jumped);
  for (int i = 0; i < 10; i++) {
    if (xie_server_recv(f.srv) == XIE_OK) break;
  }

  uint32_t lost = xie_server_lost(f.srv);
  ASSERT(lost == 9);
  teardown_server(&f);
}

/*
 * apply_safe_state テスト
 *   接続失効時に apply_safe_state() が呼ばれ、状態が安全(0)に保たれること
 */
static void test_apply_safe_state(void) {
  TEST("io: connection_lost時にapply_safe_stateが状態をクリアすること");

  TestFixture f = setup_server();
  if (!f.srv) {
    printf("SKIP (server setup failed)\n");
    return;
  }

  // 初期状態は connection_lost = 1
  const XieState *st = xie_server_state(f.srv);
  int ok = 1;
  if (!st) {
    printf("\n    -> state is NULL\n");
    ok = 0;
  } else {
    if (st->lx != 0 || st->ly != 0 || st->rx != 0 || st->ry != 0 ||
        st->lt != 0 || st->rt != 0 || st->buttons != 0) {
      printf("\n    -> safe state not applied correctly\n");
      ok = 0;
    }
  }

  ASSERT(ok);
  teardown_server(&f);
}

/*
 * resync テスト
 *   sample_idギャップが1000以上のときにresync処理が走ること
 */
static void test_resync(void) {
  TEST("io: 大きなsample_idギャップ時にresyncが働き受信が継続すること");

  TestFixture f = setup_server();
  if (!f.srv) {
    printf("SKIP (server setup failed)\n");
    return;
  }

  // 1. 最初は sample_id = 1 を送る
  XiePacket p1 = make_valid_packet(1);
  send_mock_packet(f.port, &p1);
  for (int i = 0; i < 10; i++) {
    if (xie_server_recv(f.srv) == XIE_OK) break;
  }

  // 2. sample_id が 1002 のパケットを送る
  // delta = 1002 - (1 + 1) = 1000 で resync(s, packet) が呼ばれる
  XiePacket p2 = make_valid_packet(1002);
  send_mock_packet(f.port, &p2);
  
  int ok = 0;
  for (int i = 0; i < 10; i++) {
    if (xie_server_recv(f.srv) == XIE_OK) {
      ok = 1;
      break;
    }
  }

  ASSERT(ok);
  teardown_server(&f);
}

/*
 * xie_server_latest_packet テスト
 *   記録された直近の生パケットを取得できること
 */
static void test_latest_packet(void) {
  TEST("io: xie_server_latest_packet で直近のパケットが取得できること");

  TestFixture f = setup_server();
  if (!f.srv) {
    printf("SKIP (server setup failed)\n");
    return;
  }

  // 特徴的な値を設定して送信
  XiePacket pkt = make_valid_packet(1);
  pkt.lx = 1234;
  send_mock_packet(f.port, &pkt);

  int recv_ret = XIE_TIMEOUT;
  for (int i = 0; i < 10; i++) {
    recv_ret = xie_server_recv(f.srv);
    if (recv_ret == XIE_OK || recv_ret == XIE_DROP) {
      break; 
    }
  }

  // DEJITTER分 送る
  if (recv_ret == XIE_OK) {
    for (uint16_t id = 2; id <= 6; id++) {
      XiePacket p = make_valid_packet(id);
      send_mock_packet(f.port, &p);
      for (int i = 0; i < 10; i++) {
        int r = xie_server_recv(f.srv);
        if (r == XIE_OK || r == XIE_DROP) break;
      }
    }
  }

  // 内部状態へ反映させるため xie_server_state を呼ぶ
  xie_server_state(f.srv);

  // xie_server_latest_packet の戻り値を確認
  const XiePacket *latest = (const XiePacket *)xie_server_latest_packet(f.srv);

  int ok = 1;
  if (!latest) {
    printf("\n    -> latest_packet is NULL\n");
    ok = 0;
  } else if (latest->lx != 1234) {
    printf("\n    -> latest_packet mismatch: exp %d, got %d\n", 1234, latest->lx);
    ok = 0;
  }

  ASSERT(ok);
  teardown_server(&f);
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
  test_apply_safe_state();
  test_resync();
  test_latest_packet();

  printf("\n%d / %d tests passed\n", g_tests - g_failed, g_tests);
  return g_failed == 0 ? 0 : 1;
}
