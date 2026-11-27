"""Download a bounded, highest-detail subset of Helsinki's official 2017 survey.

The municipal archive is a public ZIP containing many independent tiles. HTTP
ranges fetch only the six selected directory ranges, rather than its full 2 GB.
Original OBJ/MTL/JPEG files and CRC checks are retained. No city geometry is invented.
Install the optional asset tooling dependencies before running this script.
"""

import argparse
import json
import struct
import zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import requests
from remotezip import RemoteZip

ARCHIVE_URL = (
    "https://3d.hel.ninja/data/mesh/Helsinki3D-MESH_2017_OBJ_2km-250m_ZIP/"
    "Helsinki3D_2017_OBJ_672496x2.zip"
)
DIRECTORIES = ["672497b2", "672497b4", "672497d2", "673497a1", "673497a3", "673497c1"]


def download_directory(directory, entries, destination):
    """Validate each selected original member against the publisher's ZIP CRC."""
    members = [entry for entry in entries if entry.filename.startswith(directory + "/")]
    first_byte = min(entry.header_offset for entry in members)
    last_entry = max(members, key=lambda entry: entry.header_offset)
    last_byte = (
        last_entry.header_offset
        + last_entry.compress_size
        + len(last_entry.filename.encode())
        + 65536
    )
    response = requests.get(
        ARCHIVE_URL, headers={"Range": f"bytes={first_byte}-{last_byte}"}, timeout=120
    )
    response.raise_for_status()
    if response.status_code != 206:
        raise RuntimeError("The server must honor byte ranges; refusing a full archive download")
    data = response.content
    count = 0
    for entry in members:
        if "_L21_" not in entry.filename or not entry.filename.endswith((".obj", ".mtl", ".jpg")):
            continue
        output = destination / entry.filename
        existing_file_is_valid = False
        if output.exists() and output.stat().st_size == entry.file_size:
            existing_file_is_valid = zlib.crc32(output.read_bytes()) == entry.CRC
        if existing_file_is_valid:
            count += 1
            continue
        offset = entry.header_offset - first_byte
        header = struct.unpack_from("<4s5H3I2H", data, offset)
        if header[0] != b"PK\x03\x04":
            raise ValueError("Invalid ZIP local header")
        payload_start = offset + 30 + header[-2] + header[-1]
        payload = data[payload_start : payload_start + entry.compress_size]
        if entry.compress_type == 8:
            payload = zlib.decompress(payload, -15)
        elif entry.compress_type != 0:
            raise ValueError("Unsupported archive compression")
        if len(payload) != entry.file_size or zlib.crc32(payload) != entry.CRC:
            raise ValueError("Survey file failed its size or CRC check: " + entry.filename)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(payload)
        count += 1
    print(f"{directory}: {count} highest-detail source files", flush=True)
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--destination", type=Path, default=Path("Assets/External/HelsinkiSurvey/raw")
    )
    args = parser.parse_args()
    with RemoteZip(ARCHIVE_URL) as archive:
        entries = archive.infolist()
        metadata = archive.read("metadata.xml")
    args.destination.mkdir(parents=True, exist_ok=True)
    (args.destination / "metadata.xml").write_bytes(metadata)
    with ThreadPoolExecutor(max_workers=3) as executor:
        futures = [
            executor.submit(download_directory, directory, entries, args.destination)
            for directory in DIRECTORIES
        ]
        for future in futures:
            future.result()
    manifest = {
        "archive": ARCHIVE_URL,
        "directories": DIRECTORIES,
        "source_lod": 21,
        "license": "CC BY 4.0",
        "publisher": "City of Helsinki",
        "source_crs": "EPSG:3879+5773",
        "source_origin": [25490000, 6668000, 0],
    }
    (args.destination.parent / "download_manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
