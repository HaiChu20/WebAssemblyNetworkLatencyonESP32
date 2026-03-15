#pragma once

// Server IP for all benchmarks. TCP/UDP use this + port; Ping uses this only (ICMP, no port).
// Adjust to match the laptop (e.g. when laptop is hotspot/router, often 192.168.137.1).

#define SERVER_IP       "192.168.137.1"
#define TCP_ECHO_PORT   9000
#define UDP_ECHO_PORT   9001

// Delay (ms) between each single-ping call to avoid LwIP socket exhaustion. Subtracted from reported total.
#define PING_DELAY_MS   100

