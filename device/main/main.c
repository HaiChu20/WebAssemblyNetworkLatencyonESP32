#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi.h"
#include "native_bench.h"
#include "wasm_bench.h"
#include "wasm_bench_config.h"

static const char *TAG = "app_main";

static void
print_menu(void)
{
    printf("\n");
    printf("======================================\n");
    printf("  Design A Network Benchmark Menu\n");
    printf("  Server: %s (TCP %d, UDP %d)\n",
           SERVER_IP,
           TCP_ECHO_PORT,
           UDP_ECHO_PORT);
    printf("  count: enter when run (1–100000)\n");
    printf("--------------------------------------\n");
    printf("  1 = Native TCP\n");
    printf("  2 = WASM TCP\n");
    printf("  3 = Native UDP\n");
    printf("  4 = WASM UDP\n");
    printf("  5 = Native Ping\n");
    printf("  6 = WASM Ping\n");
    printf("  7 = Run all (output CSV)\n");
    printf("  0 = Exit\n");
    printf("======================================\n");
    printf("Enter choice: ");
    fflush(stdout);
}

/* Read a positive integer from stdin (digits until newline). Returns 0 on invalid/empty. */
static uint32_t
read_count(void)
{
    char buf[16];
    int i = 0;
    int c;
    while (i < (int)sizeof(buf) - 1) {
        c = getchar();
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (c == '\r' || c == '\n') {
            break;
        }
        if (isdigit((unsigned char)c)) {
            buf[i++] = (char)c;
        }
    }
    buf[i] = '\0';
    if (i == 0) {
        return 0;
    }
    return (uint32_t)atoi(buf);
}

static int
read_choice_char(void)
{
    int c;
    do {
        c = getchar();
        if (c == '\r' || c == '\n') {
            continue;
        }
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        return c;
    } while (1);
}

void
app_main(void)
{
    ESP_LOGI(TAG, "Starting Design A benchmark app");

    // Initialise WiFi and wait for connection.
    wifi_init_sta();
    if (!wifi_wait_connected(15000)) {
        ESP_LOGE(TAG, "WiFi not connected within timeout; benchmarks may fail.");
    } else {
        ESP_LOGI(TAG, "WiFi connected; ready to run benchmarks.");
    }

    if (!wasm_bench_init()) {
        ESP_LOGW(TAG, "WASM benchmark init failed; WASM options may not work.");
    } else {
        ESP_LOGI(TAG, "WASM benchmark initialised.");
    }

    while (1) {
        print_menu();
        int choice = read_choice_char();
        printf("%c\n", choice);  // echo back choice

        switch (choice) {
        case '1': case '2': case '3': case '4': case '5': case '6': {
            printf("\nEnter count: ");
            fflush(stdout);
            uint32_t n = read_count();
            printf("%" PRIu32 "\n", n);
            if (n == 0 || n > 100000) {
                ESP_LOGW(TAG, "Invalid count (use 1–100000)");
                break;
            }
            switch (choice) {
            case '1':
                ESP_LOGI(TAG, "Running native TCP benchmark (count=%" PRIu32 ")", n);
                native_run_tcp_bench(n, NULL);
                break;
            case '2':
                ESP_LOGI(TAG, "Running WASM TCP benchmark (count=%" PRIu32 ")", n);
                wasm_run_tcp_bench(n, NULL);
                break;
            case '3':
                ESP_LOGI(TAG, "Running native UDP benchmark (count=%" PRIu32 ")", n);
                native_run_udp_bench(n, NULL);
                break;
            case '4':
                ESP_LOGI(TAG, "Running WASM UDP benchmark (count=%" PRIu32 ")", n);
                wasm_run_udp_bench(n, NULL);
                break;
            case '5':
                ESP_LOGI(TAG, "Running native Ping benchmark (count=%" PRIu32 ")", n);
                native_run_ping_bench(n, NULL);
                break;
            case '6':
                ESP_LOGI(TAG, "Running WASM Ping benchmark (count=%" PRIu32 ")", n);
                wasm_run_ping_bench(n, NULL);
                break;
            default:
                break;
            }
            break;
        }
        case '7': {
            printf("\nEnter count: ");
            fflush(stdout);
            uint32_t n = read_count();
            printf("%" PRIu32 "\n", n);
            if (n == 0 || n > 100000) {
                ESP_LOGW(TAG, "Invalid count (use 1–100000)");
                break;
            }
            bench_result_t r[6];
            memset(r, 0, sizeof(r));
            ESP_LOGI(TAG, "Running all 6 benchmarks (count=%" PRIu32 ")...", n);
            native_run_tcp_bench(n, &r[0]);
            wasm_run_tcp_bench(n, &r[1]);
            native_run_udp_bench(n, &r[2]);
            wasm_run_udp_bench(n, &r[3]);
            native_run_ping_bench(n, &r[4]);
            /* Delay so LwIP releases ping sockets before WASM Ping (avoids "create socket failed") */
            vTaskDelay(pdMS_TO_TICKS(1500));
            wasm_run_ping_bench(n, &r[5]);
            printf("name,count,ok,fail,total_us,per_iter_us\n");
            printf("Native TCP,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRId64 "\n", n, r[0].ok, r[0].fail, (int64_t)r[0].total_us, (int64_t)r[0].per_iter_us);
            printf("WASM TCP,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRId64 "\n", n, r[1].ok, r[1].fail, (int64_t)r[1].total_us, (int64_t)r[1].per_iter_us);
            printf("Native UDP,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRId64 "\n", n, r[2].ok, r[2].fail, (int64_t)r[2].total_us, (int64_t)r[2].per_iter_us);
            printf("WASM UDP,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRId64 "\n", n, r[3].ok, r[3].fail, (int64_t)r[3].total_us, (int64_t)r[3].per_iter_us);
            printf("Native Ping,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRId64 "\n", n, r[4].ok, r[4].fail, (int64_t)r[4].total_us, (int64_t)r[4].per_iter_us);
            printf("WASM Ping,%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRId64 "\n", n, r[5].ok, r[5].fail, (int64_t)r[5].total_us, (int64_t)r[5].per_iter_us);
            break;
        }
        case '0':
            ESP_LOGI(TAG, "Exit requested; stopping menu loop.");
            return;
        default:
            ESP_LOGW(TAG, "Unknown choice '%c'; please select 0–7.", choice);
            break;
        }
    }
}

