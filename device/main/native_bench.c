/**
 * Native benchmark: run N TCP/UDP/Ping requests using host_net.
 * We measure wall-clock total time and per-iteration time (for WASM vs native
 * overhead); ok/fail counts only (no mean/min/max RTT).
 */

 #include "native_bench.h"
 #include "host_net.h"
 #include "wasm_bench_config.h"
 #include <inttypes.h>
 #include <stdint.h>
 #include <stdlib.h>
 
 #include "esp_log.h"
 #include "esp_timer.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 
 static const char *TAG = "native_bench";
 
 /** Success/fail counts only (no RTT stats; we measure wall-clock time instead). */
 typedef struct {
     uint32_t count;
     uint32_t ok;
     uint32_t fail;
 } native_bench_stats_t;
 
 static void stats_init(native_bench_stats_t *s, uint32_t count)
 {
     s->count = count;
     s->ok    = 0;
     s->fail  = 0;
 }
 
 static void stats_add(native_bench_stats_t *s, int64_t rtt_us)
 {
     if (rtt_us < 0)
         s->fail++;
     else
         s->ok++;
 }
 
 static void stats_print(const char *proto, const native_bench_stats_t *s)
 {
     ESP_LOGI(TAG, "--- Native %s benchmark (count=%" PRIu32 ") ---", proto, s->count);
     ESP_LOGI(TAG, "  ok=%" PRIu32 "  fail=%" PRIu32, s->ok, s->fail);
     if (s->ok == 0)
         ESP_LOGW(TAG, "  no successful samples");
 }
 
 void native_run_tcp_bench(uint32_t count)
 {
     int64_t bench_start_us = esp_timer_get_time();
 
     native_bench_stats_t stats;
     stats_init(&stats, count);
 
     for (uint32_t i = 0; i < count; i++) {
         int64_t rtt = tcp_request_rtt_us(SERVER_IP, (uint16_t)TCP_ECHO_PORT);
         stats_add(&stats, rtt);
     }
 
     stats_print("TCP", &stats);
 
     int64_t bench_end_us = esp_timer_get_time();
     int64_t total_us = bench_end_us - bench_start_us;
     int64_t per_iter_us = (count > 0) ? total_us / (int64_t)count : 0;
     ESP_LOGI(TAG, "Native TCP bench total_us=%" PRId64 " per_iter_us=%" PRId64, total_us, per_iter_us);
 }
 
 void native_run_udp_bench(uint32_t count)
 {
     int64_t bench_start_us = esp_timer_get_time();
 
     native_bench_stats_t stats;
     stats_init(&stats, count);
 
     for (uint32_t i = 0; i < count; i++) {
         int64_t rtt = udp_request_rtt_us(SERVER_IP, (uint16_t)UDP_ECHO_PORT);
         stats_add(&stats, rtt);
     }
 
     stats_print("UDP", &stats);
 
     int64_t bench_end_us = esp_timer_get_time();
     int64_t total_us = bench_end_us - bench_start_us;
     int64_t per_iter_us = (count > 0) ? total_us / (int64_t)count : 0;
     ESP_LOGI(TAG, "Native UDP bench total_us=%" PRId64 " per_iter_us=%" PRId64, total_us, per_iter_us);
 }
 
 void native_run_ping_bench(uint32_t count)
 {
     int64_t bench_start_us = esp_timer_get_time();
 
     native_bench_stats_t stats;
     stats_init(&stats, count);
     ESP_LOGI(TAG, "Native Ping: target %s (%" PRIu32 " pings, %d ms delay between; delay subtracted from total)", SERVER_IP, count, PING_DELAY_MS);
 
     for (uint32_t i = 0; i < count; i++) {
         int64_t rtt = -1;
         if (ping_run_bench(SERVER_IP, 1, &rtt) != 0) {
             rtt = -1;
         }
         stats_add(&stats, rtt);
         if (i + 1 < count) {
             vTaskDelay(pdMS_TO_TICKS(PING_DELAY_MS));
         }
     }
 
     stats_print("Ping", &stats);
 
     int64_t bench_end_us = esp_timer_get_time();
     int64_t total_us = bench_end_us - bench_start_us;
     int64_t delay_us = (int64_t)(count > 0 ? count - 1 : 0) * PING_DELAY_MS * 1000;
     total_us -= delay_us;
     if (total_us < 0) {
         total_us = 0;
     }
     int64_t per_iter_us = (count > 0) ? total_us / (int64_t)count : 0;
     ESP_LOGI(TAG, "Native Ping bench total_us=%" PRId64 " (delay_subtracted) per_iter_us=%" PRId64, total_us, per_iter_us);
 }
 