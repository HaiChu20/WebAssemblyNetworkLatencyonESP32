#include <stdint.h>
#include <stddef.h>

#include "bench.h"
/* Share server IP and ports with device firmware (single source of truth). */
#include "../device/main/wasm_bench_config.h"

// ---------------------------------------------------------------------------
// Host imports: device (WASM3) provides these at runtime. Use import_module/import_name
// so clang emits them as WASM import symbols in the .o; wasm-ld then creates import section.
// Signature must match wasm_bench.c host_tcp_request_rtt_us / host_udp_request_rtt_us.
// ---------------------------------------------------------------------------

__attribute__((import_module("env"), import_name("tcp_request_rtt_us")))
extern int64_t tcp_request_rtt_us(int32_t host_ptr, int32_t host_len, int32_t port);

__attribute__((import_module("env"), import_name("udp_request_rtt_us")))
extern int64_t udp_request_rtt_us(int32_t host_ptr, int32_t host_len, int32_t port);

__attribute__((import_module("env"), import_name("ping_request_rtt_us")))
extern int64_t ping_request_rtt_us(int32_t host_ptr, int32_t host_len);

// ---------------------------------------------------------------------------
// Simple strlen implementation (we avoid pulling in libc).
// ---------------------------------------------------------------------------

static int32_t
str_len(const char *s)
{
    int32_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Stats layout at fixed offset (avoid offset 0 in case runtime treats it specially).
// Must match wasm_bench_stats_t and WASM_STATS_OFFSET in Code/device/main/wasm_bench.c.
// ---------------------------------------------------------------------------

#define WASM_STATS_OFFSET  256

typedef struct {
    uint32_t ok;
    uint32_t fail;
    int64_t  sum_us;
    int64_t  min_us;
    int64_t  max_us;
} wasm_bench_stats_t;

static wasm_bench_stats_t *
get_stats(void)
{
    return (wasm_bench_stats_t *)(uintptr_t)WASM_STATS_OFFSET;
}

static void
stats_reset(wasm_bench_stats_t *s)
{
    s->ok     = 0;
    s->fail   = 0;
    s->sum_us = 0;
    s->min_us = -1;
    s->max_us = -1;
}

static void
stats_add(wasm_bench_stats_t *s, int64_t rtt_us)
{
    if (rtt_us < 0) {
        s->fail++;
        return;
    }

    s->ok++;
    s->sum_us += rtt_us;

    if (s->min_us < 0 || rtt_us < s->min_us) {
        s->min_us = rtt_us;
    }
    if (s->max_us < 0 || rtt_us > s->max_us) {
        s->max_us = rtt_us;
    }
}

// ---------------------------------------------------------------------------
// Host / port constants for the benchmark.
// We reuse SERVER_IP, TCP_ECHO_PORT and UDP_ECHO_PORT from wasm_bench_config.h
// so device and WASM module share one source of truth.
// ---------------------------------------------------------------------------

static const char HOST_STR[] = SERVER_IP;

// ---------------------------------------------------------------------------
// Exported benchmark functions
// ---------------------------------------------------------------------------

void
run_tcp_test(int32_t count)
{
    wasm_bench_stats_t *stats = get_stats();
    stats_reset(stats);

    int32_t host_ptr = (int32_t)(uintptr_t)HOST_STR;
    int32_t host_len = str_len(HOST_STR);

    for (int32_t i = 0; i < count; i++) {
        int64_t rtt = tcp_request_rtt_us(host_ptr, host_len, TCP_ECHO_PORT);
        stats_add(stats, rtt);
    }
}

void
run_udp_test(int32_t count)
{
    wasm_bench_stats_t *stats = get_stats();
    stats_reset(stats);

    int32_t host_ptr = (int32_t)(uintptr_t)HOST_STR;
    int32_t host_len = str_len(HOST_STR);

    for (int32_t i = 0; i < count; i++) {
        int64_t rtt = udp_request_rtt_us(host_ptr, host_len, UDP_ECHO_PORT);
        stats_add(stats, rtt);
    }
}

void
run_ping_test(int32_t count)
{
    wasm_bench_stats_t *stats = get_stats();
    stats_reset(stats);

    int32_t host_ptr = (int32_t)(uintptr_t)HOST_STR;
    int32_t host_len = str_len(HOST_STR);

    for (int32_t i = 0; i < count; i++) {
        int64_t rtt = ping_request_rtt_us(host_ptr, host_len);
        stats_add(stats, rtt);
    }
}

