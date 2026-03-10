#include <stdio.h>
#include <inttypes.h>

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
    printf("  BENCH_COUNT = %d\n", BENCH_COUNT);
    printf("--------------------------------------\n");
    printf("  1 = Native TCP\n");
    printf("  2 = WASM TCP\n");
    printf("  3 = Native UDP\n");
    printf("  4 = WASM UDP\n");
    printf("  5 = Native Ping\n");
    printf("  6 = WASM Ping\n");
    printf("  0 = Exit\n");
    printf("======================================\n");
    printf("Enter choice: ");
    fflush(stdout);
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
        case '1':
            ESP_LOGI(TAG, "Running native TCP benchmark (count=%d)", BENCH_COUNT);
            native_run_tcp_bench(BENCH_COUNT);
            break;
        case '2':
            ESP_LOGI(TAG, "Running WASM TCP benchmark (count=%d)", BENCH_COUNT);
            wasm_run_tcp_bench(BENCH_COUNT);
            break;
        case '3':
            ESP_LOGI(TAG, "Running native UDP benchmark (count=%d)", BENCH_COUNT);
            native_run_udp_bench(BENCH_COUNT);
            break;
        case '4':
            ESP_LOGI(TAG, "Running WASM UDP benchmark (count=%d)", BENCH_COUNT);
            wasm_run_udp_bench(BENCH_COUNT);
            break;
        case '5':
            ESP_LOGI(TAG, "Running native Ping benchmark (count=%d)", BENCH_COUNT);
            native_run_ping_bench(BENCH_COUNT);
            break;
        case '6':
            ESP_LOGI(TAG, "Running WASM Ping benchmark (count=%d)", BENCH_COUNT);
            wasm_run_ping_bench(BENCH_COUNT);
            break;
        case '0':
            ESP_LOGI(TAG, "Exit requested; stopping menu loop.");
            return;
        default:
            ESP_LOGW(TAG, "Unknown choice '%c'; please select 0–6.", choice);
            break;
        }
    }
}

