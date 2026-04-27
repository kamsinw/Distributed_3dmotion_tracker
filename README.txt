================================================================================
EE 250 PROJECT — README
================================================================================

TEAM MEMBER NAMES
--------------------------------------------------------------------------------
  • Kamsi Nwabueze
  • Kamron Rousseau


================================================================================
HOW TO COMPILE AND RUN
================================================================================

PREREQUISITES
--------------------------------------------------------------------------------
  • Python 3.9+ with pip
  • PlatformIO (CLI) for ESP32 firmware — if `pio` is not on your PATH,
    use the full path to your PlatformIO binary (example used on this project):
      /Users/kamsinw/Library/Python/3.9/bin/pio

  • WiFi: edit each node’s include/config.h for WIFI_SSID, WIFI_PASS, and
    SERVER_IP (your laptop’s LAN IP, e.g. from: ipconfig getifaddr en0)


PYTHON — INSTALL DEPENDENCIES (once)
--------------------------------------------------------------------------------
  cd <path-to-repo>
  pip3 install -r requirements.txt


PYTHON — 3D MOTION TRACKER (block + trail + ultrasonic camera zoom)
--------------------------------------------------------------------------------
  • TCP: MPU ESP32 → port 9001 ; Ultrasonic ESP32 → port 9002
  • Web: http://localhost:5001

  cd <path-to-repo>
  python3 server/server.py

 


PYTHON — MPU PAINT (canvas + ultrasonic pen lift / swipe undo)
--------------------------------------------------------------------------------
  • TCP: Paint ESP32 → port 9000 ; Ultrasonic ESP32 → port 9002
  • Web: http://localhost:5002

  cd <path-to-repo>
  python3 server/paint_server.py

  Do not run paint_server.py and server.py at the same time if both need the
  same ultrasonic board on port 9002 — use one server at a time, or assign
  different TCP ports in firmware and server code.


ESP32 — COMPILE AND UPLOAD (PlatformIO)
--------------------------------------------------------------------------------
  MPU node (3D tracker, TCP 9001):
    cd <path-to-repo>/mpu_node
    pio run --target upload

  Paint node (paint app, TCP 9000):
    cd <path-to-repo>/paint_node
    pio run --target upload

  Ultrasonic node (TCP 9002 — paint OR block server, not both at once):
    cd <path-to-repo>/ultrasonic_node
    pio run --target upload

  Serial monitor (optional, 115200 baud):
    cd <path-to-repo>/<node_folder>
    pio device monitor --baud 115200


WEB BROWSER
--------------------------------------------------------------------------------
  Open the URLs above after the matching Python server is running.
  The paint and dashboard pages load Socket.IO and Three.js from CDNs
  (see web/*.html script tags).


================================================================================
EXTERNAL LIBRARIES AND DEPENDENCIES
================================================================================

PYTHON (requirements.txt)
--------------------------------------------------------------------------------
  • flask
  • flask-socketio
  • simple-websocket  (transitive dependency for Socket.IO stack)

  Transitive packages (installed automatically with the above) include
  e.g. python-socketio, python-engineio, werkzeug, jinja2 — see pip output.


ESP32 / PLATFORMIO (platformio.ini per node)
--------------------------------------------------------------------------------
  • espressif32 platform (Arduino framework)
  • WiFi (built-in)


BROWSER (loaded from CDN in HTML; not installed via pip)
--------------------------------------------------------------------------------
  • Socket.IO client (e.g. 4.7.2) — paint and main dashboard
  • three.js (r128) — 3D block visualization (index.html)


================================================================================
END OF README
================================================================================
