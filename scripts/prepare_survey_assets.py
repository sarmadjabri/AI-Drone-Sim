"""Rebase the municipal survey and flatten the licensed vehicle for Unreal import.

Survey axes entering the OBJ importer are east/north/up. Unreal's OBJ importer
reflects Y, so the resulting game frame is east/south/up in centimeters. Rebasing
keeps millimeter-scale precision near the drone instead of using national-grid
coordinates directly. The original survey files remain untouched under raw/.
"""

import json
import re
from pathlib import Path

import numpy as np
from prepare_vehicle_asset import flatten_vehicle

SURVEY_ORIGIN_METERS = np.array([7350.0, 4950.0, 8.0])


def prepare_city():
    root = Path("Assets/External/HelsinkiSurvey")
    destination = root / "prepared"
    destination.mkdir(parents=True, exist_ok=True)
    tiles = []
    for source in sorted((root / "raw").rglob("*.obj")):
        name = "H_" + source.parent.name + "_" + re.sub(r"[^a-zA-Z0-9_]", "_", source.stem)
        output = destination / (name + ".obj")
        minimum = np.full(3, np.inf)
        maximum = np.full(3, -np.inf)
        triangles = 0
        # UV coordinates, material assignments and face indices remain the survey author's.
        with source.open() as original, output.open("w") as rebased:
            for line in original:
                if line.startswith("v "):
                    vertex = np.array([float(value) for value in line.split()[1:4]])
                    vertex -= SURVEY_ORIGIN_METERS
                    minimum = np.minimum(minimum, vertex)
                    maximum = np.maximum(maximum, vertex)
                    line = f"v {vertex[0]:.6f} {vertex[1]:.6f} {vertex[2]:.6f}\n"
                elif line.startswith("mtllib "):
                    # Textures are imported explicitly; a relative MTL is not needed here.
                    continue
                elif line.startswith("f "):
                    triangles += len(line.split()) - 3
                rebased.write(line)
        texture = source.with_suffix(".mtl").read_text()
        texture_name = re.search(r"^map_Kd\s+(.+)$", texture, re.MULTILINE).group(1).strip()
        tiles.append(
            {
                "name": name,
                "obj": str(output),
                "texture": str(source.parent / texture_name),
                "triangles": triangles,
                "bounds_min_m": minimum.tolist(),
                "bounds_max_m": maximum.tolist(),
            }
        )
    manifest = {
        "schema_version": 1,
        "city": "Helsinki Senate Square",
        "publisher": "City of Helsinki",
        "survey_year": 2017,
        "license": "CC BY 4.0",
        "origin_projected_m": [25497350, 6672950, 8],
        "game_axes": "X east, Y south, Z up; centimeters",
        "tiles": tiles,
    }
    (root / "prepared_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(
        f"Prepared {len(tiles)} survey meshes, {sum(tile['triangles'] for tile in tiles):,} triangles",
        flush=True,
    )


if __name__ == "__main__":
    prepare_city()
    flatten_vehicle()
