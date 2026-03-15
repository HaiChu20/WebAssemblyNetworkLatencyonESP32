#pragma once

#include <stdint.h>

/** Optional result for CSV output. Pass NULL to run_*_bench for normal log-only behavior. */
typedef struct bench_result {
	uint32_t ok;
	uint32_t fail;
	int64_t total_us;
	int64_t per_iter_us;
} bench_result_t;

/**
 * Run the native TCP echo benchmark. If result != NULL, fill it (ok, fail, total_us, per_iter_us).
 */
void native_run_tcp_bench(uint32_t count, bench_result_t *result);

/**
 * Run the native UDP echo benchmark. If result != NULL, fill it.
 */
void native_run_udp_bench(uint32_t count, bench_result_t *result);

/**
 * Run the native ICMP ping benchmark. If result != NULL, fill it.
 */
void native_run_ping_bench(uint32_t count, bench_result_t *result);
