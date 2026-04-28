import socket
import threading
import json
import time

from websocket_server import app, socketio, broadcast_loop
from fusion import SensorFusion

_lock = threading.Lock()

_state = {
    "mpu": {
        "node":      "mpu",
        "timestamp": 0,
        "ax":        0.0,
        "ay":        0.0,
        "az":        9.81,
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


def _handle_client(conn: socket.socket, addr, node_key: str):
    print(f"[TCP] {node_key} connected from {addr}")
    try:
        reader = conn.makefile("r", buffering=1)
        for raw_line in reader:
            line = raw_line.strip()
            if not line:
                continue
            try:
                data = json.loads(line)
            except json.JSONDecodeError:
                continue

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
        if node_key == "mpu":
            _fusion.reset()


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


def _get_fused_data() -> dict:
    with _lock:
        mpu   = dict(_state["mpu"])
        ultra = dict(_state["ultrasonic"])

    now        = time.time()
    mpu_stale  = (now - mpu["last_seen"])   > 2.0
    ultra_stale = (now - ultra["last_seen"]) > 2.0

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


if __name__ == "__main__":
    threading.Thread(
        target=_tcp_listener, args=(9001, "mpu"),
        daemon=True, name="tcp-listener-mpu"
    ).start()

    threading.Thread(
        target=_tcp_listener, args=(9002, "ultrasonic"),
        daemon=True, name="tcp-listener-ultrasonic"
    ).start()

    threading.Thread(
        target=broadcast_loop, args=(_get_fused_data, 20),
        daemon=True, name="ws-broadcast"
    ).start()

    print("[SYS] Server running — open http://localhost:5000 in your browser")
    socketio.run(app, host="0.0.0.0", port=5001, debug=False, use_reloader=False, allow_unsafe_werkzeug=True)
