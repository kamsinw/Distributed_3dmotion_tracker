"use strict";
//gen with llm
const SCALE    = 8.0;
const LERP     = 0.12;
const MAX_JUMP = 2.0;

const CAM_Z_DEFAULT  = 6;
const CAM_Z_MIN      = 3;
const CAM_Z_MAX      = 14;
const ULTRA_DIST_MIN = 10;
const ULTRA_DIST_MAX = 80;

class ThreeScene {
  constructor(container) {
    this._container = container;
    this._maxTrail  = 100;

    this._trailBuf  = new Float32Array(this._maxTrail * 3);

    this._targetPos = { x: 0, y: 0, z: 0 };

    this._calOffset = { x: 0, y: 0, z: 0 };

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
    this._camTargetX = 0;
  }

  _initLights() {
    this._scene.add(new THREE.AmbientLight(0x223355, 0.9));

    const key = new THREE.DirectionalLight(0x5599ff, 1.4);
    key.position.set(4, 8, 6);
    this._scene.add(key);

    const rim = new THREE.DirectionalLight(0xff6622, 0.45);
    rim.position.set(-6, -2, -4);
    this._scene.add(rim);

    const fill = new THREE.DirectionalLight(0x0044aa, 0.3);
    fill.position.set(0, -6, 2);
    this._scene.add(fill);
  }

  _initGrid() {
    const grid = new THREE.GridHelper(30, 30, 0x0e2040, 0x091528);
    grid.position.y = -2.5;
    this._scene.add(grid);
  }

  _initCube() {
    const geo = new THREE.BoxGeometry(1.4, 1.4, 1.4);

    const mat = new THREE.MeshPhongMaterial({
      color:     0x1a5fcc,
      emissive:  0x061228,
      specular:  0x88ccff,
      shininess: 90,
    });
    this._cube = new THREE.Mesh(geo, mat);
    this._scene.add(this._cube);

    const wireMat = new THREE.MeshBasicMaterial({
      color:       0x44aaff,
      wireframe:   true,
      transparent: true,
      opacity:     0.22,
    });
    this._cube.add(new THREE.Mesh(geo, wireMat));

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

  update(data) {
    const mpu   = data.mpu      || {};
    const pos   = data.position || { x: 0, y: 0, z: 0 };
    const trail = data.trail    || [];

    this._cube.rotation.x = _f(mpu.pitch);
    this._cube.rotation.y = _f(mpu.roll);
    this._cube.rotation.z = _f(mpu.yaw);

    const rawX = (_f(pos.x) - this._calOffset.x) * SCALE;
    const rawY = (_f(pos.y) - this._calOffset.y) * SCALE;
    const rawZ = (_f(pos.z) - this._calOffset.z) * SCALE;

    this._targetPos.x = this._clamp(rawX, this._targetPos.x);
    this._targetPos.y = this._clamp(rawY, this._targetPos.y);
    this._targetPos.z = this._clamp(rawZ, this._targetPos.z);

    const ultra = data.ultra || {};
    const dist  = _f(ultra.distance);
    if (!data.ultra_stale && dist > 0) {
      const t = Math.max(0, Math.min(1,
        (dist - ULTRA_DIST_MIN) / (ULTRA_DIST_MAX - ULTRA_DIST_MIN)
      ));
      this._targetCamZ = CAM_Z_MIN + t * (CAM_Z_MAX - CAM_Z_MIN);
    }

    this._updateTrail(trail);
  }

  _clamp(val, prev) {
    const delta = val - prev;
    if (Math.abs(delta) > MAX_JUMP) return prev + Math.sign(delta) * MAX_JUMP;
    return val;
  }

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

  _animate() {
    requestAnimationFrame(() => this._animate());

    this._cube.position.x += (this._targetPos.x - this._cube.position.x) * LERP;
    this._cube.position.y += (this._targetPos.y - this._cube.position.y) * LERP;
    this._cube.position.z += (this._targetPos.z - this._cube.position.z) * LERP;

    this._camTargetX += (this._cube.position.x - this._camTargetX) * 0.04;
    this._camera.position.x = this._camTargetX;

    this._camera.position.z += (this._targetCamZ - this._camera.position.z) * LERP;

    this._camera.lookAt(this._camTargetX, 0, 0);

    this._renderer.render(this._scene, this._camera);
  }

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

function _f(v) {
  const n = parseFloat(v);
  return isFinite(n) ? n : 0;
}
