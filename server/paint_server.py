"""
paint_server.py  —  Standalone server for the MPU Paint app.

Flow:
  ESP32  ──TCP:9000──►  this script  ──WebSocket──►  paint.html

Receives {"node":"mpu","roll":X,"pitch":Y} from the ESP32.
Emits    {"x": roll, "y": pitch}  to the browser at 20 Hz.

No physics, no fusion, no trail.  Roll IS x.  Pitch IS y.
"""

import socket
import threading
import json
import time
import os

from flask import Flask, send_from_directory
from flask_socketio import SocketIO

# ── Flask / SocketIO ──────────────────────────────────────────────────────────

WEB_DIR = os.path.join(os.path.dirname(__file__), "..", "web")

app      = Flask(__name__, static_folder=None)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")

@app.route("/")
def index():
    return send_from_directory(WEB_DIR, "paint.html")

@app.route("/<path:f>")
def static_files(f):
    return send_from_directory(WEB_DIR, f)

# ── Shared sensor state ───────────────────────────────────────────────────────

_lock  = threading.Lock()
_state = {"roll": 0.0, "pitch": 0.0, "alive": False}

# ── TCP listener  (ESP32 connects here) ──────────────────────────────────────

def tcp_listener(port=9000):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen(5)
    print(f"[TCP] Listening on :{port}")

    while True:
        conn, addr = srv.accept()
        print(f"[TCP] Connected: {addr}")
        threading.Thread(target=handle_client, args=(conn, addr),
                         daemon=True).start()

def handle_client(conn, addr):
    with _lock:
        _state["alive"] = True
    try:
        reader = conn.makefile("r", buffering=1)
        for line in reader:
            line = line.strip()
            if not line:
                continue
            try:
                data = json.loads(line)
            except json.JSONDecodeError:
                continue
            with _lock:
                _state["roll"]  = float(data.get("roll",  0.0))
                _state["pitch"] = float(data.get("pitch", 0.0))
    except Exception as e:
        print(f"[TCP] Client error: {e}")
    finally:
        conn.close()
        with _lock:
            _state["alive"] = False
        print(f"[TCP] Disconnected: {addr}")

# ── Broadcast loop  (20 Hz → browser) ────────────────────────────────────────

def broadcast_loop(hz=20):
    interval = 1.0 / hz
    while True:
        t0 = time.time()
        with _lock:
            payload = {
                "x":     round(_state["roll"],  4),
                "y":     round(_state["pitch"], 4),
                "alive": _state["alive"],
            }
        socketio.emit("paint_update", payload)
        time.sleep(max(0.0, interval - (time.time() - t0)))

# ── Entry point ───────────────────────────────────────────────────────────────

if __name__ == "__main__":
    threading.Thread(target=tcp_listener, daemon=True).start()
    threading.Thread(target=broadcast_loop, daemon=True).start()
    print("[SYS] Paint server  →  http://localhost:5002/")
    socketio.run(app, host="0.0.0.0", port=5002, debug=False,
                 use_reloader=False, allow_unsafe_werkzeug=True)
