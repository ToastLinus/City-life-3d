#!/usr/bin/env python3
"""Fetch the mapped Bayreuth street/building geometry from OpenStreetMap.

This is an offline asset-preparation tool. It does not contact the network at
game runtime. Output is GeoJSON so the geometry can be inspected and tiled
before it is converted to the Nintendo 3DS runtime format.
"""
import json
import sys
import time
import urllib.parse
import urllib.request

ENDPOINTS = [
    "https://overpass-api.de/api/interpreter",
    "https://overpass.kumi.systems/api/interpreter",
]
QUERY = r"""
[out:json][timeout:180];
area["name"="Bayreuth"]["boundary"="administrative"]["admin_level"="8"]->.city;
(
  way["highway"~"^(motorway|trunk|primary|secondary|tertiary|unclassified|residential|living_street|service|pedestrian|track|cycleway|footway|path)$"](area.city);
  way["building"](area.city);
);
out body geom;
"""

def fetch():
    data = urllib.parse.urlencode({"data": QUERY}).encode()
    last_error = None
    for endpoint in ENDPOINTS:
        request = urllib.request.Request(
            endpoint, data=data,
            headers={"User-Agent": "CITY-LIFE-3D map asset builder/1.0"}
        )
        try:
            with urllib.request.urlopen(request, timeout=210) as response:
                payload = json.load(response)
            if payload.get("remark"):
                raise RuntimeError(payload["remark"])
            return payload
        except Exception as exc:
            last_error = exc
            print(f"Overpass endpoint failed: {endpoint}: {exc}", file=sys.stderr)
            time.sleep(2)
    raise RuntimeError(f"All Overpass endpoints failed: {last_error}")

def main():
    output = sys.argv[1] if len(sys.argv) > 1 else "bayreuth-osm.geojson"
    payload = fetch()
    features = []
    for element in payload.get("elements", []):
        if element.get("type") != "way" or not element.get("geometry"):
            continue
        tags = element.get("tags", {})
        coords = [[p["lon"], p["lat"]] for p in element["geometry"]]
        is_building = "building" in tags
        is_road = "highway" in tags
        if not (is_building or is_road) or len(coords) < 2:
            continue
        geometry_type = "Polygon" if is_building and len(coords) >= 4 else "LineString"
        geometry = [coords] if geometry_type == "Polygon" else coords
        if geometry_type == "Polygon" and coords[0] != coords[-1]:
            geometry[0].append(coords[0])
        features.append({
            "type": "Feature",
            "id": f"way/{element['id']}",
            "properties": {
                "osm_id": element["id"],
                "kind": "building" if is_building else "road",
                "building": tags.get("building"),
                "highway": tags.get("highway"),
                "name": tags.get("name"),
                "levels": tags.get("building:levels"),
                "surface": tags.get("surface"),
                "width": tags.get("width"),
            },
            "geometry": {"type": geometry_type, "coordinates": geometry},
        })
    if not features:
        raise RuntimeError("Overpass returned no road/building geometry; refusing to write an empty map.")
    document = {
        "type": "FeatureCollection",
        "name": "Bayreuth municipal road and building geometry",
        "source": "© OpenStreetMap contributors",
        "license": "Open Database License (ODbL)",
        "osm_timestamp": payload.get("osm3s", {}).get("timestamp_osm_base"),
        "feature_count": len(features),
        "features": features,
    }
    with open(output, "w", encoding="utf-8") as handle:
        json.dump(document, handle, ensure_ascii=False, separators=(",", ":"))
    roads = sum(f["properties"]["kind"] == "road" for f in features)
    buildings = len(features) - roads
    print(f"Wrote {output}: {roads} road ways, {buildings} building footprints")
    print(f"OSM data timestamp: {document['osm_timestamp']}")

if __name__ == "__main__":
    main()
