import gzip
import importlib.util
import pathlib
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "tools" / "fetch_bayreuth_map.py"
SPEC = importlib.util.spec_from_file_location("fetch_bayreuth_map", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def area():
    return {
        "type": "relation", "id": 3600000001,
        "tags": {"name": "Bayreuth", "boundary": "administrative", "admin_level": "8"},
    }


def way(osm_id, tags, coords):
    return {
        "type": "way", "id": osm_id, "tags": tags,
        "geometry": [{"lon": lon, "lat": lat} for lon, lat in coords],
    }


class ResponseDecodingTests(unittest.TestCase):
    def test_decodes_plain_json(self):
        self.assertEqual(MODULE.decode_response(b'{"elements": []}'), {"elements": []})

    def test_decodes_gzip_json_even_without_header(self):
        compressed = gzip.compress(b'{"elements": []}')
        self.assertEqual(MODULE.decode_response(compressed), {"elements": []})

    def test_decodes_gzip_json_with_header(self):
        compressed = gzip.compress(b'{"elements": []}')
        self.assertEqual(MODULE.decode_response(compressed, "gzip"), {"elements": []})


class GeometryConversionTests(unittest.TestCase):
    def test_exports_roads_and_closed_building_footprints(self):
        payload = {"elements": [
            area(),
            way(10, {"highway": "residential", "name": "Teststraße"},
                [(11.5, 49.9), (11.501, 49.901)]),
            way(11, {"building": "yes"},
                [(11.5, 49.9), (11.51, 49.9), (11.51, 49.91),
                 (11.5, 49.91), (11.5, 49.9)]),
        ]}
        features, resolved_area = MODULE.build_features(payload)
        self.assertEqual(resolved_area["name"], "Rotmain-Center, Hofgarten, Lidl Meranierring, Unterpreuschwitz")
        self.assertEqual([f["properties"]["kind"] for f in features],
                         ["road", "building"])
        self.assertEqual(features[1]["geometry"]["type"], "Polygon")

    def test_does_not_invent_polygon_from_open_building_way(self):
        payload = {"elements": [
            area(),
            way(12, {"building": "yes"},
                [(11.5, 49.9), (11.51, 49.9), (11.51, 49.91)]),
        ]}
        with self.assertRaisesRegex(RuntimeError, "no valid road/building"):
            MODULE.build_features(payload)

    def test_requires_real_bayreuth_area(self):
        payload = {"elements": [
            way(13, {"highway": "residential"}, [(11.5, 49.9), (11.51, 49.9)])
        ]}
        with self.assertRaisesRegex(RuntimeError, "did not resolve"):
            MODULE.build_features(payload)

    def test_rejects_invalid_coordinates(self):
        self.assertIsNone(MODULE.valid_point({"lon": 999, "lat": 49.9}))
        self.assertIsNone(MODULE.valid_point({"lon": float("nan"), "lat": 49.9}))


if __name__ == "__main__":
    unittest.main()
