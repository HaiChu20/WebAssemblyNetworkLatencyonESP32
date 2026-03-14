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


typedef struct {
    int64_t *rtt_us;
    uint32_t count;
    SemaphoreHandle_t done;
} ping_bench_ctx_t;

/* esp_ping uses 1-based sequence numbers (1..count). Map to 0-based index. */
static void on_bench_ping_success(esp_ping_handle_t hdl, void *args)
{
    ping_bench_ctx_t *ctx = (ping_bench_ctx_t *)args;
    uint16_t seqno;
    uint32_t elapsed_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_ms, sizeof(elapsed_ms));
    if (ctx->rtt_us != NULL && seqno >= 1 && seqno <= ctx->count) {
        ctx->rtt_us[seqno - 1] = (int64_t)elapsed_ms * 1000;
    }
}

static void on_bench_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    ping_bench_ctx_t *ctx = (ping_bench_ctx_t *)args;
    uint16_t seqno;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    if (ctx->rtt_us != NULL && seqno >= 1 && seqno <= ctx->count) {
        ctx->rtt_us[seqno - 1] = -1;
    }
}

static void on_bench_ping_end(esp_ping_handle_t hdl, void *args)
{
    ping_bench_ctx_t *ctx = (ping_bench_ctx_t *)args;
    if (ctx->done != NULL) {
        xSemaphoreGive(ctx->done);
    }
    (void)hdl;
}

int
ping_run_bench(const char *host, uint32_t count, int64_t *rtt_us_out)
{
    if (!host || count == 0 || rtt_us_out == NULL) {
        return -1;
    }

    ip_addr_t target;
    memset(&target, 0, sizeof(target));
    if (!ip4addr_aton(host, ip_2_ip4(&target))) {
        ESP_LOGE(HOST_NET_LOG_TAG, "Invalid IP for ping: '%s'", host);
        return -1;
    }

    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    if (done == NULL) {
        ESP_LOGE(HOST_NET_LOG_TAG, "ping_run_bench: semaphore create failed");
        return -1;
    }

    for (uint32_t i = 0; i < count; i++) {
        rtt_us_out[i] = -1;
    }

    ping_bench_ctx_t ctx = {
        .rtt_us = rtt_us_out,
        .count = count,
        .done = done,
    };

    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    cfg.target_addr = target;
    cfg.count = count;
    cfg.timeout_ms = 2000;
    cfg.interval_ms = 1;    /* minimal gap so total_us reflects RTT, not spacing */

    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_bench_ping_success,
        .on_ping_timeout = on_bench_ping_timeout,
        .on_ping_end = on_bench_ping_end,
        .cb_args = &ctx,
    };

    esp_ping_handle_t hdl = NULL;
    esp_err_t err = esp_ping_new_session(&cfg, &cbs, &hdl);
    if (err != ESP_OK || hdl == NULL) {
        ESP_LOGE(HOST_NET_LOG_TAG, "ping_run_bench: esp_ping_new_session failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(done);
        return -1;
    }

    esp_ping_start(hdl);

    /* Wait for all count pings to finish: timeout ~ count * (timeout_ms + interval_ms) */
    uint32_t wait_ms = count * (2000 + 1) + 2000;
    if (xSemaphoreTake(done, pdMS_TO_TICKS(wait_ms)) != pdTRUE) {
        ESP_LOGW(HOST_NET_LOG_TAG, "ping_run_bench: wait timeout");
    }
    esp_ping_stop(hdl);
    esp_ping_delete_session(hdl);
    vSemaphoreDelete(done);

    return 0;
}
