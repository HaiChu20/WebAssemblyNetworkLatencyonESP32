#pragma once

#include <stdint.h>

/**
 * Perform a single TCP echo round-trip to the given host/port.
 *
 * Returns:
 *  - RTT in microseconds on success.
 *  - -1 on error (connection, send, recv, timeout, etc.).
 */
int64_t tcp_request_rtt_us(const char *host, uint16_t port);

/**
 * Perform a single UDP echo round-trip to the given host/port.
 *
 * Returns:
 *  - RTT in microseconds on success.
 *  - -1 on error (send, recv, timeout, etc.).
 */
int64_t udp_request_rtt_us(const char *host, uint16_t port);

/**
 * Run N pings in one session (one socket). Fills rtt_us_out[i] with RTT in
 * microseconds or -1 on timeout. Returns 0 on success, -1 if session failed.
 */
int ping_run_bench(const char *host, uint32_t count, int64_t *rtt_us_out);

