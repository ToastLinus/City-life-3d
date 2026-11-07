# Bayreuth map data preparation

`fetch_bayreuth_map.py` downloads mapped road centerlines and building
footprints from the OpenStreetMap administrative area named Bayreuth. It
produces GeoJSON with coordinates and basic tags, for offline conversion into
small Nintendo 3DS map tiles.

Run locally with Python 3:

```sh
python3 tools/fetch_bayreuth_map.py bayreuth-osm.geojson
```

The tool resolves the municipal area on the Overpass server rather than using
a guessed rectangular boundary. It tries two public Overpass endpoints and
fails explicitly if neither returns data. It does not use a bounding rectangle
as a silent fallback, because that could include neighboring municipalities.

Current scope:
- road and path centerlines (where mapped);
- building footprints (where mapped);
- names, road/building classes and selected building attributes.

Not yet included in the game:
- this GeoJSON is an intermediate source asset, not a Nintendo 3DS runtime
  tile and not a playable map;
- multipolygon building relations, terrain/elevation, land-use polygons,
  detailed facade geometry and custom landmark models still need separate
  processing;
- data coverage and accuracy depend on OpenStreetMap mapping.

OpenStreetMap data is © OpenStreetMap contributors and is available under the
Open Database License (ODbL). Any distributed game build using derived OSM
data must include appropriate attribution and comply with the ODbL:
https://www.openstreetmap.org/copyright
