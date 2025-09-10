import ipaddress
import json
import socket
import threading
import random
import queue
import argparse
from mcstatus import JavaServer
from concurrent.futures import ThreadPoolExecutor, as_completed
import os

# CONFIGURATION
PORT = 25565
BATCH_SIZE = 2048
MAX_THREADS = 1024
OUTPUT_FILE = "minecraft_server_status.ndjson"

# Thread-safe write lock
write_lock = threading.Lock()
existing_data = {}  # Store already found IPs
result_queue = queue.Queue()

def writer_thread():
    with open(OUTPUT_FILE, "a", encoding="utf-8") as f:
        while True:
            item = result_queue.get()
            if item is None:  # Sentinel value to exit
                break
            ip_str, result = item
            json.dump({"ip": ip_str, "status": result}, f)
            f.write("\n")
            f.flush()
            result_queue.task_done()

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

    print(f"Pinging {ip_str}...")
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

    # Start the writer thread
    writer = threading.Thread(target=writer_thread)
    writer.start()

    # Create a list of (start, end) block tuples
    blocks = []
    for block_start in range(start_ip, end_ip + 1, BATCH_SIZE):
        block_end = min(block_start + BATCH_SIZE - 1, end_ip)
        blocks.append((block_start, block_end))

    # Randomize the order of blocks
    random.shuffle(blocks)

    for block_start, block_end in blocks:
        current_batch = []

        for ip_int in range(block_start, block_end + 1):
            ip = ipaddress.IPv4Address(ip_int)

            # Skip undesirable IPs
            if ip.is_private or ip.is_loopback or ip.is_link_local or ip.is_multicast or ip.is_reserved:
                continue

            if str(ip) in existing_data:
                continue

            current_batch.append(ip)

        if current_batch:
            process_batch(current_batch)

    # Wait for all queued results to be written
    result_queue.join()
    # Stop the writer thread
    result_queue.put(None)
    writer.join()

    print("Done. Results written to", OUTPUT_FILE)

if __name__ == "__main__":
    main()
