import json
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading
from mcstatus import JavaServer
import time
from itertools import islice
import socket

BATCH_SIZE = 512
SLEEP_BETWEEN_BATCHES = 1

INPUT_FILE = "minecraft_server_status.ndjson"
OUTPUT_FILE = "minecraft_server_status_updated.ndjson"
MAX_THREADS = 1024
PORT = 25565

write_lock = threading.Lock()

def ping_server(entry):
    ip = entry["ip"]
    try:
        hostname = socket.getnameinfo((ip, 0), 0)
    except:
        hostname = ""
    try:
        server = JavaServer(ip, PORT)
        status = server.status()

        result = {
            "ip": ip,
            "hostname": hostname[0],
            "status": {
                "online": True,
                "motd": status.motd.to_plain(),
                "players_online": status.players.online,
                "players_max": status.players.max,
                "version": status.version.name,
                "latency_ms": round(status.latency, 2)
            }
        }

        players_sample = []
        try:
            sample = None
            if hasattr(status, "players") and hasattr(status.players, "sample"):
                sample = status.players.sample

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

        result["status"]["players_sample"] = players_sample
    except Exception:
        result = {
            "ip": ip,
            "status": {
                "online": False
            }
        }
    return result

def batched(iterable, batch_size):
    it = iter(iterable)
    while True:
        batch = list(islice(it, batch_size))
        if not batch:
            break
        yield batch

def main():
    entries = []

    try:
        with open(INPUT_FILE, "r", encoding="utf-8") as f:
            for line in f:
                try:
                    entry = json.loads(line.strip())
                    entries.append(entry)
                except json.JSONDecodeError:
                    continue
    except FileNotFoundError:
        print(f"Input file {INPUT_FILE} not found.")
        return

    updated_entries = []

    for batch_num, batch in enumerate(batched(entries, BATCH_SIZE), 1):
        print(f"Pinging batch {batch_num} with {len(batch)} servers...")
        with ThreadPoolExecutor(max_workers=min(MAX_THREADS, len(batch))) as executor:
            futures = [executor.submit(ping_server, entry) for entry in batch]

            for future in as_completed(futures):
                result = future.result()
                if result:
                    updated_entries.append(result)

        time.sleep(SLEEP_BETWEEN_BATCHES)

    updated_entries.sort(
        key=lambda x: x["status"].get("players_online", 0),
        reverse=True
    )

    with open(OUTPUT_FILE, "w", encoding="utf-8") as out_file:
        for entry in updated_entries:
            json.dump(entry, out_file)
            out_file.write("\n")

    print(f"Updated and sorted status written to {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
