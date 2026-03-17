from __future__ import annotations

import argparse
import time


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Export ESP32 benchmark CSV to a file via serial.")
    p.add_argument("--port", default="COM10", help="Serial port (e.g. COM10)")
    p.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    p.add_argument("--count", type=int, default=1000, help="Benchmark count to send (default: 1000)")
    p.add_argument("--out", default="bench_results.csv", help="Output CSV file path")
    p.add_argument("--timeout-sec", type=int, default=900, help="Max seconds to wait for results")
    return p.parse_args()


def main() -> int:
    args = parse_args()

    try:
        import serial  # type: ignore
    except Exception:
        print("Missing dependency: pyserial. Install with: pip install pyserial")
        return 2

    start_marker = "--- CSV"
    end_marker = "--- end CSV ---"
    choice_prompt = "Enter choice"
    count_prompt = "Enter count"

    print(f"Opening {args.port} @ {args.baud} ...")
    print("Make sure your serial monitor is CLOSED.")

    with serial.Serial(args.port, args.baud, timeout=0.5) as ser:
        ser.reset_input_buffer()
        time.sleep(0.2)

        print("Waiting for menu prompt, then sending option 7 and count...")
        deadline = time.time() + args.timeout_sec
        csv_lines: list[str] = []
        phase = 0

        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                continue  # serial timeout, keep waiting

            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            if phase == 0:
                if "Enter choice" in line:
                    # send choice WITHOUT newline to avoid read_count() consuming CR/LF and reading 0
                    ser.write(b"7")
                    ser.flush()
                    print("Sent menu choice: 7")
                    phase = 1
                continue

            if phase == 1:
                if "Enter count" in line:
                    ser.write(f"{args.count}\r\n".encode("ascii"))
                    ser.flush()
                    print(f"Sent count: {args.count}")
                    phase = 2
                continue

            if phase == 2:
                if "--- CSV" in line:
                    print("CSV start detected; capturing...")
                    phase = 3
                continue

            # phase == 3: capturing CSV lines
            if "--- end CSV ---" in line:
                print("CSV end detected.")
                break
            csv_lines.append(line)

        if not csv_lines:
            print("Did not capture CSV. Check device is running and markers exist.")
            print("Tips:")
            print("- Make sure firmware prints the CSV markers (--- CSV ... --- / --- end CSV ---).")
            print("- Make sure the device is at the menu (it must print 'Enter choice:').")
            return 1

        with open(args.out, "w", newline="") as f:
            for ln in csv_lines:
                f.write(ln + "\n")

        print(f"Wrote {len(csv_lines)} lines to {args.out}")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())

