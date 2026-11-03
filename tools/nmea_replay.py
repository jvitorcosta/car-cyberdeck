"""Stand-in for the iPhone's GPS 2 IP app on the bench: a TCP server that sends an NMEA file to the
black deck one sentence per second, the way the phone pushes $GPRMC at about 1 Hz.

    python tools/nmea_replay.py --file tools/testdata/drive_cam_test.nmea [--port 11123] [--loop]

Point the deck at the PC with GPS_HOST (and GPS_PORT if not 11123) in src/config.h, flash, and
watch `[cam]` lines in the serial log or GET /cam. Each client gets the file from the start; a
client that goes away is simply dropped (the deck reconnects on its own). Every line that starts
with '$' is sent, CRLF-terminated; one line per second for RMC sentences, other talkers' lines go
out with the next RMC (as a receiver groups a second's sentences). Standard library only.
"""
import argparse
import socket
import sys
import threading
import time


def load(path):
    with open(path, "r", encoding="ascii") as f:
        lines = [ln.strip() for ln in f]
    lines = [ln for ln in lines if ln.startswith("$")]
    if not lines:
        sys.exit(f"{path}: no NMEA sentences")
    # Group: each RMC closes one second's batch.
    batches, cur = [], []
    for ln in lines:
        cur.append(ln)
        if ln[3:6] == "RMC":
            batches.append(cur)
            cur = []
    if cur:
        batches.append(cur)
    return batches


def serve(conn, addr, batches, loop, interval):
    print(f"[replay] {addr[0]}:{addr[1]} connected")
    try:
        while True:
            for i, batch in enumerate(batches):
                conn.sendall("".join(s + "\r\n" for s in batch).encode("ascii"))
                print(f"[replay] {addr[0]} {i + 1}/{len(batches)} {batch[-1]}")
                time.sleep(interval)
            if not loop:
                break
        print(f"[replay] {addr[0]} end of file")
    except OSError as e:
        print(f"[replay] {addr[0]} gone ({e})")
    finally:
        conn.close()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--file", required=True, help="NMEA file (e.g. tools/testdata/drive_cam_test.nmea)")
    ap.add_argument("--port", type=int, default=11123, help="TCP port (GPS 2 IP's default: 11123)")
    ap.add_argument("--host", default="0.0.0.0", help="address to listen on (default: all)")
    ap.add_argument("--loop", action="store_true", help="start the file over at its end")
    ap.add_argument("--interval", type=float, default=1.0, help="seconds between sentences (default 1)")
    args = ap.parse_args(argv)
    sys.stdout.reconfigure(line_buffering=True)   # a log line per fix, also when piped to a file
    batches = load(args.file)
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Restart at once on the same port. Not on Windows: there SO_REUSEADDR lets a second server bind
    # the port while the first still answers, and the deck would talk to whichever wins.
    if sys.platform != "win32":
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.host, args.port))
    srv.listen(2)
    print(f"[replay] {len(batches)} fixes from {args.file} on port {args.port}"
          f"{' (looping)' if args.loop else ''}")
    srv.settimeout(1.0)   # wake up now and then, or Ctrl+C waits for a client on Windows
    try:
        while True:
            try:
                conn, addr = srv.accept()
            except socket.timeout:
                continue
            threading.Thread(target=serve, args=(conn, addr, batches, args.loop, args.interval),
                             daemon=True).start()
    except KeyboardInterrupt:
        pass
    finally:
        srv.close()


if __name__ == "__main__":
    main()
