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


def query_for_areas():
    """Fetch the four target areas in one Overpass request.

    A single union query avoids waiting through four independent server
    queues while still keeping the requested geographic boxes precise.
    """
    clauses = []
    for south, west, north, east in BBOXES:
        box = f"{south},{west},{north},{east}"
        clauses.extend([
            f'way["highway"~"^(motorway|trunk|primary|secondary|tertiary|unclassified|residential|living_street|service|pedestrian|track|cycleway|footway|path)$"]({box});',
            f'way["building"]({box});',
        ])
    return "[out:json][timeout:45];\\n(\\n" + "\\n".join(clauses) + "\\n);\\nout body geom;\\n"


def decode_response(raw, content_encoding=""):
    """Decode JSON from Overpass, including servers that gzip despite headers."""
    if raw[:2] == bytes((31, 139)) or content_encoding.lower() == "gzip":
        raw = gzip.decompress(raw)
    return json.loads(raw.decode("utf-8"))


def fetch():
    """Fetch all four target areas together, with a short provider fallback."""
    data = urllib.parse.urlencode({"data": query_for_areas()}).encode()
    last_error = None
    for endpoint in ENDPOINTS:
        request = urllib.request.Request(
            endpoint,
            data=data,
            headers={
                "User-Agent": "CITY-LIFE-3D map asset builder/1.4 (OSM attribution included; one-time Bayreuth map export)",
                "Accept": "application/json",
                "Accept-Encoding": "gzip",
            },
        )
        try:
            with urllib.request.urlopen(request, timeout=22) as response:
                raw = response.read()
                payload = decode_response(raw, response.headers.get("Content-Encoding", ""))
            if payload.get("remark"):
                raise RuntimeError(payload["remark"])
            features, _ = build_features(payload)
            roads = sum(f["properties"]["kind"] == "road" for f in features)
            buildings = len(features) - roads
            print(f"Fetched all four Bayreuth areas from {endpoint}: {roads} roads, {buildings} buildings")
            return payload
        except Exception as exc:
            last_error = exc
            print(f"Overpass provider failed: {endpoint}: {exc}", file=sys.stderr)
    raise RuntimeError(f"Could not fetch the four Bayreuth areas: {last_error}")

