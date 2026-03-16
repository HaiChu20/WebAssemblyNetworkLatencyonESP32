import argparse
import logging
import socket
import threading
import time
from typing import Tuple


TCP_PORT_DEFAULT = 9000
UDP_PORT_DEFAULT = 9001
HOST_DEFAULT = "0.0.0.0"
BUFFER_SIZE = 2048


def setup_logging(verbose: bool = False) -> None:
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(levelname)s] %(threadName)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )


def create_tcp_socket(host: str, port: int) -> socket.socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((host, port))
    sock.listen()
    logging.info("TCP echo server listening on %s:%d", host, port)
    return sock


def create_udp_socket(host: str, port: int) -> socket.socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((host, port))
    logging.info("UDP echo server listening on %s:%d", host, port)
    return sock


def tcp_echo_loop(host: str, port: int) -> None:
    sock = create_tcp_socket(host, port)
    with sock:
        while True:
            conn, addr = sock.accept()
            peer = "%s:%d" % addr
            logging.debug("TCP connection from %s", peer)
            with conn:
                while True:
                    try:
                        data = conn.recv(BUFFER_SIZE)
                    except ConnectionResetError:
                        logging.debug("TCP connection reset by peer %s", peer)
                        break

                    if not data:
                        break

                    # Echo back the exact payload
                    conn.sendall(data)

            logging.debug("TCP connection closed %s", peer)


def udp_echo_loop(host: str, port: int) -> None:
    sock = create_udp_socket(host, port)
    with sock:
        while True:
            data, addr = sock.recvfrom(BUFFER_SIZE)
            if not data:
                continue

            # Echo back the exact payload
            sock.sendto(data, addr)
            logging.debug("UDP datagram %d bytes from %s:%d", len(data), addr[0], addr[1])


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Simple TCP + UDP echo server for ESP32-C3 network benchmarking.",
    )
    parser.add_argument(
        "--host",
        default=HOST_DEFAULT,
        help=f"IP/interface to bind (default: {HOST_DEFAULT})",
    )
    parser.add_argument(
        "--tcp-port",
        type=int,
        default=TCP_PORT_DEFAULT,
        help=f"TCP port to listen on (default: {TCP_PORT_DEFAULT})",
    )
    parser.add_argument(
        "--udp-port",
        type=int,
        default=UDP_PORT_DEFAULT,
        help=f"UDP port to listen on (default: {UDP_PORT_DEFAULT})",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Enable verbose (DEBUG) logging.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    setup_logging(verbose=args.verbose)

    logging.info(
        "Starting echo server (TCP:%d, UDP:%d, host=%s)",
        args.tcp_port,
        args.udp_port,
        args.host,
    )

    tcp_thread = threading.Thread(
        target=tcp_echo_loop,
        args=(args.host, args.tcp_port),
        daemon=True,
        name="tcp-listener",
    )
    udp_thread = threading.Thread(
        target=udp_echo_loop,
        args=(args.host, args.udp_port),
        daemon=True,
        name="udp-listener",
    )

    tcp_thread.start()
    udp_thread.start()

    try:
        # Keep the main thread alive so Ctrl+C works
        while True:
            time.sleep(1.0)
    except KeyboardInterrupt:
        logging.info("Shutting down echo server (Ctrl+C pressed)")


if __name__ == "__main__":
    main()

