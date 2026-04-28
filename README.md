# Distributed Motion Sensing — Web Visualization & Air-Painting

**USC EE 250 Final Project** · Kamron Rousseau & Kamsi Nwabueze

Two ESP32 nodes stream IMU and ultrasonic sensor data over Wi-Fi to a Python server, which fuses the streams and pushes live updates to the browser via WebSocket at ~20 Hz. There are two demos built on top of the same hardware:

| Demo | What it does |
|------|--------------|
| **3D Motion Tracker** | Live 3D block + motion trail driven by inertial data; ultrasonic sets camera zoom |
| **Air-Paint Canvas** | Tilt the IMU to move a cursor and draw; ultrasonic lifts/resumes the pen and triggers swipe-to-undo |

---

## Architecture

```
ESP32 MPU node  ──(TCP :9001)──┐
ESP32 Paint node──(TCP :9000)──┤  Python Flask/Socket.IO  ──(WS)──▶  Browser
ESP32 Ultrasonic──(TCP :9002)──┘
```

- Each ESP32 sends **newline-delimited JSON** over a plain TCP socket
- The Python server fuses or forwards fields and emits Socket.IO events
- No build step for the front-end — Three.js and Socket.IO loaded from CDN

---

## Hardware

| Node | Sensors | Key pins |
|------|---------|----------|
| MPU node | 2× MPU9250-class IMU (I²C, `0x68` / `0x69`) | SDA=22, SCL=21, INT=GPIO35 |
| Paint node | 1× MPU6050 (I²C, `0x69`) | SDA=22, SCL=21 |
| Ultrasonic node | HC-SR04 | TRIG=GPIO5, ECHO=GPIO18 |

> **ECHO level shift:** HC-SR04 ECHO swings to 5 V; a 1 kΩ / 2 kΩ resistor divider scales it to ~3.3 V before GPIO18.

---

## Stack

**Embedded (C++/Arduino via PlatformIO)**
- Hand-rolled circular buffer, rolling-window feature extraction (mean, std, max), threshold classifier (IDLE / MOVE / SHAKE / DROP)
- Complementary-style orientation (accel `atan2` + gyro integration); Madgwick AHRS stub included in `orientation.cpp` (commented out)
- HC-SR04 ECHO timed via GPIO interrupt (no blocking `pulseIn`)
- Non-blocking TCP reconnect loop

**Server (Python)**
- `flask` + `flask-socketio` for WebSocket broadcast
- Sensor fusion maps IMU tilt to 3D position; 100-point trail
- Stale-detection per node (2 s timeout)

**Browser**
- Three.js (r128) 3D scene — rotating block + motion trail
- Vanilla JS paint canvas — velocity-mode cursor, bias calibration, undo history
- Socket.IO for live updates

---

## Quick Start

### 1 — Python dependencies
```bash
pip3 install -r requirements.txt
```

### 2 — Edit Wi-Fi / IP in each node's `include/config.h`
```c
#define WIFI_SSID  "your_network"
#define WIFI_PASS  "your_password"
#define SERVER_IP  "your.laptop.ip"   // e.g. ipconfig getifaddr en0
```

### 3a — 3D motion tracker
```bash
# Flash ESP32s
cd mpu_node       && pio run --target upload
cd ultrasonic_node && pio run --target upload

# Start server
python3 server/server.py
# Open http://localhost:5001
```

### 3b — Air-paint canvas
```bash
# Flash ESP32s
cd paint_node      && pio run --target upload
cd ultrasonic_node && pio run --target upload

# Start server
python3 server/paint_server.py
# Open http://localhost:5002
```

> Run only one server at a time if sharing a single ultrasonic board (both bind TCP 9002).

---

## Project Structure

```
mpu_node/          ESP32 firmware — dual IMU, orientation, feature extraction, classifier
paint_node/        ESP32 firmware — single IMU, roll/pitch only
ultrasonic_node/   ESP32 firmware — HC-SR04, interrupt echo, sliding-window filter
server/
  server.py        3D tracker backend (ports 9001, 9002 → WS 5001)
  paint_server.py  Paint backend (ports 9000, 9002 → WS 5002)
  fusion.py        Position fusion + trail
  websocket_server.py  Shared Flask/Socket.IO app
web/
  index.html       3D motion dashboard (Three.js)
  paint.html       Air-paint canvas
  main.js          Dashboard logic
  three_scene.js   Three.js scene setup
```

---

## Features Worth Noting

- **Dual IMU on one I²C bus** — two MPU9250-class sensors at distinct addresses by wiring AD0 high/low
- **Interrupt-driven ultrasonic** — echo pulse timed at ISR level, not blocking; baseline auto-set on startup
- **Gesture recognition from ultrasonic** — pen lift (hysteresis threshold), swipe-to-undo (speed threshold)
- **Bias calibration in-browser** — 30-sample resting average subtracted so cursor is still when device is flat
- **Undo history** — canvas snapshots stored per stroke (max 20), restored on swipe or keyboard shortcut

---

## Dependencies

| Layer | Packages |
|-------|----------|
| Python | `flask`, `flask-socketio`, `simple-websocket` |
| ESP32 | espressif32 platform, Arduino framework (PlatformIO) |
| Browser | Three.js r128, Socket.IO 4.7.2 (CDN) |
