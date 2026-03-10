/**
 * WASM benchmark: load bench.wasm, register host functions (tcp/udp RTT, run_ping_bench).
 * Call run_tcp_test(count) / run_udp_test(count) / run_ping_test(count), read stats
 * from WASM memory and print. Same pattern for TCP, UDP, and Ping.
 * The WASM binary must be provided at link time (bench_wasm, bench_wasm_len).
 */

#include "wasm_bench.h"
#include "host_net.h"
#include "wasm_bench_config.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wasm3.h"
#include "m3_env.h"

static const char *TAG = "wasm_bench";

/** WASM runtime stack (bytes). */
#define WASM_STACK_SIZE  (4 * 1024)

/** Linear memory: 1 page (64KB). 2-page build needs 128KB and fails allocation on ESP32-C3. */
#define WASM_MEMORY_LIMIT  (64 * 1024)

/**
 * Stats layout and offset. Must match wasm_module/bench.c (WASM_STATS_OFFSET).
 */
#define WASM_STATS_OFFSET  256
typedef struct __attribute__((packed)) {
	uint32_t ok;
	uint32_t fail;
	int64_t  sum_us;
	int64_t  min_us;
	int64_t  max_us;
} wasm_bench_stats_t;

/** Provided by embedding bench.wasm (e.g. from build or bench_wasm.c). */
extern const uint8_t  bench_wasm[];
extern const unsigned bench_wasm_len;

static IM3Environment  s_env   = NULL;
static IM3Runtime      s_runtime = NULL;
static IM3Function     s_run_tcp = NULL;
static IM3Function     s_run_udp = NULL;
static IM3Function     s_run_ping = NULL;

/* -------------------------------------------------------------------------
 * Host function: TCP RTT (WASM imports env.tcp_request_rtt_us(ptr, len, port) -> i64)
 * Use device config SERVER_IP so WASM and native hit the same server (WASM's
 * HOST_STR is baked at build time and may not match).
 * ------------------------------------------------------------------------- */
static m3ApiRawFunction(host_tcp_request_rtt_us)
{
	m3ApiReturnType(int64_t);
	m3ApiGetArg(uint32_t, host_ptr);
	m3ApiGetArg(uint32_t, host_len);
	m3ApiGetArg(uint32_t, port);

	(void)host_ptr;
	(void)host_len;
	(void)port;
	int64_t rtt = tcp_request_rtt_us(SERVER_IP, (uint16_t)TCP_ECHO_PORT);
	m3ApiReturn(rtt);
}

/* -------------------------------------------------------------------------
 * Host function: UDP RTT. Use device config SERVER_IP (same as TCP host above).
 * ------------------------------------------------------------------------- */
static m3ApiRawFunction(host_udp_request_rtt_us)
{
	m3ApiReturnType(int64_t);
	m3ApiGetArg(uint32_t, host_ptr);
	m3ApiGetArg(uint32_t, host_len);
	m3ApiGetArg(uint32_t, port);

	(void)host_ptr;
	(void)host_len;
	(void)port;
	int64_t rtt = udp_request_rtt_us(SERVER_IP, (uint16_t)UDP_ECHO_PORT);
	m3ApiReturn(rtt);
}

/* -------------------------------------------------------------------------
 * Host function: single ping RTT. Delay after each ping to avoid socket exhaustion;
 * caller subtracts total delay from reported time.
 * ------------------------------------------------------------------------- */
static m3ApiRawFunction(host_ping_request_rtt_us)
{
	m3ApiReturnType(int64_t);
	m3ApiGetArg(uint32_t, host_ptr);
	m3ApiGetArg(uint32_t, host_len);

	(void)host_ptr;
	(void)host_len;
	int64_t rtt = -1;
	if (ping_run_bench(SERVER_IP, 1, &rtt) != 0) {
		rtt = -1;
	}
	vTaskDelay(pdMS_TO_TICKS(PING_DELAY_MS));
	m3ApiReturn(rtt);
}

/* -------------------------------------------------------------------------
 * Load module, link host functions, resolve exports.
 * ------------------------------------------------------------------------- */
