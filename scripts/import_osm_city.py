"""Convert a bounded OSM XML extract into the Unreal simulator's offline city map.

Coordinates use a local tangent-plane approximation: X east, Y north, Z up,
in meters. Suitable for neighborhood-scale scenes, not regional mapping.
Buildings use axis-aligned bounding envelopes, not surveyed footprint meshes.
Missing heights use a documented 3 m/floor, three-floor estimate.
"""

import argparse
import json
import math
import xml.etree.ElementTree as ET
from pathlib import Path

EARTH_RADIUS_METERS = 6378137.0
ROAD_TYPES = {
    "motorway",
    "trunk",
    "primary",
    "secondary",
    "tertiary",
    "residential",
    "unclassified",
    "living_street",
    "service",
}


def positive_number(value, default):
    """Accept OSM meters or an explicit 'm' suffix; unsupported units use the fallback."""
    try:
        number = float(value.removesuffix("m").strip())
    except (ValueError, AttributeError):
        return default
    if not math.isfinite(number) or number <= 0:
        return default
    return number


def convert(source, latitude, longitude):
    """Retain source way IDs and flag inferred dimensions for later asset replacement."""
    if not (-85 < latitude < 85 and -180 <= longitude <= 180):
        raise ValueError("origin must be a finite nonpolar latitude/longitude")
    root = ET.parse(source).getroot()
    nodes = {}
    for node in root.findall("node"):
        north = math.radians(float(node.attrib["lat"]) - latitude) * EARTH_RADIUS_METERS
        east = (
            math.radians(float(node.attrib["lon"]) - longitude)
            * EARTH_RADIUS_METERS
            * math.cos(math.radians(latitude))
        )
        nodes[node.attrib["id"]] = [east, north]
    buildings = []
    roads = []
    for way in root.findall("way"):
        tags = {}
        for tag in way.findall("tag"):
            tags[tag.attrib["k"]] = tag.attrib["v"]
        references = [node.attrib["ref"] for node in way.findall("nd")]
        # Do not bridge missing nodes into fictitious roads or building boundaries.
        if any(reference not in nodes for reference in references):
            continue
        points = [nodes[reference] for reference in references]
        if len(points) < 2:
            continue
        if tags.get("building", "no") != "no" and references[0] == references[-1]:
            floors = positive_number(tags.get("building:levels"), 3.0)
            height = positive_number(tags.get("height"), floors * 3.0)
            minimum_x = min(point[0] for point in points)
            minimum_y = min(point[1] for point in points)
            width = max(point[0] for point in points) - minimum_x
            depth = max(point[1] for point in points) - minimum_y
            if min(width, depth) < 1.0:
                continue
            buildings.append(
                {
                    "id": way.attrib["id"],
                    "x": minimum_x,
                    "y": minimum_y,
                    "width": width,
                    "depth": depth,
                    "height": height,
                    "height_inferred": "height" not in tags,
                }
            )
        elif tags.get("highway") in ROAD_TYPES:
            # Elevated/tunnel roads require terrain and bridge geometry, deferred here.
            if tags.get("bridge", "no") != "no" or tags.get("tunnel", "no") != "no":
                continue
            lanes = positive_number(tags.get("lanes"), 2.0)
            width = positive_number(tags.get("width"), lanes * 3.2)
            roads.append({"id": way.attrib["id"], "width": width, "points": points})
    return {
        "schema_version": 1,
        "origin_latitude": latitude,
        "origin_longitude": longitude,
        "attribution": "© OpenStreetMap contributors — ODbL 1.0",
        "license_url": "https://www.openstreetmap.org/copyright",
        "buildings": buildings,
        "roads": roads,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--latitude", required=True, type=float)
    parser.add_argument("--longitude", required=True, type=float)
    args = parser.parse_args()
    city = convert(args.source, args.latitude, args.longitude)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(city, indent=2) + "\n", encoding="utf-8")
    print(f"Converted {len(city['buildings'])} buildings and {len(city['roads'])} roads")


if __name__ == "__main__":
    main()
