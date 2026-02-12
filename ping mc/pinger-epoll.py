import ipaddress
import json
import socket
import select
import errno
import time
import threading
import random
import argparse
import queue
from collections import deque
from mcstatus import JavaServer

PORT = 25565
BATCH_SIZE = 10240
MAX_PARALLEL = 16384 # max open sockets registered in epoll
OUTPUT_FILE = "minecraft_server_status.ndjson"
STATS_WINDOW = 30
CONNECT_TIMEOUT = 0.6

write_lock = threading.Lock()
existing_data = set()
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


def query_minecraft(ip_str):
    """Use mcstatus to query server details (blocking)."""
    try:
        server = JavaServer(ip_str, PORT)
        status = server.status()
        players_sample = []
        sample = getattr(status.players, "sample", None)
        if not sample:
            raw = getattr(status, "raw", None)
            if isinstance(raw, dict):
                sample = raw.get("players", {}).get("sample", [])
        if sample:
            for p in sample:
                if isinstance(p, dict):
                    name = p.get("name")
                else:
                    name = getattr(p, "name", None)
                if name:
                    players_sample.append(name)
        return {
            "online": True,
            "motd": status.motd.to_plain(),
            "players_online": status.players.online,
            "players_max": status.players.max,
            "version": status.version.name,
            "latency_ms": round(status.latency, 2),
            "players_sample": players_sample,
        }
    except Exception:
        return None


def epoll_scan(batch):
    """Use epoll to test TCP connections concurrently."""
    global ping_attempts
    ep = select.epoll()
    fd_to_ip = {}
    start_time = {}
    results = []

    # Create and register sockets
    for ip in batch:
        ip_str = str(ip)
        if ip_str in existing_data:
            continue
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM | socket.SOCK_NONBLOCK)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, b"wlp195s0f3u1")

            err = s.connect_ex((ip_str, PORT))
            fd = s.fileno()
            ep.register(fd, select.EPOLLOUT)
            fd_to_ip[fd] = (s, ip_str)
            start_time[fd] = time.time()
            with ping_lock:
                ping_attempts += 1
        except Exception:
            continue

        # Limit active sockets to MAX_PARALLEL
        if len(fd_to_ip) >= MAX_PARALLEL:
            handle_events(ep, fd_to_ip, start_time, results)

    # Process remaining sockets
    while fd_to_ip:
        handle_events(ep, fd_to_ip, start_time, results)

    ep.close()
    return results


def handle_events(ep, fd_to_ip, start_time, results):
    """Poll for connect completion and handle them."""
    try:
        events = ep.poll(0.1)
    except InterruptedError:
        return

    now = time.time()
    for fd, ev in events:
        s, ip_str = fd_to_ip.pop(fd, (None, None))
        if not s:
            continue
        try:
            ep.unregister(fd)
        except Exception:
            pass

        if now - start_time.get(fd, 0) > CONNECT_TIMEOUT:
            s.close()
            continue

        try:
            err = s.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
            s.close()
            if err == 0:
                results.append(ip_str)
        except Exception:
            s.close()

    # Drop expired sockets
    expired = [fd for fd, t in start_time.items() if now - t > CONNECT_TIMEOUT]
    for fd in expired:
        if fd in fd_to_ip:
            s, _ = fd_to_ip.pop(fd)
            try:
                ep.unregister(fd)
            except Exception:
                pass
            s.close()


def main():
    parser = argparse.ArgumentParser(description="Scan IP range for Minecraft servers using epoll.")
    parser.add_argument("start_ip")
    parser.add_argument("end_ip")
    args = parser.parse_args()

    start_ip = int(ipaddress.IPv4Address(args.start_ip))
    end_ip = int(ipaddress.IPv4Address(args.end_ip))
    if start_ip > end_ip:
        print("Error: start_ip must be <= end_ip")
        return

    load_existing_data()

    running_event.set()
    threading.Thread(target=stats_thread, daemon=True).start()
    writer = threading.Thread(target=writer_thread, daemon=True)
    writer.start()

    ip_blocks = [
        (i, min(i + BATCH_SIZE - 1, end_ip))
        for i in range(start_ip, end_ip + 1, BATCH_SIZE)
    ]
    random.shuffle(ip_blocks)

    for block_start, block_end in ip_blocks:
        batch = [
            ipaddress.IPv4Address(i)
            for i in range(block_start, block_end + 1)
            if not ipaddress.IPv4Address(i).is_private
            and not ipaddress.IPv4Address(i).is_loopback
            and not ipaddress.IPv4Address(i).is_link_local
            and not ipaddress.IPv4Address(i).is_multicast
            and str(ipaddress.IPv4Address(i)) not in existing_data
        ]
        if not batch:
            continue

        alive_ips = epoll_scan(batch)
        for ip_str in alive_ips:
            result = query_minecraft(ip_str)
            if result:
                result_queue.put((ip_str, result))

    result_queue.join()
    result_queue.put(None)
    writer.join()
    running_event.clear()
    print("Done. Results written to", OUTPUT_FILE)


if __name__ == "__main__":
    main()
