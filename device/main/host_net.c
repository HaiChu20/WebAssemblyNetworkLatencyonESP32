#include "host_net.h"

#include <stdbool.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"

#define HOST_NET_LOG_TAG "host_net"

// Small echo payload used for both TCP and UDP.
static const char *ECHO_PAYLOAD = "ping";
static const size_t ECHO_PAYLOAD_LEN = 4;

static bool
resolve_host(const char *host, uint16_t port, struct sockaddr_in *out_addr)
{
    if (!host || !out_addr) {
        return false;
    }

    memset(out_addr, 0, sizeof(*out_addr));
    out_addr->sin_family = AF_INET;
    out_addr->sin_port = htons(port);

    if (!inet_aton(host, &out_addr->sin_addr)) {
        ESP_LOGE(HOST_NET_LOG_TAG, "Invalid IP '%s' (use dotted-quad e.g. 192.168.1.100)", host);
        return false;
    }
    return true;
}

int64_t
tcp_request_rtt_us(const char *host, uint16_t port)
{
    struct sockaddr_in dest_addr;
    if (!resolve_host(host, port, &dest_addr)) {
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "Unable to create TCP socket: errno=%d", errno);
        return -1;
    }

    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0,
    };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "TCP connect failed: errno=%d", errno);
        close(sock);
        return -1;
    }

    int64_t start_us = esp_timer_get_time();

    ssize_t to_send = (ssize_t)ECHO_PAYLOAD_LEN;
    const char *buf = ECHO_PAYLOAD;
    while (to_send > 0) {
        ssize_t sent = send(sock, buf, to_send, 0);
        if (sent < 0) {
            ESP_LOGE(HOST_NET_LOG_TAG, "TCP send failed: errno=%d", errno);
            close(sock);
            return -1;
        }
        to_send -= sent;
        buf += sent;
    }

    char rx_buf[16];
    ssize_t recvd = recv(sock, rx_buf, sizeof(rx_buf), 0);
    if (recvd <= 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "TCP recv failed or timed out: errno=%d", errno);
        close(sock);
        return -1;
    }

    int64_t end_us = esp_timer_get_time();
    close(sock);

    return end_us - start_us;
}

int64_t
udp_request_rtt_us(const char *host, uint16_t port)
{
    struct sockaddr_in dest_addr;
    if (!resolve_host(host, port, &dest_addr)) {
        return -1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "Unable to create UDP socket: errno=%d", errno);
        return -1;
    }

    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0,
    };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    int64_t start_us = esp_timer_get_time();

    ssize_t sent = sendto(sock,
                          ECHO_PAYLOAD,
                          ECHO_PAYLOAD_LEN,
                          0,
                          (struct sockaddr *)&dest_addr,
                          sizeof(dest_addr));
    if (sent < 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "UDP sendto failed: errno=%d", errno);
        close(sock);
        return -1;
    }

    char rx_buf[32];
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    ssize_t recvd = recvfrom(sock,
                             rx_buf,
                             sizeof(rx_buf),
                             0,
                             (struct sockaddr *)&source_addr,
                             &socklen);
    if (recvd <= 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "UDP recvfrom failed or timed out: errno=%d", errno);
        close(sock);
        return -1;
    }

    int64_t end_us = esp_timer_get_time();
    close(sock);

    return end_us - start_us;
}

int64_t
tcp_request_rtt_us(const char *host, uint16_t port)
{
    struct sockaddr_in dest_addr;
    if (!resolve_host(host, port, &dest_addr)) {
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "Unable to create TCP socket: errno=%d", errno);
        return -1;
    }

    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0,
    };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "TCP connect failed: errno=%d", errno);
        close(sock);
        return -1;
    }

    int64_t start_us = esp_timer_get_time();

    ssize_t to_send = (ssize_t)ECHO_PAYLOAD_LEN;
    const char *buf = ECHO_PAYLOAD;
    while (to_send > 0) {
        ssize_t sent = send(sock, buf, to_send, 0);
        if (sent < 0) {
            ESP_LOGE(HOST_NET_LOG_TAG, "TCP send failed: errno=%d", errno);
            close(sock);
            return -1;
        }
        to_send -= sent;
        buf += sent;
    }

    char rx_buf[16];
    ssize_t recvd = recv(sock, rx_buf, sizeof(rx_buf), 0);
    if (recvd <= 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "TCP recv failed or timed out: errno=%d", errno);
        close(sock);
        return -1;
    }

    int64_t end_us = esp_timer_get_time();
    close(sock);

    return end_us - start_us;
}

int64_t
udp_request_rtt_us(const char *host, uint16_t port)
{
    struct sockaddr_in dest_addr;
    if (!resolve_host(host, port, &dest_addr)) {
        return -1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "Unable to create UDP socket: errno=%d", errno);
        return -1;
    }

    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0,
    };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    int64_t start_us = esp_timer_get_time();

    ssize_t sent = sendto(sock,
                          ECHO_PAYLOAD,
                          ECHO_PAYLOAD_LEN,
                          0,
                          (struct sockaddr *)&dest_addr,
                          sizeof(dest_addr));
    if (sent < 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "UDP sendto failed: errno=%d", errno);
        close(sock);
        return -1;
    }

    char rx_buf[32];
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    ssize_t recvd = recvfrom(sock,
                             rx_buf,
                             sizeof(rx_buf),
                             0,
                             (struct sockaddr *)&source_addr,
                             &socklen);
    if (recvd <= 0) {
        ESP_LOGE(HOST_NET_LOG_TAG, "UDP recvfrom failed or timed out: errno=%d", errno);
        close(sock);
        return -1;
    }

    int64_t end_us = esp_timer_get_time();
    close(sock);

    return end_us - start_us;
}
