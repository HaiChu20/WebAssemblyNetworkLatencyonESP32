#pragma once

#include <stdint.h>

/**
 * Run the native TCP echo benchmark: perform `count` round-trips to the
 * configured server (SERVER_IP, TCP_ECHO_PORT), collect RTTs, and print
 * stats (mean, min, max, total, failures) over serial.
 */
void native_run_tcp_bench(uint32_t count);

/**
 * Run the native UDP echo benchmark: perform `count` round-trips to the
 * configured server (SERVER_IP, UDP_ECHO_PORT), collect RTTs, and print
 * stats (mean, min, max, total, failures) over serial.
 */
void native_run_udp_bench(uint32_t count);

/**
 * Run the native ICMP ping benchmark: perform `count` pings to SERVER_IP,
 * collect RTTs, and print stats over serial.
 */
void native_run_ping_bench(uint32_t count);
