#ifndef UDP_H
#define UDP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// OSのソケットハンドルを隠蔽
typedef struct {
  int socket_fd;
} UdpReceiver;

// 初期化（IP・ポート番号を指定）。bind_ipが NULL の場合は INADDR_ANY(0.0.0.0)。失敗時 -1 を返す。
int xie_udp_init(UdpReceiver *udp, const char *bind_ip, int port, int recv_timeout_us);

// 非同期で受信を試みる
// 戻り値: 受信バイト数（>0）、タイムアウト（XIE_TIMEOUT）、エラー（XIE_ERROR）
int xie_udp_recv(UdpReceiver *udp, void *buffer, size_t buffer_size);

// クローズ
void xie_udp_close(UdpReceiver *udp);

#ifdef __cplusplus
}
#endif

#endif // UDP_H
