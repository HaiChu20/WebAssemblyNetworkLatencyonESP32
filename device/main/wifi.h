#pragma once

#include <stdbool.h>

/**
 * Initialise WiFi in station mode and start connection using
 * SSID/PASSWORD from sdkconfig (e.g. CONFIG_WIFI_SSID / CONFIG_WIFI_PASSWORD).
 */
void wifi_init_sta(void);

/**
 * Block until WiFi is connected or timeout (in milliseconds) expires.
 * Returns true if connected, false on timeout or error.
 */
bool wifi_wait_connected(int timeout_ms);

