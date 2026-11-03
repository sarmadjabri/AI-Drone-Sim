"""Scalar-first quaternions rotate body vectors into the world frame."""
import numpy as np


def multiply(a, b):
    w, x, y, z = a
    v, i, j, k = b
    return np.array([w*v-x*i-y*j-z*k, w*i+x*v+y*k-z*j,
                     w*j-x*k+y*v+z*i, w*k+x*j-y*i+z*v])


def rotation(q):
    w, x, y, z = q / np.linalg.norm(q)
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                     [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                     [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])


def integrate(q, omega, dt):
    omega = np.asarray(omega, dtype=float)
    angle = np.linalg.norm(omega)*dt
    vector = omega * (0.5*dt if angle < 1e-9 else np.sin(angle/2)*dt/angle)
    result = multiply(q, np.r_[np.cos(angle/2), vector])
    return result / np.linalg.norm(result)
