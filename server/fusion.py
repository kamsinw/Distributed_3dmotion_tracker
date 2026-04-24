"""
fusion.py — Sensor fusion and position reconstruction.

Uses MPU9250 orientation (roll, pitch, yaw) and HC-SR04 relative
distance to compute a 3-D position estimate.  A deque of the last
100 positions is maintained for the motion trail.
"""

import math
from collections import deque


class SensorFusion:
    def __init__(self, trail_size: int = 100):
        self._trail: deque = deque(maxlen=trail_size)

    def fuse(self, mpu_data: dict, ultra_data: dict) -> dict:
        """
        Compute fused 3-D position from latest sensor readings.

        Mapping (all values in sensor-native units):
            x = relative_distance  (cm from HC-SR04 baseline)
            y = sin(pitch)         (dimensionless, -1 … +1)
            z = sin(roll)          (dimensionless, -1 … +1)
        """
        roll  = float(mpu_data.get("roll",  0.0))
        pitch = float(mpu_data.get("pitch", 0.0))
        rel   = float(ultra_data.get("relative_distance", 0.0))

        pos = {
            "x": round(rel,              4),
            "y": round(math.sin(pitch),  4),
            "z": round(math.sin(roll),   4),
        }

        self._trail.append(pos)
        return pos

    def get_trail(self) -> list:
        """Return ordered list of position dicts (oldest first)."""
        return list(self._trail)
