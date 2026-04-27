"use strict";

(function () {

  // ------------------------------------------------------------------
  // Bootstrap Three.js scene
  // ------------------------------------------------------------------
  const container = document.getElementById("canvas-wrap");
  const scene = new ThreeScene(container);

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

}());
