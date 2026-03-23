/*
 * test_time_util.c
 * time_util のテスト
 */

#include "../src/time_util.h"
#include <stdio.h>

static int g_tests = 0;
static int g_failed = 0;

#define TEST(name)                                                             \
  do {                                                                         \
    printf("  %-60s", name);                                                   \
    g_tests++;                                                                 \
  } while (0)

#define ASSERT(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL  (%s:%d)\n", __FILE__, __LINE__);                           \
      g_failed++;                                                              \
    } else {                                                                   \
      printf("OK\n");                                                          \
    }                                                                          \
  } while (0)

static void test_xie_sleep_us(void) {
  TEST("time_util: xie_sleep_us で指定時間待機できること");

  uint32_t start = xie_time_us();
  xie_sleep_us(10000); // 10ms
  uint32_t end = xie_time_us();

  uint32_t diff = end - start;
  // 厳密なタイマーではないので、OSのスケジューリングのブレを考慮し、
  // 10000us以上を期待するが余裕を持たせ 8000us 以上とする
  ASSERT(diff >= 8000);
}

int main(void) {
  printf("=== test_time_util ===\n");

  test_xie_sleep_us();

  printf("\n%d / %d tests passed\n", g_tests - g_failed, g_tests);
  return g_failed == 0 ? 0 : 1;
}
