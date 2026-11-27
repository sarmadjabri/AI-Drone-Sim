"""Summarize Unreal CSV frame timings, excluding startup frames and metadata rows."""

import argparse
import csv
import json
from pathlib import Path

import numpy as np


def summarize(path, warmup_frames):
    """Use FrameTime to identify real samples; engine metadata is not a frame."""
    with path.open(newline="", encoding="utf-8-sig") as source:
        rows = csv.reader(source)
        header = next(rows)
        frame_column = header.index("FrameTime")
        samples = []
        for row in rows:
            try:
                frame_time = float(row[frame_column])
            except (ValueError, IndexError):
                continue
            if np.isfinite(frame_time) and frame_time > 0:
                samples.append(row)
    samples = samples[warmup_frames:]
    if not samples:
        raise ValueError("Capture contains no samples after warmup")
    result = {"capture": str(path), "sample_count": len(samples), "warmup_frames": warmup_frames}
    for name in [
        "FrameTime",
        "GameThreadTime",
        "RenderThreadTime",
        "GPU/Total",
        "GPU/ShadowDepths",
    ]:
        if name not in header:
            continue
        column = header.index(name)
        values = []
        for row in samples:
            try:
                value = float(row[column])
            except (ValueError, IndexError):
                continue
            if np.isfinite(value):
                values.append(value)
        if values:
            result[name] = {
                "median_ms": float(np.median(values)),
                "p95_ms": float(np.percentile(values, 95)),
            }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", type=Path, nargs="+")
    parser.add_argument("--warmup-frames", type=int, default=120)
    args = parser.parse_args()
    if args.warmup_frames < 0:
        parser.error("warmup frames must be nonnegative")
    print(json.dumps([summarize(path, args.warmup_frames) for path in args.captures], indent=2))


if __name__ == "__main__":
    main()
