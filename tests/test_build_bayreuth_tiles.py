import importlib.util
import pathlib
import struct
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "tools" / "build_bayreuth_tiles.py"
SPEC = importlib.util.spec_from_file_location("build_bayreuth_tiles", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class TileBuilderTests(unittest.TestCase):
    def setUp(self):
        self.document = {"features": [
            {"properties": {"kind": "road"},
             "geometry": {"type": "LineString",
                          "coordinates": [[11.57, 49.94], [11.575, 49.94]]}},
            {"properties": {"kind": "building"},
             "geometry": {"type": "Polygon", "coordinates": [[
                 [11.571, 49.941], [11.572, 49.941],
                 [11.572, 49.942], [11.571, 49.942],
                 [11.571, 49.941]
             ]]}},
        ]}

    def test_writes_versioned_binary_header_and_tiles(self):
        data, info = MODULE.convert(self.document, 256.0)
        magic, version, flags, lat, lon, size, count = MODULE.HEADER.unpack_from(data)
        self.assertEqual(magic, b"BT3D")
        self.assertEqual(version, 1)
        self.assertEqual(flags, 0)
        self.assertGreater(count, 0)
        self.assertEqual(info["tile_count"], count)
        self.assertGreater(info["feature_count"], 0)
        self.assertAlmostEqual(size, 256.0)

    def test_long_road_is_present_in_each_touched_tile(self):
        doc = {"features": [{
            "properties": {"kind": "road"},
            "geometry": {"type": "LineString",
                         "coordinates": [[11.56, 49.94], [11.59, 49.94]]}
        }]}
        _, info = MODULE.convert(doc, 256.0)
        self.assertGreater(info["tile_count"], 1)

    def test_rejects_empty_supported_geometry(self):
        with self.assertRaisesRegex(ValueError, "no supported"):
            MODULE.convert({"features": []})

    def test_rejects_invalid_tile_size(self):
        with self.assertRaisesRegex(ValueError, "positive"):
            MODULE.convert(self.document, 0)


if __name__ == "__main__":
    unittest.main()
