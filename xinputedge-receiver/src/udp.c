#include "udp.h"
#include "../protocol/xie_protocol.h"
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int xie_udp_init(UdpReceiver *udp, const char *bind_ip, int port, int recv_timeout_us) {
  if (!udp)
    return -1;

  udp->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (udp->socket_fd < 0) {
    perror("socket init failed");
    return -1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr =
      (bind_ip != NULL && bind_ip[0] != '\0') ? inet_addr(bind_ip) : htonl(INADDR_ANY);

  if (bind(udp->socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind failed");
    xie_udp_close(udp);
    return -1;
  }

  // SO_RCVTIMEO ポリシー追加
  if (recv_timeout_us > 0) {
    struct timeval tv;
    tv.tv_sec = recv_timeout_us / 1000000;
    tv.tv_usec = recv_timeout_us % 1000000;
    setsockopt(udp->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  }

  return 0;
}

int xie_udp_recv(UdpReceiver *udp, void *buffer, size_t buffer_size) {
  if (!udp || udp->socket_fd < 0 || !buffer)
    return XIE_ERROR;

  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);

  ssize_t received = recvfrom(udp->socket_fd, buffer, buffer_size, 0,
                              (struct sockaddr *)&client_addr, &client_len);
  if (received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return XIE_TIMEOUT;
    }
    return XIE_ERROR;
  }

  return (int)received;
}

void xie_udp_close(UdpReceiver *udp) {
  if (!udp || udp->socket_fd < 0)
    return;

  close(udp->socket_fd);
  udp->socket_fd = -1;
}
