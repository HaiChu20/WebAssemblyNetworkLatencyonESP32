#pragma once

#include <stdint.h>

/**
 * Perform one full TCP echo request: socket, connect, send, recv, close.
 * Timing includes all of the above (full request time, not only send/recv).
 *
 * Returns:
 *  - Time in microseconds on success.
 *  - -1 on error (connection, send, recv, timeout, etc.).
 */
int64_t tcp_request_rtt_us(const char *host, uint16_t port);

/**
 * Perform one full UDP echo request: socket, sendto, recvfrom, close.
 * Timing includes all of the above (full request time, not only send/recv).
 *
 * Returns:
 *  - Time in microseconds on success.
 *  - -1 on error (send, recv, timeout, etc.).
 */
int64_t udp_request_rtt_us(const char *host, uint16_t port);

/**
 * Run N pings in one session (one socket). Fills rtt_us_out[i] with RTT in
 * microseconds or -1 on timeout. Returns 0 on success, -1 if session failed.
 * Use count=1 for a single ping; use this to avoid exhausting LwIP sockets.
 */
int ping_run_bench(const char *host, uint32_t count, int64_t *rtt_us_out);

