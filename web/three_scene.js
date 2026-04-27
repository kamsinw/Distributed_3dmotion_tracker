"use strict";

/**
 * ThreeScene — manages the Three.js 3D visualisation.
 *
 * Public API:
 *   new ThreeScene(containerEl)   — attach renderer to container
 *   scene.update(data)            — accept a sensor_update payload
 */
// ── Tuning constants ──────────────────────────────────────────────────────────
// SCALE: position units from Python → Three.js scene units.
//   Increase if movement looks too small; decrease if it flies off screen.
const SCALE    = 8.0;
// LERP: fraction of gap closed per 60 Hz frame (0.05 = very smooth/laggy,
//   0.20 = responsive with slight smoothing).
const LERP     = 0.12;
// MAX_JUMP: maximum position change per frame in scene units.
//   Prevents the cube teleporting if a stale packet arrives.
const MAX_JUMP = 2.0;

// Camera zoom driven by ultrasonic distance (cm → camera Z scene units).
const CAM_Z_DEFAULT  = 6;
const CAM_Z_MIN      = 3;
const CAM_Z_MAX      = 14;
const ULTRA_DIST_MIN = 10;
const ULTRA_DIST_MAX = 80;

class ThreeScene {
  constructor(container) {
    this._container = container;
    this._maxTrail  = 100;

    // Pre-allocated Float32Array for the trail geometry (avoids GC churn)
    this._trailBuf  = new Float32Array(this._maxTrail * 3);

    // Target position set by update(); cube lerps toward it each frame.
    this._targetPos = { x: 0, y: 0, z: 0 };

    // Calibration offset — subtracted from every incoming position.
    this._calOffset = { x: 0, y: 0, z: 0 };

    // Target camera Z — driven by ultrasonic distance, lerped each frame.
    this._targetCamZ = CAM_Z_DEFAULT;

    this._initRenderer();
    this._initScene();
    this._initCamera();
    this._initLights();
    this._initGrid();
    this._initCube();
    this._initTrail();

    window.addEventListener("resize", () => this._onResize());
    this._animate();
  }

  // ----------------------------------------------------------------
  // Initialisation helpers
  // ----------------------------------------------------------------

  _initRenderer() {
    this._renderer = new THREE.WebGLRenderer({ antialias: true });
    this._renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this._renderer.setClearColor(0x070c18, 1);
    this._renderer.shadowMap.enabled = false;
    this._resize(
      this._container.clientWidth,
      this._container.clientHeight
    );
    this._container.appendChild(this._renderer.domElement);
  }

  _initScene() {
    this._scene = new THREE.Scene();
    this._scene.fog = new THREE.FogExp2(0x070c18, 0.04);
  }

  _initCamera() {
    const w = this._container.clientWidth;
    const h = this._container.clientHeight;
    this._camera = new THREE.PerspectiveCamera(55, w / h, 0.1, 200);
    this._camera.position.set(0, 2.5, 6);
    this._camera.lookAt(0, 0, 0);
    // Target X for smooth camera follow
    this._camTargetX = 0;
  }

  _initLights() {
    // Soft ambient fill
    this._scene.add(new THREE.AmbientLight(0x223355, 0.9));

    // Primary key light
    const key = new THREE.DirectionalLight(0x5599ff, 1.4);
    key.position.set(4, 8, 6);
    this._scene.add(key);

    // Warm rim / back light
    const rim = new THREE.DirectionalLight(0xff6622, 0.45);
    rim.position.set(-6, -2, -4);
    this._scene.add(rim);

    // Blue fill from below
    const fill = new THREE.DirectionalLight(0x0044aa, 0.3);
    fill.position.set(0, -6, 2);
    this._scene.add(fill);
  }

  _initGrid() {
    // Reference plane
    const grid = new THREE.GridHelper(30, 30, 0x0e2040, 0x091528);
    grid.position.y = -2.5;
    this._scene.add(grid);
  }

  _initCube() {
    const geo = new THREE.BoxGeometry(1.4, 1.4, 1.4);

    // Solid faces
    const mat = new THREE.MeshPhongMaterial({
      color:     0x1a5fcc,
      emissive:  0x061228,
      specular:  0x88ccff,
      shininess: 90,
    });
    this._cube = new THREE.Mesh(geo, mat);
    this._scene.add(this._cube);

    // Wireframe overlay for the "breadboard" look
    const wireMat = new THREE.MeshBasicMaterial({
      color:       0x44aaff,
      wireframe:   true,
      transparent: true,
      opacity:     0.22,
    });
    this._cube.add(new THREE.Mesh(geo, wireMat));

    // Axis arrows attached to the cube so they rotate with it
    this._cube.add(new THREE.AxesHelper(1.2));
  }

  _initTrail() {
    const geo = new THREE.BufferGeometry();
    const attr = new THREE.BufferAttribute(this._trailBuf, 3);
    attr.setUsage(THREE.DynamicDrawUsage);
    geo.setAttribute("position", attr);
    geo.setDrawRange(0, 0);

    const mat = new THREE.LineBasicMaterial({
      color:       0x00e5ff,
      transparent: true,
      opacity:     0.65,
    });

    this._trailLine = new THREE.Line(geo, mat);
    this._scene.add(this._trailLine);
  }