bool wasm_bench_init(void)
{
	if (s_runtime != NULL) {
		ESP_LOGW(TAG, "already initialized");
		return true;
	}

	if (bench_wasm_len == 0) {
		ESP_LOGE(TAG, "bench_wasm_len is 0; embed bench.wasm or provide bench_wasm");
		return false;
	}

	s_env = m3_NewEnvironment();
	if (s_env == NULL) {
		ESP_LOGE(TAG, "m3_NewEnvironment failed");
		return false;
	}

	s_runtime = m3_NewRuntime(s_env, WASM_STACK_SIZE, NULL);
	if (s_runtime == NULL) {
		ESP_LOGE(TAG, "m3_NewRuntime failed");
		m3_FreeEnvironment(s_env);
		s_env = NULL;
		return false;
	}
	s_runtime->memoryLimit = WASM_MEMORY_LIMIT;

	IM3Module module = NULL;
	M3Result err = m3_ParseModule(s_env, &module, bench_wasm, bench_wasm_len);
	if (err != NULL) {
		ESP_LOGE(TAG, "m3_ParseModule: %s", err);
		goto fail;
	}

	err = m3_LoadModule(s_runtime, module);
	if (err != NULL) {
		ESP_LOGE(TAG, "m3_LoadModule: %s", err);
		goto fail;
	}

	err = m3_LinkRawFunction(module, "*", "tcp_request_rtt_us", "I(iii)", host_tcp_request_rtt_us);
	if (err != NULL) {
		ESP_LOGE(TAG, "link tcp_request_rtt_us: %s", err);
		goto fail;
	}
	err = m3_LinkRawFunction(module, "*", "udp_request_rtt_us", "I(iii)", host_udp_request_rtt_us);
	if (err != NULL) {
		ESP_LOGE(TAG, "link udp_request_rtt_us: %s", err);
		goto fail;
	}
	err = m3_LinkRawFunction(module, "*", "ping_request_rtt_us", "I(ii)", host_ping_request_rtt_us);
	if (err != NULL) {
		ESP_LOGE(TAG, "link ping_request_rtt_us: %s", err);
		goto fail;
	}

	err = m3_FindFunction(&s_run_tcp, s_runtime, "run_tcp_test");
	if (err != NULL || s_run_tcp == NULL) {
		ESP_LOGE(TAG, "run_tcp_test not found: %s", err ? err : "null");
		goto fail;
	}

	err = m3_FindFunction(&s_run_udp, s_runtime, "run_udp_test");
	if (err != NULL || s_run_udp == NULL) {
		ESP_LOGE(TAG, "run_udp_test not found: %s", err ? err : "null");
		goto fail;
	}
	err = m3_FindFunction(&s_run_ping, s_runtime, "run_ping_test");
	if (err != NULL || s_run_ping == NULL) {
		ESP_LOGE(TAG, "run_ping_test not found: %s", err ? err : "null");
		goto fail;
	}

	ESP_LOGI(TAG, "WASM benchmark init OK");
	return true;

fail:
	m3_FreeRuntime(s_runtime);
	m3_FreeEnvironment(s_env);
	s_runtime = NULL;
	s_env = NULL;
	s_run_tcp = NULL;
	s_run_udp = NULL;
	s_run_ping = NULL;
	return false;
}

void wasm_bench_cleanup(void)
{
	if (s_runtime) {
		m3_FreeRuntime(s_runtime);
		s_runtime = NULL;
		s_run_tcp = NULL;
		s_run_udp = NULL;
		s_run_ping = NULL;
	}
	if (s_env) {
		m3_FreeEnvironment(s_env);
		s_env = NULL;
	}
	ESP_LOGI(TAG, "WASM benchmark cleanup");
}

static void print_wasm_stats(const char *proto, uint32_t count,
                            const wasm_bench_stats_t *s)
{
	ESP_LOGI(TAG, "--- WASM %s benchmark (count=%" PRIu32 ") ---", proto, count);
	ESP_LOGI(TAG, "  ok=%" PRIu32 "  fail=%" PRIu32, s->ok, s->fail);
	if (s->ok == 0)
		ESP_LOGW(TAG, "  no successful samples");
}

static bool call_run_tcp(uint32_t count)
{
	const void *args[] = { &count };
	M3Result err = m3_Call(s_run_tcp, 1, args);
	if (err != NULL) {
		ESP_LOGE(TAG, "run_tcp_test call: %s", err);
		return false;
	}
	return true;
}

static bool call_run_udp(uint32_t count)
{
	const void *args[] = { &count };
	M3Result err = m3_Call(s_run_udp, 1, args);
	if (err != NULL) {
		ESP_LOGE(TAG, "run_udp_test call: %s", err);
		return false;
	}
	return true;
}

static bool call_run_ping(uint32_t count)
{
	const void *args[] = { &count };
	M3Result err = m3_Call(s_run_ping, 1, args);
	if (err != NULL) {
		ESP_LOGE(TAG, "run_ping_test call: %s", err);
		return false;
	}
	return true;
}

