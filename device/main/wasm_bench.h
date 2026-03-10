#pragma once

#include <stdbool.h>
#include <stdint.h>

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
 * Run the WASM TCP echo benchmark: call the WASM export run_tcp_test(count),
 * then read back stats from WASM memory and print them over serial.
 */
void wasm_run_tcp_bench(uint32_t count);

/**
 * Run the WASM UDP echo benchmark: call the WASM export run_udp_test(count),
 * then read back stats from WASM memory and print them over serial.
 */
void wasm_run_udp_bench(uint32_t count);

/**
 * Run the WASM ICMP ping benchmark: call the WASM export run_ping_test(count),
 * then read back stats and print them over serial.
 */
void wasm_run_ping_bench(uint32_t count);
