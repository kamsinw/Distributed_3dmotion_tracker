"""
fusion.py — Direct angle-to-position mapping.

Roll  → position X  (tilt left/right)
Pitch → position Y  (tilt forward/back)
Yaw   → position Z  (spin)

No integration, no drift, no thresholds.
"""

from collections import deque


class SensorFusion:
    def __init__(self, trail_size: int = 100):
        self._trail: deque = deque(maxlen=trail_size)
        self._last_pos: dict = {"x": 0.0, "y": 0.0, "z": 0.0}

    def fuse(self, mpu_data: dict) -> dict:
        # Use sensor B (pos_roll/pos_pitch) for position when available,
        # otherwise fall back to sensor A orientation angles.
        pos = {
            "x": round(float(mpu_data.get("pos_roll",  mpu_data.get("roll",  0.0))), 4),
            "y": round(float(mpu_data.get("pos_pitch", mpu_data.get("pitch", 0.0))), 4),
            "z": round(float(mpu_data.get("yaw", 0.0)), 4),
        }
        self._last_pos = pos
        self._trail.append(pos)
        return pos

    def reset(self):
        self._last_pos = {"x": 0.0, "y": 0.0, "z": 0.0}

    def get_last_pos(self) -> dict:
        return self._last_pos

    def get_trail(self) -> list:
        return list(self._trail)
