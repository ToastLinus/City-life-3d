#!/usr/bin/env python3
"""Fetch mapped Bayreuth roads and building footprints from OpenStreetMap.

This is an offline asset-preparation tool. It never contacts the network at
game runtime. The output is GeoJSON for later conversion into 3DS map tiles.
"""
import gzip
import json
import math
import sys
import time
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
QUERY = r"""
[out:json][timeout:180];
rel["name"="Bayreuth"]["boundary"="administrative"]->.cityrel;
.cityrel map_to_area -> .city;
.cityrel out tags;
(
  way["highway"~"^(motorway|trunk|primary|secondary|tertiary|unclassified|residential|living_street|service|pedestrian|track|cycleway|footway|path)$"](area.city);
  way["building"](area.city);
);
out body geom;
"""


def decode_response(raw, content_encoding=""):
    """Decode JSON from Overpass, including servers that gzip despite headers."""
    if raw[:2] == bytes((31, 139)) or content_encoding.lower() == "gzip":
        raw = gzip.decompress(raw)
    return json.loads(raw.decode("utf-8"))


def fetch():
    data = urllib.parse.urlencode({"data": QUERY}).encode()
    last_error = None
    # Two attempts per independent provider, with bounded backoff. This avoids
    # hammering a busy public server while still surviving transient 502/504s.
    for endpoint in ENDPOINTS:
        for attempt in range(2):
            request = urllib.request.Request(
                endpoint,
                data=data,
                headers={
                    "User-Agent": "CITY-LIFE-3D map asset builder/1.2 (OSM attribution included; one-time Bayreuth map export)",
                    "Accept": "application/json",
                    "Accept-Encoding": "gzip",
                },
            )
            try:
                with urllib.request.urlopen(request, timeout=150) as response:
                    raw = response.read()
                    payload = decode_response(
                        raw, response.headers.get("Content-Encoding", "")
                    )
                if payload.get("remark"):
                    raise RuntimeError(payload["remark"])
                # Some Overpass mirrors return HTTP 200 with only the boundary
                # relation and silently omit the requested ways when overloaded.
                # Validate the actual map payload here so the next provider is tried.
                build_features(payload)
                return payload
            except Exception as exc:
                last_error = exc
                retry_after = 0
                if isinstance(exc, urllib.error.HTTPError):
                    try:
                        retry_after = int(exc.headers.get("Retry-After", "0"))
                    except (TypeError, ValueError):
                        retry_after = 0
                print(
                    f"Overpass attempt {attempt + 1}/2 failed: {endpoint}: {exc}",
                    file=sys.stderr,
                )
                if attempt == 0:
                    time.sleep(min(max(retry_after, 3), 20))
        print(f"Moving to next independent Overpass provider: {endpoint}", file=sys.stderr)
    raise RuntimeError(f"All Overpass endpoints failed: {last_error}")


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
    city_areas = [
        e for e in elements
        if e.get("type") == "relation"
        and e.get("tags", {}).get("name") == "Bayreuth"
        and e.get("tags", {}).get("boundary") == "administrative"
        and e.get("tags", {}).get("admin_level") in ("6", "8")
    ]
    if not city_areas:
        raise RuntimeError(
            "The Bayreuth administrative area did not resolve on this Overpass "
            "server. No guessed rectangle will be used."
        )

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
    return features, city_areas[0]


def main():
    output = sys.argv[1] if len(sys.argv) > 1 else "bayreuth-osm.geojson"
    payload = fetch()
    features, city_area = build_features(payload)
    roads = sum(f["properties"]["kind"] == "road" for f in features)
    buildings = len(features) - roads
    document = {
        "type": "FeatureCollection",
        "name": "Bayreuth municipal road and building geometry",
        "source": "© OpenStreetMap contributors",
        "license": "Open Database License (ODbL)",
        "area_osm_id": city_area.get("id"),
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