void wasm_run_tcp_bench(uint32_t count)
{
	int64_t bench_start_us = esp_timer_get_time();

	if (s_run_tcp == NULL) {
		ESP_LOGE(TAG, "WASM not initialized or run_tcp_test missing");
		return;
	}
	uint32_t n = (count > (uint32_t)BENCH_COUNT) ? (uint32_t)BENCH_COUNT : count;
	if (n < count)
		ESP_LOGI(TAG, "WASM TCP: running %" PRIu32 " of %" PRIu32 " (stack limit)", n, count);

	if (!call_run_tcp(n))
		return;

	uint32_t mem_size = 0;
	uint8_t *mem = m3_GetMemory(s_runtime, &mem_size, 0);
	if (mem == NULL || mem_size < WASM_STATS_OFFSET + sizeof(wasm_bench_stats_t)) {
		ESP_LOGE(TAG, "no linear memory or too small for stats");
		return;
	}

	wasm_bench_stats_t stats;
	memcpy(&stats, mem + WASM_STATS_OFFSET, sizeof(stats));
	print_wasm_stats("TCP", n, &stats);

	int64_t bench_end_us = esp_timer_get_time();
	int64_t total_us = bench_end_us - bench_start_us;
	int64_t per_iter_us = (n > 0) ? total_us / (int64_t)n : 0;
	ESP_LOGI(TAG, "WASM TCP bench total_us=%" PRId64 " per_iter_us=%" PRId64, total_us, per_iter_us);
}

void wasm_run_udp_bench(uint32_t count)
{
	int64_t bench_start_us = esp_timer_get_time();

	if (s_run_udp == NULL) {
		ESP_LOGE(TAG, "WASM not initialized or run_udp_test missing");
		return;
	}
	uint32_t n = (count > (uint32_t)BENCH_COUNT) ? (uint32_t)BENCH_COUNT : count;
	if (n < count)
		ESP_LOGI(TAG, "WASM UDP: running %" PRIu32 " of %" PRIu32 " (stack limit)", n, count);

	if (!call_run_udp(n))
		return;

	uint32_t mem_size = 0;
	uint8_t *mem = m3_GetMemory(s_runtime, &mem_size, 0);
	if (mem == NULL || mem_size < WASM_STATS_OFFSET + sizeof(wasm_bench_stats_t)) {
		ESP_LOGE(TAG, "no linear memory or too small for stats");
		return;
	}

	wasm_bench_stats_t stats;
	memcpy(&stats, mem + WASM_STATS_OFFSET, sizeof(stats));
	print_wasm_stats("UDP", n, &stats);

	int64_t bench_end_us = esp_timer_get_time();
	int64_t total_us = bench_end_us - bench_start_us;
	int64_t per_iter_us = (n > 0) ? total_us / (int64_t)n : 0;
	ESP_LOGI(TAG, "WASM UDP bench total_us=%" PRId64 " per_iter_us=%" PRId64, total_us, per_iter_us);
}

void wasm_run_ping_bench(uint32_t count)
{
	int64_t bench_start_us = esp_timer_get_time();

	if (s_run_ping == NULL) {
		ESP_LOGE(TAG, "WASM not initialized or run_ping_test missing");
		return;
	}
	uint32_t n = (count > (uint32_t)BENCH_COUNT) ? (uint32_t)BENCH_COUNT : count;
	if (n < count)
		ESP_LOGI(TAG, "WASM Ping: running %" PRIu32 " of %" PRIu32 " (stack limit)", n, count);

	if (!call_run_ping(n))
		return;

	uint32_t mem_size = 0;
	uint8_t *mem = m3_GetMemory(s_runtime, &mem_size, 0);
	if (mem == NULL || mem_size < WASM_STATS_OFFSET + sizeof(wasm_bench_stats_t)) {
		ESP_LOGE(TAG, "no linear memory or too small for stats");
		return;
	}

	wasm_bench_stats_t stats;
	memcpy(&stats, mem + WASM_STATS_OFFSET, sizeof(stats));
	print_wasm_stats("Ping", n, &stats);

	int64_t bench_end_us = esp_timer_get_time();
	int64_t total_us = bench_end_us - bench_start_us;
	int64_t delay_us = (int64_t)n * PING_DELAY_MS * 1000;
	total_us -= delay_us;
	if (total_us < 0) {
		total_us = 0;
	}
	int64_t per_iter_us = (n > 0) ? total_us / (int64_t)n : 0;
	ESP_LOGI(TAG, "WASM Ping bench total_us=%" PRId64 " (delay_subtracted) per_iter_us=%" PRId64, total_us, per_iter_us);
}
