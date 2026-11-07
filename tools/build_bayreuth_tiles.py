#!/usr/bin/env python3
"""Convert Bayreuth GeoJSON into compact, spatially indexed 3DS tile data.

Format v1 is little-endian:
  Header: magic[4], version[u16], flags[u16], origin_lat[f64],
          origin_lon[f64], tile_size_m[f32], tile_count[u32]
  Tile:   tile_x[i32], tile_z[i32], feature_count[u32]
  Feature: kind[u8] (1 road, 2 building), reserved[u8],
           point_count[u16], then point_count pairs (x_quarter_m[i16],
           z_quarter_m[i16]).
Coordinates are relative to the tile's south-west origin. Features are copied
to every tile touched by their bounding box so long roads remain available
when adjacent chunks are loaded. The signed quarter-metre coordinates allow
a feature to extend beyond its owning tile.
"""
import json
import math
import struct
import sys
from collections import defaultdict

MAGIC = b"BT3D"
VERSION = 1
DEFAULT_TILE_SIZE = 256.0
HEADER = struct.Struct("<4sHHddfI")
TILE_HEADER = struct.Struct("<iiI")
FEATURE_HEADER = struct.Struct("<BBH")
POINT = struct.Struct("<hh")


def project(lon, lat, origin_lon, origin_lat):
    """Local equirectangular projection in metres, adequate for city tiles."""
    radius = 6378137.0
    x = math.radians(lon - origin_lon) * radius * math.cos(math.radians(origin_lat))
    z = math.radians(lat - origin_lat) * radius
    return x, z


def convert(document, tile_size=DEFAULT_TILE_SIZE):
    if tile_size <= 0 or not math.isfinite(tile_size):
        raise ValueError("tile size must be a positive finite number")
    features = document.get("features", [])
    all_points = []
    parsed = []
    for feature in features:
        geometry = feature.get("geometry") or {}
        kind = feature.get("properties", {}).get("kind")
        if kind not in ("road", "building"):
            continue
        if geometry.get("type") == "LineString" and kind == "road":
            rings = [geometry.get("coordinates", [])]
        elif geometry.get("type") == "Polygon" and kind == "building":
            rings = geometry.get("coordinates", [])[:1]
        else:
            continue
        for ring in rings:
            points = []
            for pair in ring:
                if len(pair) < 2:
                    continue
                lon, lat = float(pair[0]), float(pair[1])
                if not (math.isfinite(lon) and math.isfinite(lat)):
                    raise ValueError("non-finite coordinate in GeoJSON")
                points.append((lon, lat))
            if len(points) >= 2:
                parsed.append((1 if kind == "road" else 2, points))
                all_points.extend(points)

    if not all_points:
        raise ValueError("GeoJSON contains no supported road/building geometry")

    origin_lon = sum(p[0] for p in all_points) / len(all_points)
    origin_lat = sum(p[1] for p in all_points) / len(all_points)
    tiles = defaultdict(list)
    for kind, geographic_points in parsed:
        local = [project(lon, lat, origin_lon, origin_lat)
                 for lon, lat in geographic_points]
        min_x = min(p[0] for p in local)
        max_x = max(p[0] for p in local)
        min_z = min(p[1] for p in local)
        max_z = max(p[1] for p in local)
        tx0, tx1 = math.floor(min_x / tile_size), math.floor(max_x / tile_size)
        tz0, tz1 = math.floor(min_z / tile_size), math.floor(max_z / tile_size)
        for tx in range(tx0, tx1 + 1):
            for tz in range(tz0, tz1 + 1):
                encoded = []
                for x, z in local:
                    qx = round((x - tx * tile_size) * 4.0)
                    qz = round((z - tz * tile_size) * 4.0)
                    if not (-32768 <= qx <= 32767 and -32768 <= qz <= 32767):
                        raise ValueError("feature exceeds signed tile coordinate range")
                    encoded.append((qx, qz))
                tiles[(tx, tz)].append((kind, encoded))

    output = bytearray(HEADER.pack(
        MAGIC, VERSION, 0, origin_lat, origin_lon, tile_size, len(tiles)
    ))
    for (tx, tz), entries in sorted(tiles.items()):
        output.extend(TILE_HEADER.pack(tx, tz, len(entries)))
        for kind, points in entries:
            if len(points) > 65535:
                raise ValueError("feature has too many vertices for format v1")
            output.extend(FEATURE_HEADER.pack(kind, 0, len(points)))
            for x, z in points:
                output.extend(POINT.pack(x, z))
    return bytes(output), {
        "tile_count": len(tiles),
        "feature_count": sum(len(v) for v in tiles.values()),
        "origin_lat": origin_lat,
        "origin_lon": origin_lon,
        "tile_size_m": tile_size,
        "version": VERSION,
    }


def main():
    if len(sys.argv) < 3:
        raise SystemExit(
            "Usage: python3 tools/build_bayreuth_tiles.py input.geojson output.bin [tile_size_m]"
        )
    tile_size = float(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_TILE_SIZE
    with open(sys.argv[1], encoding="utf-8") as source:
        document = json.load(source)
    data, info = convert(document, tile_size)
    with open(sys.argv[2], "wb") as target:
        target.write(data)
    print("Wrote {} bytes: {} tiles, {} tile-feature records".format(
        len(data), info["tile_count"], info["feature_count"]
    ))
    print("Origin: {:.7f}, {:.7f}; tile size: {:.1f} m".format(
        info["origin_lat"], info["origin_lon"], info["tile_size_m"]
    ))


if __name__ == "__main__":
    main()
