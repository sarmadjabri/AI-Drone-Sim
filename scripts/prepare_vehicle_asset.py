"""Download and flatten the licensed CarConcept model for native import.

Vehicle preparation is independent of any city dataset. Original materials,
textures and authored scene transforms are retained in the combined GLB.
"""

import hashlib
import json
import struct
from pathlib import Path

import numpy as np
import requests


def flatten_vehicle():
    """Bake each original scene node's transform into a single static glTF mesh."""
    root = Path("Assets/External/CarConcept")
    data = (root / "CarConcept.glb").read_bytes()
    json_size, chunk_kind = struct.unpack_from("<II", data, 12)
    if chunk_kind != 0x4E4F534A:
        raise ValueError("First glTF chunk must contain JSON")
    document = json.loads(data[20 : 20 + json_size])
    bin_start = 20 + json_size + 8
    binary = bytearray(data[bin_start:])
    primitives = []

    def bake_accessor(index, transform, directions=False, tangent=False):
        accessor = document["accessors"][index]
        view = document["bufferViews"][accessor["bufferView"]]
        if accessor["componentType"] != 5126:
            raise ValueError("Vehicle vertex transforms require float accessors")
        columns = 3
        if tangent:
            columns = 4
        offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        stride = view.get("byteStride", columns * 4)
        values = np.ndarray(
            (accessor["count"], columns),
            dtype="<f4",
            buffer=binary,
            offset=offset,
            strides=(stride, 4),
        ).copy()
        if directions:
            normal_matrix = np.linalg.inv(transform[:3, :3]).T
            values[:, :3] = values[:, :3] @ normal_matrix.T
            lengths = np.linalg.norm(values[:, :3], axis=1)
            values[:, :3] /= np.maximum(lengths[:, None], 1e-12)
            if tangent and np.linalg.det(transform[:3, :3]) < 0:
                values[:, 3] *= -1
        else:
            values[:, :3] = values[:, :3] @ transform[:3, :3].T + transform[:3, 3]
        while len(binary) % 4:
            binary.append(0)
        new_view = {
            "buffer": 0,
            "byteOffset": len(binary),
            "byteLength": values.nbytes,
            "target": 34962,
        }
        binary.extend(values.astype("<f4").tobytes())
        document["bufferViews"].append(new_view)
        new_accessor = {
            "bufferView": len(document["bufferViews"]) - 1,
            "componentType": 5126,
            "count": len(values),
            "type": accessor["type"],
        }
        if not directions:
            new_accessor["min"] = values.min(axis=0).tolist()
            new_accessor["max"] = values.max(axis=0).tolist()
        document["accessors"].append(new_accessor)
        return len(document["accessors"]) - 1

    def visit(index, parent_transform):
        node = document["nodes"][index]
        if "matrix" in node:
            local = np.array(node["matrix"], dtype=float).reshape(4, 4).T
        else:
            local = np.eye(4)
            quaternion = node.get("rotation", [0, 0, 0, 1])
            from scipy.spatial.transform import Rotation

            local[:3, :3] = Rotation.from_quat(quaternion).as_matrix() @ np.diag(
                node.get("scale", [1, 1, 1])
            )
            local[:3, 3] = node.get("translation", [0, 0, 0])
        world = parent_transform @ local
        if "mesh" in node:
            for original in document["meshes"][node["mesh"]]["primitives"]:
                primitive = dict(original)
                attributes = dict(original["attributes"])
                attributes["POSITION"] = bake_accessor(attributes["POSITION"], world)
                if "NORMAL" in attributes:
                    attributes["NORMAL"] = bake_accessor(attributes["NORMAL"], world, True)
                if "TANGENT" in attributes:
                    attributes["TANGENT"] = bake_accessor(attributes["TANGENT"], world, True, True)
                primitive["attributes"] = attributes
                primitives.append(primitive)
        for child in node.get("children", []):
            visit(child, world)

    for node in document["scenes"][document.get("scene", 0)]["nodes"]:
        visit(node, np.eye(4))
    document["nodes"] = [{"name": "SurveyTrafficCar", "mesh": 0}]
    document["meshes"] = [{"name": "SurveyTrafficCar", "primitives": primitives}]
    document["scenes"] = [{"nodes": [0]}]
    document["scene"] = 0
    document["buffers"] = [{"byteLength": len(binary)}]
    encoded = json.dumps(document, separators=(",", ":")).encode()
    encoded += b" " * (-len(encoded) % 4)
    binary.extend(b"\0" * (-len(binary) % 4))
    total_size = 12 + 8 + len(encoded) + 8 + len(binary)
    output = (
        struct.pack("<III", 0x46546C67, 2, total_size)
        + struct.pack("<II", len(encoded), 0x4E4F534A)
        + encoded
        + struct.pack("<II", len(binary), 0x004E4942)
        + binary
    )
    (root / "SurveyTrafficCar.glb").write_bytes(output)
    print(f"Flattened {len(primitives)} authored vehicle parts without remodeling them", flush=True)


def prepare_vehicle():
    root = Path("Assets/External/CarConcept")
    root.mkdir(parents=True, exist_ok=True)
    source = root / "CarConcept.glb"
    expected = "c272098089d78c5cd9fd9f24ff50ee8acf8d932c55f2d55fc10adb6c8998966b"
    if not source.exists():
        url = "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/CarConcept/glTF-Binary/CarConcept.glb"
        with requests.get(url, stream=True, timeout=(30, 120)) as response:
            response.raise_for_status()
            with source.with_suffix(".partial").open("wb") as handle:
                for block in response.iter_content(1024 * 1024):
                    handle.write(block)
        source.with_suffix(".partial").replace(source)
    if hashlib.sha256(source.read_bytes()).hexdigest() != expected:
        raise ValueError("CarConcept differs from the pinned source asset")
    flatten_vehicle()


if __name__ == "__main__":
    prepare_vehicle()
