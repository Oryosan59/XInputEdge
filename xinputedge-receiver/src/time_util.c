#include "time_util.h"

#include <time.h>
#include <unistd.h>

uint32_t xie_time_us(void) {
  struct timespec ts;
#ifdef CLOCK_MONOTONIC
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  clock_gettime(CLOCK_REALTIME, &ts); // フォールバック
#endif
  return (uint32_t)((ts.tv_sec * 1000000ULL) + (ts.tv_nsec / 1000ULL));
}

void xie_sleep_us(uint32_t microseconds) {
  usleep(microseconds);
}
