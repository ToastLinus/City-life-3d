# Bayreuth map data preparation

The pipeline has two offline stages. Neither contacts the network while the
3DS game is running.

## 1. Fetch OpenStreetMap geometry

```sh
python3 tools/fetch_bayreuth_map.py bayreuth-osm.geojson
```

This resolves the Bayreuth administrative area and exports mapped road/path
centerlines and closed building-way footprints. It refuses guessed boundaries
and skips open building ways rather than inventing polygons.

## 2. Convert geometry to 3DS tile data

```sh
python3 tools/build_bayreuth_tiles.py bayreuth-osm.geojson bayreuth-tiles.bin
python3 -m unittest discover -s tests -v
```

The binary format uses the `BT3D` magic and version 1. It stores a local
metric origin, 256 m spatial tiles, road/building type codes, and signed
quarter-metre vertex coordinates. A feature is copied into every tile touched
by its bounding box, so a road crossing a tile boundary is still present in
both neighboring chunks. The converter reports tile and feature counts.

The tile binary is an intermediate runtime asset format; it is **not yet
loaded or rendered by the current 3DS game**. The next stage is the C runtime
reader, chunk visibility/loading, and conversion of road/building geometry
into Citro3D draw batches. Multipolygon relations, terrain/elevation,
land-use polygons, facade detail and custom landmark models also remain.

## Licensing

OpenStreetMap data is © OpenStreetMap contributors and is available under the
Open Database License (ODbL). Any distributed game build using derived OSM
data must include appropriate attribution and comply with the ODbL:
https://www.openstreetmap.org/copyright