  // ----------------------------------------------------------------
  // Public reset — snap cube and camera back to the origin
  // ----------------------------------------------------------------

  resetToCenter() {
    this._targetPos.x = 0;
    this._targetPos.y = 0;
    this._targetPos.z = 0;
    this._cube.position.set(0, 0, 0);
    this._camTargetX  = 0;
    this._targetCamZ  = CAM_Z_DEFAULT;
    this._camera.position.set(0, 2.5, CAM_Z_DEFAULT);
    this._camera.lookAt(0, 0, 0);
  }

  setCalibrationOffset(x, y, z) {
    this._calOffset.x = x;
    this._calOffset.y = y;
    this._calOffset.z = z;
  }

  // ----------------------------------------------------------------
  // Public update — called on every sensor_update WebSocket event
  // ----------------------------------------------------------------

  update(data) {
    const mpu   = data.mpu      || {};
    const pos   = data.position || { x: 0, y: 0, z: 0 };
    const trail = data.trail    || [];

    // --- Cube rotation ---
    // roll  → X (up/down tilt)
    // pitch → Y (left/right turn)
    // yaw   → Z (in/out spin)
    this._cube.rotation.x = _f(mpu.pitch);
    this._cube.rotation.y = _f(mpu.roll);
    this._cube.rotation.z = _f(mpu.yaw);

    // --- Target position (lerp happens in _animate) ---
    // Clamp large jumps to prevent teleporting on reconnect / stale packets.
    const rawX = (_f(pos.x) - this._calOffset.x) * SCALE;
    const rawY = (_f(pos.y) - this._calOffset.y) * SCALE;
    const rawZ = (_f(pos.z) - this._calOffset.z) * SCALE;

    this._targetPos.x = this._clamp(rawX, this._targetPos.x);
    this._targetPos.y = this._clamp(rawY, this._targetPos.y);
    this._targetPos.z = this._clamp(rawZ, this._targetPos.z);

    // --- Camera zoom from ultrasonic distance ---
    const ultra = data.ultra || {};
    const dist  = _f(ultra.distance);
    if (!data.ultra_stale && dist > 0) {
      const t = Math.max(0, Math.min(1,
        (dist - ULTRA_DIST_MIN) / (ULTRA_DIST_MAX - ULTRA_DIST_MIN)
      ));
      this._targetCamZ = CAM_Z_MIN + t * (CAM_Z_MAX - CAM_Z_MIN);
    }

    // --- Motion trail ---
    this._updateTrail(trail);
  }

  // Clamp val to within MAX_JUMP of prev to prevent sudden jumps.
  _clamp(val, prev) {
    const delta = val - prev;
    if (Math.abs(delta) > MAX_JUMP) return prev + Math.sign(delta) * MAX_JUMP;
    return val;
  }

  // ----------------------------------------------------------------
  // Trail geometry update
  // ----------------------------------------------------------------

  _updateTrail(trailData) {
    if (!trailData || trailData.length === 0) return;

    const count = Math.min(trailData.length, this._maxTrail);
    for (let i = 0; i < count; i++) {
      const pt  = trailData[i];
      const idx = i * 3;
      this._trailBuf[idx]     = _f(pt.x) * SCALE;
      this._trailBuf[idx + 1] = _f(pt.y) * SCALE;
      this._trailBuf[idx + 2] = _f(pt.z) * SCALE;
    }

    const geo  = this._trailLine.geometry;
    geo.attributes.position.needsUpdate = true;
    geo.setDrawRange(0, count);
  }

  // ----------------------------------------------------------------
  // Render loop
  // ----------------------------------------------------------------

  _animate() {
    requestAnimationFrame(() => this._animate());

    // Smooth lerp toward target position every frame (independent of WebSocket rate)
    this._cube.position.x += (this._targetPos.x - this._cube.position.x) * LERP;
    this._cube.position.y += (this._targetPos.y - this._cube.position.y) * LERP;
    this._cube.position.z += (this._targetPos.z - this._cube.position.z) * LERP;

    // Camera gently follows cube on X axis
    this._camTargetX += (this._cube.position.x - this._camTargetX) * 0.04;
    this._camera.position.x = this._camTargetX;

    // Camera smoothly zooms in/out based on ultrasonic distance
    this._camera.position.z += (this._targetCamZ - this._camera.position.z) * LERP;

    this._camera.lookAt(this._camTargetX, 0, 0);

    this._renderer.render(this._scene, this._camera);
  }

  // ----------------------------------------------------------------
  // Resize handler
  // ----------------------------------------------------------------

  _resize(w, h) {
    this._renderer.setSize(w, h);
  }

  _onResize() {
    const w = this._container.clientWidth;
    const h = this._container.clientHeight;
    this._camera.aspect = w / h;
    this._camera.updateProjectionMatrix();
    this._resize(w, h);
  }
}

// ------------------------------------------------------------------
// Utility — safe float parse
// ------------------------------------------------------------------
function _f(v) {
  const n = parseFloat(v);
  return isFinite(n) ? n : 0;
}
