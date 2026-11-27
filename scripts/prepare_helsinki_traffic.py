"""Derive a closed traffic circuit from real, connected OSM street centerlines.

Only the moving-obstacle routes use OSM. Roads, facades, trees and parked objects
are rendered from Helsinki's photographic survey. Current street data can differ
from the 2017 scan; collisions with scanned objects remain active.
"""

import heapq
import json
import xml.etree.ElementTree as ET
from itertools import pairwise
from pathlib import Path

import numpy as np
from pyproj import Transformer

source = Path("Assets/External/HelsinkiSurvey/helsinki_streets.osm")
root = ET.parse(source).getroot()
projection = Transformer.from_crs(4326, 3879, always_xy=True)
nodes = {}
for node in root.findall("node"):
    east, north = projection.transform(float(node.attrib["lon"]), float(node.attrib["lat"]))
    nodes[node.attrib["id"]] = np.array([east - 25497350, -(north - 6672950)])
network = {}
allowed = {"residential", "secondary", "tertiary", "unclassified", "service"}
for way in root.findall("way"):
    tags = {tag.attrib["k"]: tag.attrib["v"] for tag in way.findall("tag")}
    if tags.get("highway") not in allowed or tags.get("area") == "yes":
        continue
    references = [node.attrib["ref"] for node in way.findall("nd")]
    for first, second in pairwise(references):
        if first not in nodes or second not in nodes:
            continue
        if np.any(nodes[first] < [-300, -280]) or np.any(nodes[first] > [350, 180]):
            continue
        distance = float(np.linalg.norm(nodes[first] - nodes[second]))
        if tags.get("oneway") != "-1":
            network.setdefault(first, []).append((second, distance))
        if tags.get("oneway") != "yes":
            network.setdefault(second, []).append((first, distance))


def shortest_path(start, finish):
    """Dijkstra's algorithm keeps routes on the connected, directed street network."""
    queue = [(0.0, start)]
    distances = {start: 0.0}
    parents = {}
    while queue:
        distance, current = heapq.heappop(queue)
        if distance != distances[current]:
            continue
        if current == finish:
            path = [current]
            while current != start:
                current = parents[current]
                path.append(current)
            path.reverse()
            return path
        for neighbor, cost in network.get(current, []):
            candidate = distance + cost
            if candidate < distances.get(neighbor, float("inf")):
                distances[neighbor] = candidate
                parents[neighbor] = current
                heapq.heappush(queue, (candidate, neighbor))
    raise ValueError("No connected street circuit exists inside the survey")


corners = [(-69, 43), (69, 37), (65, -151), (-81, -143)]
anchors = []
for corner in corners:
    nearest = min(network, key=lambda node: np.linalg.norm(nodes[node] - corner))
    anchors.append(nearest)
route = []
for start, finish in zip(anchors, anchors[1:] + anchors[:1]):
    segment = shortest_path(start, finish)
    route.extend(segment[:-1])
# Remove centimeter-spaced OSM detail; retain each real change of direction.
points = []
for node in route:
    point = nodes[node]
    if points and np.linalg.norm(point - points[-1]) < 2.0:
        continue
    points.append(point)
manifest = {
    "schema_version": 1,
    "attribution": "© OpenStreetMap contributors — ODbL 1.0",
    "license_url": "https://www.openstreetmap.org/copyright",
    "source": str(source),
    "closed_loop": True,
    "points_cm": [[float(point[0] * 100), float(point[1] * 100), 0.0] for point in points],
}
output = Path("Content/CityData/helsinki_traffic.json")
output.write_text(json.dumps(manifest, indent=2) + "\n")
print(f"Derived {len(points)} route points from connected real streets")
