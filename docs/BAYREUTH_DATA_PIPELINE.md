# Bayreuth map data preparation

`fetch_bayreuth_map.py` downloads mapped road centerlines and closed building
footprints from the OpenStreetMap administrative area named Bayreuth. It
produces GeoJSON for offline conversion into small Nintendo 3DS map tiles.

Run locally with Python 3:

```sh
python3 tools/fetch_bayreuth_map.py bayreuth-osm.geojson
python3 -m unittest discover -s tests -v
```

The query resolves the municipal area on the Overpass server and verifies
that the expected Bayreuth administrative area was returned. It does not use
a guessed rectangular boundary as a fallback. If the area cannot be resolved,
the export stops instead of silently producing a misleading map.

The converter currently exports:
- mapped road/path centerlines;
- closed building-way footprints;
- selected names, road/building classes and building attributes;
- OSM source timestamp and attribution metadata.

It deliberately skips open building ways rather than inventing closed
footprints. Multipolygon building relations, terrain/elevation, land-use
polygons, detailed facade geometry and custom landmark models still need
separate processing.

**Important project status:** the GeoJSON is an intermediate source asset.
It is not yet loaded by the 3DS game and does not make the current small test
scene into Bayreuth. The next implementation stage is projection, spatial
tiling, compact runtime data and chunk loading.

OpenStreetMap data is © OpenStreetMap contributors and is available under the
Open Database License (ODbL). Any distributed game build using derived OSM
data must include appropriate attribution and comply with the ODbL:
https://www.openstreetmap.org/copyright
