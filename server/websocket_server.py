import os
import time
from flask import Flask, send_from_directory
from flask_socketio import SocketIO

_WEB_DIR = os.path.join(os.path.dirname(__file__), "..", "web")

app = Flask(__name__, static_folder=None)
app.config["SECRET_KEY"] = "motion-tracker-secret-key"

socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")


@app.route("/")
def index():
    return send_from_directory(_WEB_DIR, "index.html")


@app.route("/<path:filename>")
def static_files(filename):
    return send_from_directory(_WEB_DIR, filename)


def broadcast_loop(get_data_fn, hz: int = 20):
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
