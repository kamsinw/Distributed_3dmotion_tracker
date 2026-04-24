"use strict";

/**
 * ThreeScene — manages the Three.js 3D visualisation.
 *
 * Public API:
 *   new ThreeScene(containerEl)   — attach renderer to container
 *   scene.update(data)            — accept a sensor_update payload
 */
class ThreeScene {
  constructor(container) {
    this._container = container;
    this._maxTrail  = 100;

    // Pre-allocated Float32Array for the trail geometry (avoids GC churn)
    this._trailBuf  = new Float32Array(this._maxTrail * 3);

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
  // Public update — called on every sensor_update WebSocket event
  // ----------------------------------------------------------------

  update(data) {
    const mpu   = data.mpu      || {};
    const pos   = data.position || { x: 0, y: 0, z: 0 };
    const trail = data.trail    || [];

    // --- Cube rotation (angles are in radians from server) ---
    this._cube.rotation.x = _f(mpu.roll);
    this._cube.rotation.y = _f(mpu.yaw);
    this._cube.rotation.z = _f(mpu.pitch);

    // --- Cube translation ---
    // x: scale relative distance (cm) to scene units
    // y/z: driven by sin(pitch)/sin(roll) — already bounded to [-1,+1]
    this._cube.position.x = _f(pos.x) * 0.04;
    this._cube.position.y = _f(pos.y) * 1.8;
    this._cube.position.z = _f(pos.z) * 1.8;

    // --- Motion trail ---
    this._updateTrail(trail);

    // --- Camera gently follows cube on X axis ---
    this._camTargetX += (this._cube.position.x - this._camTargetX) * 0.04;
    this._camera.position.x = this._camTargetX;
    this._camera.lookAt(this._camTargetX, 0, 0);
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
      this._trailBuf[idx]     = _f(pt.x) * 0.04;
      this._trailBuf[idx + 1] = _f(pt.y) * 1.8;
      this._trailBuf[idx + 2] = _f(pt.z) * 1.8;
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
