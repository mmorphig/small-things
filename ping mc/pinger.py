import ipaddress
import json
import socket
import select
import errno
import time
import threading
import random
import queue
import argparse
from collections import deque
from mcstatus import JavaServer
from concurrent.futures import ThreadPoolExecutor, as_completed

PORT = 25565
BATCH_SIZE = 4096
MAX_THREADS = 1024
OUTPUT_FILE = "minecraft_server_status.ndjson"
STATS_WINDOW = 30 # seconds to average pings/sec over

write_lock = threading.Lock()
existing_data = {}
result_queue = queue.Queue()
ping_attempts = 0
ping_lock = threading.Lock()
running_event = threading.Event()

def writer_thread():
    with open(OUTPUT_FILE, "a", encoding="utf-8") as f:
        while True:
            item = result_queue.get()
            if item is None:
                break
            ip_str, result = item
            json.dump({"ip": ip_str, "status": result}, f)
            f.write("\n")
            f.flush()
            result_queue.task_done()


def stats_thread():
    last = 0
    window = deque(maxlen=STATS_WINDOW)
    while running_event.is_set():
        time.sleep(1)
        with ping_lock:
            cur = ping_attempts
        pps = cur - last
        last = cur
        window.append(pps)
        avg = sum(window) / len(window) if window else 0
        print(f"Pings/sec: {pps} (avg {avg:.1f} over {len(window)}s)")

def load_existing_data():
    global existing_data
    existing_data = set()
    try:
        with open(OUTPUT_FILE, "r", encoding="utf-8") as f:
            for line in f:
                try:
                    entry = json.loads(line)
                    existing_data.add(entry["ip"])
                except json.JSONDecodeError:
                    continue
    except FileNotFoundError:
        pass

def ping_server(ip):
    ip_str = str(ip)
    if ip_str in existing_data:
        return None
    global ping_attempts

    try:
        with ping_lock:
            ping_attempts += 1

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, b"wlp1s0")

        try:
            sock.setblocking(False)
            err = sock.connect_ex((ip_str, PORT))
            if err not in (0, errno.EINPROGRESS, errno.EWOULDBLOCK):
                sock.close()
                return None

            r, w, x = select.select([], [sock], [], 0.6)
            if not w:
                sock.close()
                return None

            err = sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
            if err != 0:
                sock.close()
                return None
        finally:
            try:
                sock.close()
            except Exception:
                pass
    except Exception:
        return None

    try:
        server = JavaServer(ip_str, PORT)
        status = server.status()

        result = {
            "online": True,
            "motd": status.motd.to_plain(),
            "players_online": status.players.online,
            "players_max": status.players.max,
            "version": status.version.name,
            "latency_ms": round(status.latency, 2)
        }

        players_sample = []
        try:
            sample = None
            if hasattr(status, "players") and hasattr(status.players, "sample"):
                sample = status.players.sample

            # Fallback to raw if available
            if not sample:
                raw = getattr(status, "raw", None)
                if isinstance(raw, dict):
                    sample = raw.get("players", {}).get("sample")

            if sample:
                for p in sample:
                    if p is None:
                        continue
                    if isinstance(p, dict):
                        name = p.get("name")
                    else:
                        name = getattr(p, "name", None) or getattr(p, "display_name", None)
                    if name:
                        players_sample.append(name)
        except Exception:
            players_sample = []

        result["players_sample"] = players_sample

        return ip_str, result

    except Exception:
        return None

def write_result(ip_str, result):
    with write_lock:
        with open(OUTPUT_FILE, "a", encoding="utf-8") as f:
            json.dump({"ip": ip_str, "status": result}, f)
            f.write("\n")

def process_batch(batch):
    with ThreadPoolExecutor(max_workers=MAX_THREADS) as executor:
        futures = [executor.submit(ping_server, ip) for ip in batch]

        for future in as_completed(futures):
            result = future.result()
            if result:
                result_queue.put(result)

def main():
    parser = argparse.ArgumentParser(description="Scan IP range for Minecraft servers.")
    parser.add_argument("start_ip", help="Start IP address (inclusive)")
    parser.add_argument("end_ip", help="End IP address (inclusive)")
    args = parser.parse_args()

    try:
        start_ip = int(ipaddress.IPv4Address(args.start_ip))
        end_ip = int(ipaddress.IPv4Address(args.end_ip))
    except ipaddress.AddressValueError as e:
        print(f"Invalid IP address: {e}")
        return

    if start_ip > end_ip:
        print("Error: start_ip must be less than or equal to end_ip.")
        return

    load_existing_data()

    running_event.set()
    stats = threading.Thread(target=stats_thread)
    stats.start()

    writer = threading.Thread(target=writer_thread)
    writer.start()

    blocks = []
    for block_start in range(start_ip, end_ip + 1, BATCH_SIZE):
        block_end = min(block_start + BATCH_SIZE - 1, end_ip)
        blocks.append((block_start, block_end))

    random.shuffle(blocks)

    for block_start, block_end in blocks:
        current_batch = []

        for ip_int in range(block_start, block_end + 1):
            ip = ipaddress.IPv4Address(ip_int)

            if ip.is_private or ip.is_loopback or ip.is_link_local or ip.is_multicast or ip.is_reserved:
                continue

            if str(ip) in existing_data:
                continue

            current_batch.append(ip)

        if current_batch:
            #print(f"Pinging block {ipaddress.IPv4Address(block_start)} - {ipaddress.IPv4Address(block_end)} with {len(current_batch)} servers...")
            process_batch(current_batch)

    result_queue.join()

    result_queue.put(None)
    writer.join()

    running_event.clear()
    stats.join()

    print("Done. Results written to", OUTPUT_FILE)

if __name__ == "__main__":
    main()
