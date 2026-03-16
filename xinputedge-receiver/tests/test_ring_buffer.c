/*
 * test_ring_buffer.c
 * XIE RingBuffer のユニットテスト
 * 外部フレームワーク不要・stdlib のみで動作
 */

#include "../protocol/xie_protocol.h"
#include "../src/ring_buffer.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* テストユーティリティ                                                  */
/* ------------------------------------------------------------------ */
static int g_tests = 0;
static int g_failed = 0;

#define TEST(name) \
  do { \
    printf("  %-50s", name); \
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
/* ヘルパー: テスト用パケットを生成                                      */
/* ------------------------------------------------------------------ */
static XiePacket make_packet(uint16_t sample_id, int16_t lx_val) {
  XiePacket p;
  memset(&p, 0, sizeof(p));
  p.magic = XIE_MAGIC;
  p.version = XIE_VERSION;
  p.typeAndFlags = XIE_MAKE_TYPE_FLAGS(XIE_TYPE_GAMEPAD, 0);
  p.sample_id = sample_id;
  p.lx = lx_val;
  return p;
}

/* ------------------------------------------------------------------ */
/* テストケース                                                          */
/* ------------------------------------------------------------------ */

/* init() 後は has_data が 0 であること */
static void test_init_empty(void) {
  TEST("init: has_data is 0 after init");
  XieRingBuffer rb;
  xie_ring_buffer_init(&rb);
  ASSERT(rb.has_data == 0);
}

/* write/read の基本動作 */
static void test_write_read_returns_data(void) {
  TEST("write+read: returns 1 (has data)");
  XieRingBuffer rb;
  xie_ring_buffer_init(&rb);

  /* XIE_DEJITTER_DELAY より多くパケットを書いてから読む */
  for (uint16_t i = 0; i <= XIE_DEJITTER_DELAY; i++) {
    XiePacket p = make_packet(i, (int16_t)(i * 100));
    xie_ring_buffer_write(&rb, &p);
  }

  XiePacket out;
  memset(&out, 0, sizeof(out));
  int ret = xie_ring_buffer_read(&rb, &out);
  ASSERT(ret == 1);
}

/* de-jitter: 読み出しは newest - DELAY 番目のパケットであること */
static void test_dejitter_delay(void) {
  TEST("read: returns sample delayed by XIE_DEJITTER_DELAY");
  XieRingBuffer rb;
  xie_ring_buffer_init(&rb);

  const uint16_t count = XIE_DEJITTER_DELAY + 5;
  for (uint16_t i = 0; i < count; i++) {
    XiePacket p = make_packet(i, (int16_t)(i * 10));
    xie_ring_buffer_write(&rb, &p);
  }

  XiePacket out;
  xie_ring_buffer_read(&rb, &out);

  /* newest は count-1, 期待する target = newest - DEJITTER_DELAY */
  uint16_t expected_id = (uint16_t)(count - 1 - XIE_DEJITTER_DELAY);
  ASSERT(out.sample_id == expected_id);
}

/* データなしの場合 read() は 0 を返すこと */
static void test_read_empty_returns_0(void) {
  TEST("read: returns 0 when buffer is empty");
  XieRingBuffer rb;
  xie_ring_buffer_init(&rb);
  XiePacket out;
  int ret = xie_ring_buffer_read(&rb, &out);
  ASSERT(ret == 0);
}

/* NULL 安全: write/read に NULL を渡してもクラッシュしないこと */
static void test_null_safety(void) {
  TEST("null-safety: write/read with NULL does not crash");
  XieRingBuffer rb;
  xie_ring_buffer_init(&rb);
  xie_ring_buffer_write(&rb, NULL); /* no crash */
  xie_ring_buffer_write(NULL, NULL); /* no crash */
  XiePacket out;
  int ret = xie_ring_buffer_read(NULL, &out);
  ASSERT(ret == 0);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void) {
  printf("=== test_ring_buffer ===\n");

  test_init_empty();
  test_write_read_returns_data();
  test_dejitter_delay();
  test_read_empty_returns_0();
  test_null_safety();

  printf("\n%d / %d tests passed\n", g_tests - g_failed, g_tests);
  return g_failed == 0 ? 0 : 1;
}
