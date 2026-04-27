"use strict";

(function () {

  // ------------------------------------------------------------------
  // Bootstrap Three.js scene
  // ------------------------------------------------------------------
  const container = document.getElementById("canvas-wrap");
  const scene = new ThreeScene(container);

  // ------------------------------------------------------------------
  // Reset button
  // ------------------------------------------------------------------
  document.getElementById("btn-reset").addEventListener("click", () => {
    scene.resetToCenter();
  });

  // ------------------------------------------------------------------
  // Calibration — collect CAL_SAMPLES position readings while the
  // device is held still, then use their mean as the zero offset.
  // ------------------------------------------------------------------
  const CAL_SAMPLES = 30;
  let _calCount = 0;
  let _calSum   = { x: 0, y: 0, z: 0 };
  const btnCal  = document.getElementById("btn-cal");

  btnCal.addEventListener("click", () => {
    if (_calCount > 0) return;   // already collecting
    _calCount     = CAL_SAMPLES;
    _calSum.x = _calSum.y = _calSum.z = 0;
    btnCal.textContent = "⏳ Hold still…";
    btnCal.classList.add("calibrating");
    btnCal.disabled = true;
  });

  // ------------------------------------------------------------------
  // Socket.IO connection
  // ------------------------------------------------------------------
  const socket = io("http://localhost:5001", {
    reconnectionDelay:    1000,
    reconnectionAttempts: Infinity,
  });

  socket.on("connect", () => {
    console.log("[WS] connected — sid:", socket.id);
    _setNodeStatus("dot-mpu", "st-mpu", false, "waiting");
  });

  socket.on("disconnect", () => {
    console.warn("[WS] disconnected");
    _setNodeStatus("dot-mpu", "st-mpu", null, "offline");
  });

  socket.on("sensor_update", (data) => {
    // Accumulate calibration samples before forwarding to the scene
    if (_calCount > 0) {
      const pos = data.position || {};
      _calSum.x += _f(pos.x);
      _calSum.y += _f(pos.y);
      _calSum.z += _f(pos.z);
      _calCount--;
      if (_calCount === 0) {
        scene.setCalibrationOffset(
          _calSum.x / CAL_SAMPLES,
          _calSum.y / CAL_SAMPLES,
          _calSum.z / CAL_SAMPLES
        );
        scene.resetToCenter();
        btnCal.textContent = "⊙ Calibrate";
        btnCal.classList.remove("calibrating");
        btnCal.disabled = false;
      }
    }

    // Forward raw payload to the 3D scene
    scene.update(data);

    // Destructure with safe defaults
    const mpu   = data.mpu      || {};
    const pos   = data.position || {};
    const trail = data.trail    || [];

    // ---- Motion state badge ----------------------------------------
    const state = String(mpu.state || "IDLE").toUpperCase();
    _updateStateBadge(state);

    // ---- Orientation -----------------------------------------------
    _set("v-roll",  _fmt(mpu.roll));
    _set("v-pitch", _fmt(mpu.pitch));
    _set("v-yaw",   _fmt(mpu.yaw));

    // ---- Fused position --------------------------------------------
    _set("v-x", _fmt(pos.x));
    _set("v-y", _fmt(pos.y));
    _set("v-z", _fmt(pos.z));

    // ---- Trail point count -----------------------------------------
    _set("v-trail", trail.length);

    // ---- Node status lights ----------------------------------------
    _setNodeStatus("dot-mpu", "st-mpu", data.mpu_stale, data.mpu_stale ? "stale" : "live");

    // ---- Ultrasonic ------------------------------------------------
    const ultra = data.ultra || {};
    _setNodeStatus("dot-ultra", "st-ultra", data.ultra_stale, data.ultra_stale ? "stale" : "live");
    _set("v-dist",   isFinite(parseFloat(ultra.distance)) ? _fmt(ultra.distance, 1) + " cm"   : "— cm");
    _set("v-uspeed", isFinite(parseFloat(ultra.speed))    ? _fmt(ultra.speed,    1) + " cm/s" : "— cm/s");
  });

  // ------------------------------------------------------------------
  // State badge helper
  // ------------------------------------------------------------------
  const _STATE_CLASS = {
    IDLE:  "s-idle",
    MOVE:  "s-move",
    SHAKE: "s-shake",
    DROP:  "s-drop",
  };

  function _updateStateBadge(state) {
    const el = document.getElementById("state-badge");
    el.textContent = state;
    // Remove previous state class
    el.className = _STATE_CLASS[state] || "s-idle";
  }

  // ------------------------------------------------------------------
  // Node status dot helper
  // ------------------------------------------------------------------
  function _setNodeStatus(dotId, textId, stale, label) {
    const dot = document.getElementById(dotId);
    const txt = document.getElementById(textId);
    if (stale === null) {
      dot.className = "dot dot-wait";
    } else if (stale) {
      dot.className = "dot dot-stale";
    } else {
      dot.className = "dot dot-ok";
    }
    if (txt) txt.textContent = label || "";
  }

  // ------------------------------------------------------------------
  // Tiny DOM helpers
  // ------------------------------------------------------------------
  function _set(id, value) {
    const el = document.getElementById(id);
    if (el) el.textContent = value;
  }

  function _fmt(v, decimals) {
    const d = (decimals === undefined) ? 3 : decimals;
    const n = parseFloat(v);
    return isFinite(n) ? n.toFixed(d) : "—";
  }

  function _f(v) {
    const n = parseFloat(v);
    return isFinite(n) ? n : 0;
  }

}());
