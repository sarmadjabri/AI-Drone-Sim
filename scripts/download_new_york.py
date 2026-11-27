"""Download NVIDIA's official authored New York-style brownstone sample pack.

The publisher's package terms are retained during extraction. Raw source and
intermediates are local import inputs, not standalone redistributable assets.
"""

import hashlib
import zipfile
from pathlib import Path

import requests

URL = "https://d4i3qtqj3r0z5.cloudfront.net/AECDemo_NVD%4010012.zip"
EXPECTED_SHA256 = "c97bec134962ee5ae0f7b1df713656242ae51cf1d6090d2dc832e287cbcbe5a8"
ROOT = Path("Assets/External/NvidiaBrownstone")
ARCHIVE = ROOT / "source.zip"
ROOT.mkdir(parents=True, exist_ok=True)
if not ARCHIVE.exists():
    temporary = ARCHIVE.with_suffix(".partial")
    with requests.get(URL, stream=True, timeout=(30, 120)) as response:
        response.raise_for_status()
        with temporary.open("wb") as handle:
            for block in response.iter_content(1024 * 1024):
                handle.write(block)
    temporary.replace(ARCHIVE)
with ARCHIVE.open("rb") as handle:
    checksum = hashlib.file_digest(handle, "sha256").hexdigest()
if checksum != EXPECTED_SHA256:
    raise ValueError("Source archive differs from the pinned NVIDIA package: " + checksum)
with zipfile.ZipFile(ARCHIVE) as archive:
    # A complete CRC check detects partial/corrupt downloads before importing.
    invalid = archive.testzip()
    if invalid:
        raise ValueError("Archive CRC failed: " + invalid)
    for entry in archive.infolist():
        target = (ROOT / entry.filename).resolve()
        if not target.is_relative_to(ROOT.resolve()):
            raise ValueError("Unsafe archive path: " + entry.filename)
    archive.extractall(ROOT)
print("Source archive SHA256:", checksum)
print("Extracted licensed NVIDIA brownstone sample:", ROOT)
