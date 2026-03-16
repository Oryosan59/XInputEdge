/*
 * test_packet.c
 * XIE xie_packet_validate() のユニットテスト
 * 外部フレームワーク不要・stdlib のみで動作
 */

#include "../protocol/xie_protocol.h"
#include "../src/packet.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* テストユーティリティ                                                  */
/* ------------------------------------------------------------------ */
static int g_tests = 0;
static int g_failed = 0;

#define TEST(name) \
  do { \
    printf("  %-55s", name); \
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
/* ヘルパー: 正常なパケットを生成                                        */
/* ------------------------------------------------------------------ */
static XiePacket make_valid_packet(void) {
  XiePacket p;
  memset(&p, 0, sizeof(p));
  p.magic = XIE_MAGIC;
  p.version = XIE_VERSION;
  p.typeAndFlags = XIE_MAKE_TYPE_FLAGS(XIE_TYPE_GAMEPAD, 0);
  p.sample_id = 42;
  return p;
}

/* ------------------------------------------------------------------ */
/* テストケース                                                          */
/* ------------------------------------------------------------------ */

/* 正常パケットを正しいサイズで渡すと XIE_OK が返ること */
static void test_valid_packet(void) {
  TEST("validate: valid packet returns XIE_OK");
  XiePacket p = make_valid_packet();
  int ret = xie_packet_validate(&p, XIE_PACKET_SIZE);
  ASSERT(ret == XIE_OK);
}

/* サイズが合わないと XIE_DROP が返ること */
static void test_wrong_size(void) {
  TEST("validate: wrong size returns XIE_DROP");
  XiePacket p = make_valid_packet();
  int ret = xie_packet_validate(&p, XIE_PACKET_SIZE - 1);
  ASSERT(ret == XIE_DROP);
}

/* マジックナンバーが違うと XIE_DROP が返ること */
static void test_wrong_magic(void) {
  TEST("validate: wrong magic returns XIE_DROP");
  XiePacket p = make_valid_packet();
  p.magic = 0x0000;
  int ret = xie_packet_validate(&p, XIE_PACKET_SIZE);
  ASSERT(ret == XIE_DROP);
}

/* バージョンが違うと XIE_DROP が返ること */
static void test_wrong_version(void) {
  TEST("validate: wrong version returns XIE_DROP");
  XiePacket p = make_valid_packet();
  p.version = XIE_VERSION + 1;
  int ret = xie_packet_validate(&p, XIE_PACKET_SIZE);
  ASSERT(ret == XIE_DROP);
}

/* タイプが違うと XIE_DROP が返ること */
static void test_wrong_type(void) {
  TEST("validate: wrong type returns XIE_DROP");
  XiePacket p = make_valid_packet();
  p.typeAndFlags = XIE_MAKE_TYPE_FLAGS(0x0F, 0); /* 存在しないタイプ */
  int ret = xie_packet_validate(&p, XIE_PACKET_SIZE);
  ASSERT(ret == XIE_DROP);
}

/* NULL を渡すと XIE_DROP が返ること（クラッシュしないこと） */
static void test_null_packet(void) {
  TEST("validate: NULL packet returns XIE_DROP");
  int ret = xie_packet_validate(NULL, XIE_PACKET_SIZE);
  ASSERT(ret == XIE_DROP);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void) {
  printf("=== test_packet ===\n");

  test_valid_packet();
  test_wrong_size();
  test_wrong_magic();
  test_wrong_version();
  test_wrong_type();
  test_null_packet();

  printf("\n%d / %d tests passed\n", g_tests - g_failed, g_tests);
  return g_failed == 0 ? 0 : 1;
}
