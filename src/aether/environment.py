"""Static obstacle geometry and swept collision checks."""

import numpy as np


def segment_box(start, end, center, half_size, radius=0.0):
    """Intersect a swept center segment with a radius-inflated axis-aligned box.

    The slab test prevents tunneling between samples. Box inflation is a
    conservative sphere approximation at corners, not an exact sphere sweep.
    """
    low = np.asarray(center) - np.asarray(half_size) - radius
    high = np.asarray(center) + np.asarray(half_size) + radius
    direction = np.asarray(end) - start
    enter = 0.0
    leave = 1.0
    for axis in range(3):
        if abs(direction[axis]) < 1e-12:
            if start[axis] < low[axis] or start[axis] > high[axis]:
                return False
        else:
            first_crossing = (low[axis] - start[axis]) / direction[axis]
            second_crossing = (high[axis] - start[axis]) / direction[axis]
            enter = max(enter, min(first_crossing, second_crossing))
            leave = min(leave, max(first_crossing, second_crossing))
            if enter > leave:
                return False
    return True


def collides(start, end, obstacles, radius=0.3):
    """Check the flat ground and every static obstacle over the entire physics step."""
    if min(start[2], end[2]) <= radius:
        return True
    for obstacle in obstacles:
        if segment_box(start, end, obstacle["center"], obstacle["half_size"], radius):
            return True
    return False
