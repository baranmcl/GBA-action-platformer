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
        self.assertEqual(lvl['enemies'][0], (4, 1, 1, 4, 0))  # patrol from JSON, param2=0

    def test_enemy_default_patrol(self):
        lvl = compile_str(VALID, {})  # no enemies metadata -> default +-2
        self.assertEqual(lvl['enemies'][0], (4, 1, 2, 6, 0))

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

    # --- M3 symbols ---
    def test_lava_is_tile_3(self):
        txt = "#####\n#@~.#\n#####\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['tiles'][lvl['w'] * 1 + 2], 3)  # '~' at (2,1) -> lava

    def test_vine_and_ice_gates(self):
        txt = "######\n#@VI.#\n######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['gates'], [(2, 1, 'Vine'), (3, 1, 'Ice')])

    # --- M4 symbols ---
    def test_water_is_tile_4(self):
        txt = "#####\n#@w.#\n#####\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['tiles'][lvl['w'] * 1 + 2], 4)  # 'w' at (2,1) -> water

    def test_water_gate(self):
        txt = "#####\n#@W.#\n#####\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['gates'], [(2, 1, 'Water')])

    # --- M5 wind tiles ---
    def test_wind_tiles(self):
        txt = "#######\n#@u<>.#\n#######\n"
        lvl = compile_str(txt, {})
        w = lvl['w']
        self.assertEqual(lvl['tiles'][w*1 + 2], 6)  # 'u' updraft
        self.assertEqual(lvl['tiles'][w*1 + 3], 7)  # '<' wind-left
        self.assertEqual(lvl['tiles'][w*1 + 4], 8)  # '>' wind-right

    # --- M6 symbols ---
    def test_spike_is_tile_9(self):
        txt = "#####\n#@s.#\n#####\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['tiles'][lvl['w'] * 1 + 2], 9)  # 's' at (2,1) -> spikes (non-solid hazard)

    def test_cracked_wall_gate(self):
        txt = "#####\n#@K.#\n#####\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['gates'], [(2, 1, 'CrackedWall')])

    def test_shrine_ability(self):
        txt = "#####\n#@F.#\n#####\n"
        lvl = compile_str(txt, {"pickups": [{"ability": "fire"}]})
        self.assertEqual(lvl['pickups'], [(2, 1, 'Fire')])
        lvl2 = compile_str(txt, {})  # default fire
        self.assertEqual(lvl2['pickups'], [(2, 1, 'Fire')])

    def test_block_plate_button_brazier(self):
        txt = "########\n#@B=?*.#\n########\n"
        lvl = compile_str(txt, {
            "plates": [{"target": [6, 1]}],
            "buttons": [{"target": [6, 2]}],
            "braziers": [{"group": 0}],
            "brazier_groups": [{"total": 1, "target": [5, 5]}],
        })
        self.assertEqual(lvl['blocks'], [(2, 1, False)])
        self.assertEqual(lvl['plates'], [(3, 1, 6, 1, False)])
        self.assertEqual(lvl['buttons'], [(4, 1, 6, 2)])
        self.assertEqual(lvl['braziers'], [(5, 1, 0)])
        self.assertEqual(lvl['brazier_groups'], [(1, 5, 5, -1)])  # latch_id defaults to -1

    def test_fire_immune_enemy_flag(self):
        lvl = compile_str(VALID, {"enemies": [{"patrol": [1, 4], "fire_immune": True}]})
        self.assertEqual(lvl['enemies'][0], (4, 1, 1, 4, 1))  # param2 bit0 = fire_immune

    def test_plate_requires_json_target(self):
        txt = "#####\n#@=.#\n#####\n"
        with self.assertRaises(build_level.LevelError):
            compile_str(txt, {})  # plate with no target

    # --- room-to-room symbols ---
    def test_entrance_symbol(self):
        txt = "######\n#@N..#\n######\n"
        lvl = compile_str(txt, {"entrances": [{"id": 1, "facing": -1}]})
        self.assertEqual(lvl['entrances'], [(1, 2, 1, -1)])  # (id, tx, ty, facing)

    def test_entrance_defaults(self):
        txt = "######\n#@N..#\n######\n"
        lvl = compile_str(txt, {})  # no entrance metadata -> id by order, facing +1
        self.assertEqual(lvl['entrances'], [(0, 2, 1, 1)])

    def test_entrance_mixed_metadata(self):
        # Two N's, JSON only for the first; the second falls back to scan-order
        # index (1) + facing +1. Pins the documented primary-key default behavior.
        txt = "######\n#@N.N#\n######\n"  # N at (2,1) and (4,1)
        lvl = compile_str(txt, {"entrances": [{"id": 5, "facing": -1}]})
        self.assertEqual(lvl['entrances'], [(5, 2, 1, -1), (1, 4, 1, 1)])

    def test_room_door_symbol(self):
        txt = "######\n#@.D.#\n######\n"
        lvl = compile_str(txt, {"room_doors": [{"target_room": 2, "target_entrance": 1}]})
        self.assertEqual(lvl['room_doors'], [(3, 1, 2, 1)])  # (tx, ty, target_room, target_entrance)

    def test_room_door_requires_target(self):
        txt = "######\n#@.D.#\n######\n"
        with self.assertRaises(build_level.LevelError):
            compile_str(txt, {})  # room-door with no target metadata

    def test_brazier_group_latch_id(self):
        txt = "########\n#@*....#\n########\n"
        lvl = compile_str(txt, {
            "braziers": [{"group": 0}],
            "brazier_groups": [{"total": 1, "target": [5, 5], "latch_id": 4}],
        })
        self.assertEqual(lvl['brazier_groups'], [(1, 5, 5, 4)])

    def test_brazier_group_latch_default(self):
        txt = "########\n#@*....#\n########\n"
        lvl = compile_str(txt, {
            "braziers": [{"group": 0}],
            "brazier_groups": [{"total": 1, "target": [5, 5]}],
        })
        self.assertEqual(lvl['brazier_groups'], [(1, 5, 5, -1)])  # default not-latched

    def test_emit_header_has_room_fields(self):
        txt = "######\n#@ND.#\n######\n"  # N at (2,1), D at (3,1)
        lvl = compile_str(txt, {"entrances": [{"id": 0, "facing": 1}],
                                "room_doors": [{"target_room": 1, "target_entrance": 0}]})
        hdr = build_level.emit_header(lvl, "TESTRM")
        self.assertIn("TESTRM_ENTRANCES", hdr)
        self.assertIn("TESTRM_ROOM_DOORS", hdr)
        # Pin the exact emitted C++ tuple content so a field-order swap in the
        # entrance/room-door emit f-strings is caught.
        # Entrance: {id, tx, ty, facing} -> {0,2,1,1}
        self.assertIn("{0,2,1,1}", hdr)
        # Room-door: {tx, ty, target_room, target_entrance} -> {3,1,1,0}
        self.assertIn("{3,1,1,0}", hdr)

    # --- M7 symbols ---
    def test_grapple_anchor_tile(self):
        txt = "#####\n#@g.#\n#####\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['tiles'][lvl['w']*1 + 2], 10)  # 'g' at (2,1) -> GrapplePoint=10 (non-solid)

    def test_pullable_block_symbol(self):
        txt = "######\n#@.P.#\n######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['blocks'], [(3, 1, True)])     # 'P' -> pullable block (tx,ty,pullable)

    def test_plain_block_not_pullable(self):
        txt = "######\n#@.B.#\n######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['blocks'], [(3, 1, False)])    # 'B' -> normal push block

    def test_emit_block_pullable_field(self):
        txt = "######\n#@.P.#\n######\n"
        lvl = compile_str(txt, {})
        hdr = build_level.emit_header(lvl, "TESTB")
        self.assertIn("{3,1,true}", hdr)                    # BlockSpawn emits the pullable bool

    # --- Heart container symbol ---
    def test_heart_container_symbol(self):
        txt = "######\n#@.H.#\n######\n"
        lvl = compile_str(txt, {"heart_containers": [{"id": 3}]})
        self.assertEqual(lvl['heart_containers'], [(3, 1, 3)])  # (tx, ty, id)

    def test_heart_container_default_id(self):
        # When no JSON metadata for heart_containers, id defaults to scan-order index
        txt = "######\n#@.H.#\n######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['heart_containers'], [(3, 1, 0)])  # id = 0 (first in order)

    def test_heart_container_multiple_defaults(self):
        # Two H's, no JSON: ids are 0 and 1 by scan order
        txt = "########\n#@.H.H.#\n########\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['heart_containers'], [(3, 1, 0), (5, 1, 1)])

    def test_heart_container_emit_header(self):
        txt = "######\n#@.H.#\n######\n"
        lvl = compile_str(txt, {"heart_containers": [{"id": 3}]})
        hdr = build_level.emit_header(lvl, "TESTHC")
        self.assertIn("TESTHC_HEART_CONTAINERS", hdr)
        self.assertIn("{3,1,3}", hdr)                       # HeartContainerSpawn {tx,ty,id}
        # Array reference and count (1) must appear in the LevelData initialiser
        self.assertIn("TESTHC_HEART_CONTAINERS, 1", hdr)

    def test_heart_container_absent_level_still_compiles(self):
        # Level without any H: heart_containers dummy array + count 0 emitted
        lvl = compile_str(VALID, {"enemies": [{"patrol": [1, 4]}]})
        hdr = build_level.emit_header(lvl, "TESTHC2")
        self.assertIn("TESTHC2_HEART_CONTAINERS", hdr)
        # Count must be 0 in the LevelData initialiser
        self.assertIn("TESTHC2_HEART_CONTAINERS, 0", hdr)
        self.assertEqual(lvl['heart_containers'], [])


    # --- M8 symbols: Task 2.2 heavy (pound-only) flag on plates ---
    def test_heavy_plate_flag_true(self):
        # A plate entry with "heavy": true must produce a plate tuple with heavy=True (5th element)
        txt = "#######\n#@.=..#\n#######\n"
        lvl = compile_str(txt, {"plates": [{"target": [3, 4], "heavy": True}]})
        self.assertEqual(lvl['plates'], [(3, 1, 3, 4, True)])

    def test_heavy_plate_flag_false_by_default(self):
        # A plate entry without "heavy" defaults to False
        txt = "#######\n#@.=..#\n#######\n"
        lvl = compile_str(txt, {"plates": [{"target": [3, 4]}]})
        self.assertEqual(lvl['plates'], [(3, 1, 3, 4, False)])

    def test_heavy_plate_emit_header_true(self):
        # emit_header must include the bool in the PlateSpawn initializer
        txt = "#######\n#@.=..#\n#######\n"
        lvl = compile_str(txt, {"plates": [{"target": [3, 4], "heavy": True}]})
        hdr = build_level.emit_header(lvl, "TESTHP")
        self.assertIn("{3,1,3,4,true}", hdr)

    def test_heavy_plate_emit_header_false(self):
        # Non-heavy plate must emit false
        txt = "#######\n#@.=..#\n#######\n"
        lvl = compile_str(txt, {"plates": [{"target": [3, 4]}]})
        hdr = build_level.emit_header(lvl, "TESTHPF")
        self.assertIn("{3,1,3,4,false}", hdr)

    def test_existing_plate_test_still_green(self):
        # The existing block/plate/button/brazier test data still compiles correctly
        txt = "########\n#@B=?*.#\n########\n"
        lvl = compile_str(txt, {
            "plates": [{"target": [6, 1]}],
            "buttons": [{"target": [6, 2]}],
            "braziers": [{"group": 0}],
            "brazier_groups": [{"total": 1, "target": [5, 5]}],
        })
        # plate still at (3,1) targeting (6,1); heavy defaults False (5th field)
        self.assertEqual(lvl['plates'], [(3, 1, 6, 1, False)])

    # --- M8 symbols: Task 2.3 boulder 'O' + loose-platform ':' ---
    def test_boulder_symbol(self):
        # 'O' compiles to a boulder at the right tile position
        txt = "#######\n#@.O..#\n#######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['boulders'], [(3, 1)])

    def test_boulder_tile_is_empty(self):
        # 'O' is a content symbol: collision tile under it must be 0
        txt = "#######\n#@.O..#\n#######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['tiles'][lvl['w'] * 1 + 3], 0)

    def test_boulder_emit_header(self):
        # emit_header includes BOULDERS array and wires boulder_count into LevelData
        txt = "#######\n#@.O..#\n#######\n"
        lvl = compile_str(txt, {})
        hdr = build_level.emit_header(lvl, "TESTBO")
        self.assertIn("TESTBO_BOULDERS", hdr)
        self.assertIn("{3,1}", hdr)  # BoulderSpawn {tx, ty}
        self.assertIn("TESTBO_BOULDERS, 1", hdr)  # count in LevelData

    def test_boulder_absent_still_compiles(self):
        # Level without 'O': null dummy array + count 0 in LevelData
        lvl = compile_str(VALID, {"enemies": [{"patrol": [1, 4]}]})
        hdr = build_level.emit_header(lvl, "TESTBO2")
        self.assertIn("TESTBO2_BOULDERS", hdr)
        self.assertIn("TESTBO2_BOULDERS, 0", hdr)
        self.assertEqual(lvl['boulders'], [])

    def test_loose_platform_symbol_with_json_len(self):
        # ':' with JSON loose_platforms entry emits LoosePlatformSpawn(tx, ty, len)
        txt = "#######\n#@.:.:#\n#######\n"
        lvl = compile_str(txt, {"loose_platforms": [{"len": 3}, {"len": 2}]})
        self.assertEqual(lvl['loose_platforms'], [(3, 1, 3), (5, 1, 2)])

    def test_loose_platform_default_len_1(self):
        # ':' without JSON entry defaults to len=1
        txt = "#######\n#@.:..#\n#######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['loose_platforms'], [(3, 1, 1)])

    def test_loose_platform_tile_is_empty(self):
        # ':' is a content symbol: collision tile must be 0
        txt = "#######\n#@.:..#\n#######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['tiles'][lvl['w'] * 1 + 3], 0)

    def test_loose_platform_emit_header(self):
        # emit_header includes LOOSE_PLATFORMS array and wires count into LevelData
        txt = "#######\n#@.:..#\n#######\n"
        lvl = compile_str(txt, {"loose_platforms": [{"len": 4}]})
        hdr = build_level.emit_header(lvl, "TESTLP")
        self.assertIn("TESTLP_LOOSE_PLATFORMS", hdr)
        self.assertIn("{3,1,4}", hdr)  # LoosePlatformSpawn {tx, ty, len}
        self.assertIn("TESTLP_LOOSE_PLATFORMS, 1", hdr)

    def test_loose_platform_absent_still_compiles(self):
        # Level without ':': dummy array + count 0
        lvl = compile_str(VALID, {"enemies": [{"patrol": [1, 4]}]})
        hdr = build_level.emit_header(lvl, "TESTLP2")
        self.assertIn("TESTLP2_LOOSE_PLATFORMS", hdr)
        self.assertIn("TESTLP2_LOOSE_PLATFORMS, 0", hdr)
        self.assertEqual(lvl['loose_platforms'], [])

    # --- M8 symbols: Task 2.1 CrackedFloor gate 'k' ---
    def test_cracked_floor_gate(self):
        # 'k' compiles to a gate with GateType::CrackedFloor
        txt = "#####\n#@k.#\n#####\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['gates'], [(2, 1, 'CrackedFloor')])

    def test_cracked_floor_gate_tile_is_empty(self):
        # 'k' is a content symbol: collision tile under it must be 0 (empty)
        txt = "#####\n#@k.#\n#####\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['tiles'][lvl['w'] * 1 + 2], 0)

    def test_level_without_k_still_compiles(self):
        # A level with no 'k' still compiles with an empty gates list
        lvl = compile_str(VALID, {"enemies": [{"patrol": [1, 4]}]})
        cracked_floor_gates = [g for g in lvl['gates'] if g[2] == 'CrackedFloor']
        self.assertEqual(cracked_floor_gates, [])


if __name__ == '__main__':
    unittest.main()
