#pragma once

#include <stdint.h>

// Exported benchmark entry points for the WASM module.
// These functions are called from the ESP32-C3 host via the WASM3 runtime.

void run_tcp_test(int32_t count);
void run_udp_test(int32_t count);
void run_ping_test(int32_t count);

