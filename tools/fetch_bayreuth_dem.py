#!/usr/bin/env python3
"""Build compact 32 m Bayreuth terrain tiles from the public Copernicus GLO-30 COG.

The game consumes a small 9x9 height grid per 256 m map tile. Source data are
downloaded only during the build; no network access is needed on the 3DS.
"""
import json
import math
import os
import struct
import sys
import urllib.request

TILE_SIZE = 256.0
SAMPLES = 9
MAGIC = b"BTH1"
HEADER = struct.Struct("<4sHHfI")
TILE_HEADER = struct.Struct("<ii")
HEIGHT = struct.Struct("<81f")
DEM_URL = (
    "https://copernicus-dem-30m.s3.amazonaws.com/"
    "Copernicus_DSM_COG_10_N49_00_E011_00_DEM/"
    "Copernicus_DSM_COG_10_N49_00_E011_00_DEM.tif"
)


def project(lon, lat, origin_lon, origin_lat):
    radius = 6378137.0
    x = math.radians(lon - origin_lon) * radius * math.cos(math.radians(origin_lat))
    z = math.radians(lat - origin_lat) * radius
    return x, z


def main():
    if len(sys.argv) < 3:
        raise SystemExit("Usage: fetch_bayreuth_dem.py bayreuth-osm.geojson output.bin")
    import rasterio

    with open(sys.argv[1], encoding="utf-8") as handle:
        document = json.load(handle)

    points = []
    tile_keys = set()
    for feature in document.get("features", []):
        geometry = feature.get("geometry") or {}
        rings = []
        if geometry.get("type") == "LineString":
            rings = [geometry.get("coordinates", [])]
        elif geometry.get("type") == "Polygon":
            rings = geometry.get("coordinates", [])[:1]
        for ring in rings:
            for lon, lat, *_ in ring:
                lon, lat = float(lon), float(lat)
                if math.isfinite(lon) and math.isfinite(lat):
                    points.append((lon, lat))
    if not points:
        raise RuntimeError("No OSM coordinates available to align the elevation grid")

    origin_lon = sum(p[0] for p in points) / len(points)
    origin_lat = sum(p[1] for p in points) / len(points)
    for lon, lat in points:
        x, z = project(lon, lat, origin_lon, origin_lat)
        tile_keys.add((math.floor(x / TILE_SIZE), math.floor(z / TILE_SIZE)))

    dem_path = os.path.join(os.path.dirname(sys.argv[2]) or ".", "copernicus-bayreuth-dem.tif")
    if not os.path.exists(dem_path) or os.path.getsize(dem_path) < 1024 * 1024:
        print("Downloading Copernicus DEM GLO-30 tile for Bayreuth...")
        request = urllib.request.Request(
            DEM_URL, headers={"User-Agent": "CITY-LIFE-3D terrain asset builder/1.0"}
        )
        with urllib.request.urlopen(request, timeout=180) as response, open(dem_path, "wb") as output:
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                output.write(chunk)
        if os.path.getsize(dem_path) < 1024 * 1024:
            raise RuntimeError("Downloaded Copernicus DEM tile is unexpectedly small")

    with rasterio.open(dem_path) as dem:
        if dem.crs is None or dem.count < 1:
            raise RuntimeError("Copernicus DEM GeoTIFF has no usable elevation band")
        output = bytearray(HEADER.pack(MAGIC, 1, SAMPLES, TILE_SIZE, len(tile_keys)))
        for tx, tz in sorted(tile_keys):
            locations = []
            for row in range(SAMPLES):
                for col in range(SAMPLES):
                    x = tx * TILE_SIZE + col * (TILE_SIZE / (SAMPLES - 1))
                    z = tz * TILE_SIZE + row * (TILE_SIZE / (SAMPLES - 1))
                    lon = origin_lon + math.degrees(x / (6378137.0 * math.cos(math.radians(origin_lat))))
                    lat = origin_lat + math.degrees(z / 6378137.0)
                    locations.append((lon, lat))
            values = []
            for sample in dem.sample(locations, indexes=1, masked=True):
                value = float(sample[0])
                if not math.isfinite(value) or value < -500.0 or value > 9000.0:
                    raise RuntimeError("Copernicus DEM returned an invalid elevation sample")
                values.append(value)
            if len(values) != SAMPLES * SAMPLES:
                raise RuntimeError("Incomplete elevation tile")
            output.extend(TILE_HEADER.pack(tx, tz))
            output.extend(HEIGHT.pack(*values))

    with open(sys.argv[2], "wb") as handle:
        handle.write(output)
    print(
        f"Wrote {sys.argv[2]}: {len(tile_keys)} terrain tiles, "
        f"{len(tile_keys) * SAMPLES * SAMPLES} elevation samples"
    )
    print("Elevation source: Copernicus DEM GLO-30 (© DLR e.V. / © Airbus DS)")


if __name__ == "__main__":
    main()
