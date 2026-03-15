#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "native_bench.h"

/**
 * Initialize the WASM benchmark: load bench.wasm, create runtime, link host
 * functions (tcp_request_rtt_us, udp_request_rtt_us). Call once after WiFi
 * is up. Returns false if the module could not be loaded.
 */
bool wasm_bench_init(void);

/**
 * Free WASM runtime and environment. Call when shutting down or before
 * re-loading the module.
 */
void wasm_bench_cleanup(void);

/**
 * Run the WASM TCP echo benchmark. If result != NULL, fill it.
 */
void wasm_run_tcp_bench(uint32_t count, bench_result_t *result);

/**
 * Run the WASM UDP echo benchmark. If result != NULL, fill it.
 */
void wasm_run_udp_bench(uint32_t count, bench_result_t *result);

/**
 * Run the WASM ICMP ping benchmark. If result != NULL, fill it.
 */
void wasm_run_ping_bench(uint32_t count, bench_result_t *result);
