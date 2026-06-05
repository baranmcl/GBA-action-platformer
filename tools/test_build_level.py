import os
import tempfile
import unittest

import build_level


def compile_str(txt, meta=None):
    d = tempfile.mkdtemp()
    txt_path = os.path.join(d, 'lvl.txt')
    json_path = os.path.join(d, 'lvl.json')
    with open(txt_path, 'w', encoding='utf-8') as f:
        f.write(txt)
    if meta is not None:
        import json
        with open(json_path, 'w', encoding='utf-8') as f:
            json.dump(meta, f)
    return build_level.compile_level(txt_path, json_path)


VALID = (
    "######\n"
    "#@..o#\n"
    "#..C.#\n"
    "#...E#\n"
    "######\n"
)


class TestBuildLevel(unittest.TestCase):
    def test_valid_compiles(self):
        lvl = compile_str(VALID, {"enemies": [{"patrol": [1, 4]}]})
        self.assertEqual(lvl['w'], 6)
        self.assertEqual(lvl['h'], 5)
        self.assertEqual(lvl['spawn'], (1, 1))
        self.assertEqual(lvl['cage'], (3, 2))
        self.assertEqual(lvl['exit'], (4, 3))
        self.assertEqual(len(lvl['enemies']), 1)
        self.assertEqual(lvl['enemies'][0], (4, 1, 1, 4))  # patrol from JSON

    def test_enemy_default_patrol(self):
        lvl = compile_str(VALID, {})  # no enemies metadata -> default +-2
        self.assertEqual(lvl['enemies'][0], (4, 1, 2, 6))

    def test_missing_spawn_errors(self):
        txt = "####\n#..#\n####\n"
        with self.assertRaises(build_level.LevelError):
            compile_str(txt)

    def test_two_spawns_error(self):
        txt = "#####\n#@@.#\n#####\n"
        with self.assertRaises(build_level.LevelError):
            compile_str(txt)

    def test_non_rectangular_errors(self):
        txt = "#####\n#@.#\n#####\n"
        with self.assertRaises(build_level.LevelError):
            compile_str(txt)

    def test_non_solid_border_errors(self):
        txt = "#####\n.@..#\n#####\n"  # left border not solid
        with self.assertRaises(build_level.LevelError):
            compile_str(txt)

    def test_unknown_symbol_errors(self):
        txt = "#####\n#@Z.#\n#####\n"
        with self.assertRaises(build_level.LevelError):
            compile_str(txt)

    def test_gate_requires_json_entry(self):
        txt = "#####\n#@G.#\n#####\n"
        with self.assertRaises(build_level.LevelError):
            compile_str(txt, {})  # no gates metadata for the G

    def test_door_dungeon_number(self):
        txt = "######\n#@..2#\n######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['doors'], [(4, 1, 2)])

    def test_emit_header_has_data_symbol(self):
        lvl = compile_str(VALID, {"enemies": [{"patrol": [1, 4]}]})
        hdr = build_level.emit_header(lvl, "TESTLVL")
        self.assertIn("TESTLVL_DATA", hdr)
        self.assertIn("TESTLVL_TILES", hdr)
        self.assertIn("logic::LevelData", hdr)


if __name__ == '__main__':
    unittest.main()
