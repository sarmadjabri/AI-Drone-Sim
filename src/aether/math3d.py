"""Scalar-first quaternions rotate body vectors into the world frame."""

import numpy as np


def multiply(a, b):
    """Compose scalar-first Hamilton quaternions without silently normalizing inputs."""
    a = np.asarray(a, dtype=float)
    b = np.asarray(b, dtype=float)
    if (
        a.shape != (4,)
        or b.shape != (4,)
        or not np.all(np.isfinite(a))
        or not np.all(np.isfinite(b))
    ):
        raise ValueError("quaternion inputs must be finite four-vectors")
    w, x, y, z = a
    v, i, j, k = b
    return np.array(
        [
            w * v - x * i - y * j - z * k,
            w * i + x * v + y * k - z * j,
            w * j - x * k + y * v + z * i,
            w * k + x * j - y * i + z * v,
        ]
    )


def rotation(q):
    """Return a body-to-world rotation matrix after validating quaternion magnitude."""
    q = np.asarray(q, dtype=float)
    if q.shape != (4,) or not np.all(np.isfinite(q)):
        raise ValueError("quaternion must be a finite four-vector")
    norm = np.linalg.norm(q)
    if norm <= 1e-12:
        raise ValueError("quaternion must have nonzero norm")
    w, x, y, z = q / norm
    return np.array(
        [
            [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
        ]
    )


def integrate(q, omega, dt):
    """Advance orientation with a constant body angular rate in radians/second."""
    q = np.asarray(q, dtype=float)
    omega = np.asarray(omega, dtype=float)
    if (
        q.shape != (4,)
        or omega.shape != (3,)
        or not np.all(np.isfinite(q))
        or not np.all(np.isfinite(omega))
    ):
        raise ValueError("quaternion and omega must be finite 4- and 3-vectors")
    if not np.isfinite(dt):
        raise ValueError("dt must be finite")
    angle = np.linalg.norm(omega) * dt
    # Use the analytic small-angle limit instead of dividing by zero.
    if abs(angle) < 1e-09:
        rotation_scale = 0.5 * dt
    else:
        rotation_scale = np.sin(angle / 2) * dt / angle
    vector = omega * (rotation_scale)
    result = multiply(q, np.r_[np.cos(angle / 2), vector])
    norm = np.linalg.norm(result)
    if norm <= 1e-12:
        raise ValueError("rotation update produced a zero quaternion")
    return result / norm
