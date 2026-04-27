"""
server.py — Main entry point for the Distributed Motion Tracker server.

Responsibilities:
  1. Accept TCP connections from Node A (port 9000) and Node B (port 9001)
  2. Parse newline-delimited JSON packets from each node
  3. Maintain a thread-safe shared sensor state dict
  4. Run sensor fusion and build the motion trail
  5. Broadcast fused data to the browser at 20 Hz via WebSocket
  6. Serve the web dashboard via Flask
"""

import socket
import threading
import json
import time

from websocket_server import app, socketio, broadcast_loop
from fusion import SensorFusion

# ------------------------------------------------------------------
# Shared state — protected by a single lock
# ------------------------------------------------------------------

_lock = threading.Lock()

_state = {
    "mpu": {
        "node":      "mpu",
        "timestamp": 0,
        "ax":        0.0,
        "ay":        0.0,
        "az":        9.81,   # gravity present until first real packet
        "roll":      0.0,
        "pitch":     0.0,
        "yaw":       0.0,
        "velocity":  0.0,
        "accel_mag": 9.81,
        "state":     "IDLE",
        "last_seen": 0.0,
    },
    "ultrasonic": {
        "node":              "ultrasonic",
        "distance":          100.0,
        "relative_distance": 100.0,
        "speed":             0.0,
        "last_seen":         0.0,
    },
}

_fusion = SensorFusion(trail_size=100)

# ------------------------------------------------------------------
# TCP client handler — one thread per connected ESP32
# ------------------------------------------------------------------

def _handle_client(conn: socket.socket, addr, node_key: str):
    print(f"[TCP] {node_key} connected from {addr}")
    try:
        # makefile() gives us a line-buffered text interface;
        # each JSON packet from the ESP32 is terminated with '\n'.
        reader = conn.makefile("r", buffering=1)
        for raw_line in reader:
            line = raw_line.strip()
            if not line:
                continue
            try:
                data = json.loads(line)
            except json.JSONDecodeError:
                # Ignore malformed / partial packets
                continue

            # Determine which node this packet belongs to
            key = data.get("node", node_key)
            if key not in _state:
                key = node_key

            with _lock:
                _state[key].update(data)
                _state[key]["last_seen"] = time.time()

    except Exception as exc:
        print(f"[TCP] {node_key} handler error: {exc}")
    finally:
        conn.close()
        print(f"[TCP] {node_key} disconnected ({addr})")
        # Reset integrator so position starts from 0 on next connect
        if node_key == "mpu":
            _fusion.reset()


# ------------------------------------------------------------------
# TCP listener — blocks waiting for connections on a single port
# ------------------------------------------------------------------

def _tcp_listener(port: int, node_key: str):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen(5)
    print(f"[TCP] Listening on :{port}  ({node_key})")
    while True:
        try:
            conn, addr = srv.accept()
            t = threading.Thread(
                target=_handle_client,
                args=(conn, addr, node_key),
                daemon=True,
                name=f"tcp-{node_key}",
            )
            t.start()
        except Exception as exc:
            print(f"[TCP] Listener error on :{port}: {exc}")


# ------------------------------------------------------------------
# Data accessor — called by the broadcast loop at 20 Hz
# ------------------------------------------------------------------

def _get_fused_data() -> dict:
    with _lock:
        mpu   = dict(_state["mpu"])
        ultra = dict(_state["ultrasonic"])

    now        = time.time()
    mpu_stale  = (now - mpu["last_seen"])   > 2.0
    ultra_stale = (now - ultra["last_seen"]) > 2.0

    # Freeze position when the sensor is stale — avoids the cube jumping
    # to a drifted value the moment the ESP32 reconnects.
    if not mpu_stale:
        position = _fusion.fuse(mpu)
    else:
        position = _fusion.get_last_pos()
    trail = _fusion.get_trail()

    return {
        "mpu":         mpu,
        "position":    position,
        "trail":       trail,
        "mpu_stale":   mpu_stale,
        "ultra":       ultra,
        "ultra_stale": ultra_stale,
    }


# ------------------------------------------------------------------
# Entry point
# ------------------------------------------------------------------

if __name__ == "__main__":
    # TCP listener thread (daemon — auto-exit when main thread ends)
    threading.Thread(
        target=_tcp_listener, args=(9001, "mpu"),
        daemon=True, name="tcp-listener-mpu"
    ).start()

    threading.Thread(
        target=_tcp_listener, args=(9002, "ultrasonic"),
        daemon=True, name="tcp-listener-ultrasonic"
    ).start()

    # WebSocket broadcast thread
    threading.Thread(
        target=broadcast_loop, args=(_get_fused_data, 20),
        daemon=True, name="ws-broadcast"
    ).start()

    print("[SYS] Server running — open http://localhost:5000 in your browser")
    socketio.run(app, host="0.0.0.0", port=5001, debug=False, use_reloader=False, allow_unsafe_werkzeug=True)
