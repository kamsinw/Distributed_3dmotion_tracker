"""
websocket_server.py — Flask app + Flask-SocketIO setup.

Serves the web dashboard as static files and provides the
WebSocket endpoint that the browser subscribes to.
"""

import os
import time
from flask import Flask, send_from_directory
from flask_socketio import SocketIO

# Absolute path to the web/ directory
_WEB_DIR = os.path.join(os.path.dirname(__file__), "..", "web")

app = Flask(__name__, static_folder=None)
app.config["SECRET_KEY"] = "motion-tracker-secret-key"

# async_mode='threading' works without eventlet / gevent.
# Background threads can call socketio.emit() safely.
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")


# ------------------------------------------------------------------
# Static file routes — serve index.html and companion JS files
# ------------------------------------------------------------------

@app.route("/")
def index():
    return send_from_directory(_WEB_DIR, "index.html")


@app.route("/<path:filename>")
def static_files(filename):
    return send_from_directory(_WEB_DIR, filename)


# ------------------------------------------------------------------
# Background broadcast loop
# ------------------------------------------------------------------

def broadcast_loop(get_data_fn, hz: int = 20):
    """
    Continuously call get_data_fn() and emit a 'sensor_update' event
    to all connected browsers at the requested rate.

    Runs in a daemon thread started by server.py.
    """
    interval = 1.0 / hz
    while True:
        t0 = time.time()
        try:
            data = get_data_fn()
            socketio.emit("sensor_update", data)
        except Exception as exc:
            print(f"[WS] broadcast error: {exc}")
        elapsed = time.time() - t0
        sleep_s = max(0.0, interval - elapsed)
        time.sleep(sleep_s)
