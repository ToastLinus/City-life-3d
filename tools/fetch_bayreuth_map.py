#!/usr/bin/env python3
"""Fetch mapped Bayreuth roads and building footprints from OpenStreetMap.

This is an offline asset-preparation tool. It never contacts the network at
game runtime. The output is GeoJSON for later conversion into 3DS map tiles.
"""
import gzip
import json
import math
import sys
import urllib.parse
import urllib.request

# Public global instances. Private.coffee is an independent operator; the
# VK Maps endpoint is an additional fallback if the European instances are busy.
ENDPOINTS = [
    "https://overpass.private.coffee/api/interpreter",
    "https://maps.mail.ru/osm/tools/overpass/api/interpreter",
    "https://overpass-api.de/api/interpreter",
    "https://overpass.kumi.systems/api/interpreter",
]
# Keep the four requested areas as separate, smaller Overpass requests.
BBOXES = [
    (49.9378, 11.5597, 49.9538, 11.5797),  # Rotmain-Center / Hofgarten
    (49.9329, 11.5715, 49.9469, 11.5895),  # Lidl / Roter Hügel
    (49.9443, 11.5374, 49.9583, 11.5554),  # western Bayreuth area
    (49.9482, 11.5201, 49.9602, 11.5361),  # Unterpreuschwitz
]


def query_for_bbox(bbox):
    south, west, north, east = bbox
    box = f"{south},{west},{north},{east}"
    return f"""
[out:json][timeout:60];
(
  way["highway"~"^(motorway|trunk|primary|secondary|tertiary|unclassified|residential|living_street|service|pedestrian|track|cycleway|footway|path)$"]({box});
  way["building"]({box});
);
out body geom;
"""



def decode_response(raw, content_encoding=""):
    """Decode JSON from Overpass, including servers that gzip despite headers."""
    if raw[:2] == bytes((31, 139)) or content_encoding.lower() == "gzip":
        raw = gzip.decompress(raw)
    return json.loads(raw.decode("utf-8"))


def fetch():
    """Fetch each target area separately to avoid oversized Overpass queries."""
    merged = {}
    for area_index, bbox in enumerate(BBOXES, start=1):
        data = urllib.parse.urlencode({"data": query_for_bbox(bbox)}).encode()
        last_error = None
        received = False
        # Try independent public providers for this smaller area request.
        for endpoint in ENDPOINTS:
            for attempt in range(1):
                request = urllib.request.Request(
                    endpoint,
                    data=data,
                    headers={
                        "User-Agent": "CITY-LIFE-3D map asset builder/1.3 (OSM attribution included; one-time Bayreuth map export)",
                        "Accept": "application/json",
                        "Accept-Encoding": "gzip",
                    },
                )
                try:
                    with urllib.request.urlopen(request, timeout=25) as response:
                        raw = response.read()
                        payload = decode_response(raw, response.headers.get("Content-Encoding", ""))
                    if payload.get("remark"):
                        raise RuntimeError(payload["remark"])
                    # Validate each area independently; never accept an empty tile.
                    build_features(payload)
                    for element in payload.get("elements", []):
                        if element.get("type") == "way" and element.get("id") is not None:
                            merged[element["id"]] = element
                    received = True
                    print(f"Fetched Bayreuth area {area_index}/{len(BBOXES)} from {endpoint}")
                    break
                except Exception as exc:
                    last_error = exc
                    print(f"Area {area_index}, attempt {attempt + 1} failed: {endpoint}: {exc}", file=sys.stderr)
            if received:
                break
            print(f"Trying next Overpass provider for area {area_index}: {endpoint}", file=sys.stderr)
        if not received:
            raise RuntimeError(f"Could not fetch Bayreuth area {area_index}: {last_error}")

    payload = {"elements": list(merged.values()), "osm3s": {"timestamp_osm_base": None}}
    # Validate the combined dataset as well.
    build_features(payload)
    return payload


def valid_point(point):
    try:
        lon, lat = float(point["lon"]), float(point["lat"])
    except (KeyError, TypeError, ValueError):
        return None
    if not (math.isfinite(lon) and math.isfinite(lat)):
        return None
    if not (-180.0 <= lon <= 180.0 and -90.0 <= lat <= 90.0):
        return None
    return [lon, lat]


def build_features(payload):
    """Convert Overpass ways to conservative, valid GeoJSON features."""
    elements = payload.get("elements", [])
    features = []
    seen = set()
    for element in elements:
        if element.get("type") != "way" or not element.get("geometry"):
            continue
        osm_id = element.get("id")
        if osm_id is None or osm_id in seen:
            continue
        tags = element.get("tags", {})
        is_building = "building" in tags
        is_road = "highway" in tags
        if not (is_building or is_road):
            continue

        coords = []
        for point in element["geometry"]:
            converted = valid_point(point)
            if converted is None:
                coords = []
                break
            if not coords or converted != coords[-1]:
                coords.append(converted)
        if len(coords) < 2:
            continue

        if is_building:
            # Only closed ways are footprints. An open outline must never be
            # silently closed into a made-up building polygon.
            if len(coords) < 4 or coords[0] != coords[-1]:
                continue
            if len(set(tuple(p) for p in coords[:-1])) < 3:
                continue
            geometry = {"type": "Polygon", "coordinates": [coords]}
            kind = "building"
        else:
            geometry = {"type": "LineString", "coordinates": coords}
            kind = "road"

        seen.add(osm_id)
        features.append({
            "type": "Feature",
            "id": f"way/{osm_id}",
            "properties": {
                "osm_id": osm_id,
                "kind": kind,
                "building": tags.get("building"),
                "highway": tags.get("highway"),
                "name": tags.get("name"),
                "levels": tags.get("building:levels"),
                "surface": tags.get("surface"),
                "width": tags.get("width"),
            },
            "geometry": geometry,
        })

    if not features:
        raise RuntimeError(
            "Bayreuth resolved, but Overpass returned no valid road/building "
            "ways. Refusing to write an empty map."
        )
    return features, {"name": "Rotmain-Center, Hofgarten, Lidl Meranierring, Unterpreuschwitz"}


def main():
    output = sys.argv[1] if len(sys.argv) > 1 else "bayreuth-osm.geojson"
    payload = fetch()
    features, city_area = build_features(payload)
    roads = sum(f["properties"]["kind"] == "road" for f in features)
    buildings = len(features) - roads
    document = {
        "type": "FeatureCollection",
        "name": "Four target areas in Bayreuth: Rotmain-Center, Hofgarten, Lidl Meranierring, Unterpreuschwitz",
        "source": "© OpenStreetMap contributors",
        "license": "Open Database License (ODbL)",
        "target_areas": city_area.get("name"),
        "osm_timestamp": payload.get("osm3s", {}).get("timestamp_osm_base"),
        "feature_count": len(features),
        "road_way_count": roads,
        "building_way_count": buildings,
        "features": features,
    }
    with open(output, "w", encoding="utf-8") as handle:
        json.dump(document, handle, ensure_ascii=False, separators=(",", ":"))
    print(f"Wrote {output}: {roads} road ways, {buildings} building footprints")
    print(f"OSM data timestamp: {document['osm_timestamp']}")


if __name__ == "__main__":
    main()
