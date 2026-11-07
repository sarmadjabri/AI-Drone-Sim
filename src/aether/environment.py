"""Static obstacle geometry and swept collision checks."""

import numpy as np


def segment_box(start, end, center, half_size, radius=0.0):
    low = np.asarray(center) - np.asarray(half_size) - radius
    high = np.asarray(center) + np.asarray(half_size) + radius
    direction = np.asarray(end) - start
    enter, leave = 0.0, 1.0
    for axis in range(3):
        if abs(direction[axis]) < 1e-12:
            if start[axis] < low[axis] or start[axis] > high[axis]:
                return False
        else:
            a = (low[axis] - start[axis]) / direction[axis]
            b = (high[axis] - start[axis]) / direction[axis]
            enter, leave = max(enter, min(a, b)), min(leave, max(a, b))
            if enter > leave:
                return False
    return True


def collides(start, end, obstacles, radius=0.3):
    if min(start[2], end[2]) <= radius:
        return True
    return any(segment_box(start, end, o["center"], o["half_size"], radius) for o in obstacles)
