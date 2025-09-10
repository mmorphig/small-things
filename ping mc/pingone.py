import json
import argparse
from mcstatus import JavaServer
import threading
import os

# CONFIGURATION
PORT = 25565
OUTPUT_FILE = "minecraft_server_status_single.ndjson"
write_lock = threading.Lock()

def load_existing_data():
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
    return existing_data

def ping_server(ip, existing_data):
    if ip in existing_data:
        print(f"{ip} already checked, skipping.")
        return None

    print(f"Pinging {ip}...")
    try:
        server = JavaServer(ip, PORT)
        status = server.status()

        result = {
            "online": True,
            "motd": status.motd.to_plain(),
            "players_online": status.players.online,
            "players_max": status.players.max,
            "version": status.version.name,
            "latency_ms": round(status.latency, 2)
        }

        return ip, result

    except Exception:
        print(f"Failed to ping {ip}")
        return None

def write_result(ip_str, result):
    with write_lock:
        with open(OUTPUT_FILE, "a", encoding="utf-8") as f:
            json.dump({"ip": ip_str, "status": result}, f)
            f.write("\n")

def main():
    parser = argparse.ArgumentParser(description="Ping a single Minecraft server.")
    parser.add_argument("ip", help="IP address to ping")
    args = parser.parse_args()

    existing_data = load_existing_data()
    result = ping_server(args.ip, existing_data)

    if result:
        ip_str, status = result
        write_result(ip_str, status)
        print("Ping result written to", OUTPUT_FILE)
    else:
        print("No result to write.")

if __name__ == "__main__":
    main()
