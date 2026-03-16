#ifndef TIME_UTIL_H
#define TIME_UTIL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// モノトニック時刻 (マイクロ秒) を取得する
uint32_t xie_time_us(void);

// 指定したマイクロ秒だけスリープする (制御ループ用)
void xie_sleep_us(uint32_t microseconds);

#ifdef __cplusplus
}
#endif

#endif // TIME_UTIL_H
